// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-engine: "Pending list and background scheduling" with the worker
// thread (design D8): interrupted checks, edits during a check, children
// before parents, busy/idle, and a stress test racing random edits against
// the worker and comparing with `drc catchup` on the same edit sequence.
// The stress test is the one the linux-tsan CI leg runs under
// ThreadSanitizer ([tsan]).

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <random>
#include <thread>

#include "engine_fixture.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::ErrorTile;
using drc::test::Session;

namespace {

constexpr int kTimeoutMs = 20000;

/// Records the observer events (published on the test thread).
struct EventLog final : drc::DrcObserver {
  std::vector<drc::CheckerState> states;
  std::vector<const db::CellDef*> squares;
  void on_state(drc::CheckerState st) override { states.push_back(st); }
  void on_square_committed(const db::CellDef& cell, const db::Rect&) override { squares.push_back(&cell); }
};

/// One edit applied as a command would: under the exclusive database lock.
void edit(Session& s, db::CellDef& def, std::string_view type, const db::Rect& r, bool erase = false) {
  const db::DatabaseLock::Exclusive guard(s.ed.database_lock);
  if (erase) {
    REQUIRE(db::erase_type(def, *s.tech, r, s.type(type)).ok());
  } else {
    REQUIRE(db::paint_type(def, *s.tech, r, s.type(type)).ok());
  }
  db::recompute_bbox(def);
  drc::check_this(s.ed, def, r);
}

/// The error (or check) tiles of `def`, read as a reader of the database
/// must while the worker runs: under the lock (planes cache a search hint).
std::vector<ErrorTile> errors_locked(Session& s, const db::CellDef& def) {
  const db::DatabaseLock::Exclusive guard(s.ed.database_lock);
  return drc::test::errors(def);
}
std::vector<ErrorTile> checks_locked(Session& s, const db::CellDef& def) {
  const db::DatabaseLock::Exclusive guard(s.ed.database_lock);
  return drc::test::checks(def);
}

// --- a kernel that blocks inside the first square until cancelled ------------

struct Gate {
  std::mutex mutex;
  std::condition_variable cv;
  bool entered = false;
  bool cancelled = false;
  bool armed = true;

  static Gate& get() {
    static Gate gate;
    return gate;
  }
};

drc::CheckOutcome gated_kernel(const drc::CheckInput& in, const db::Rect& check, const db::Rect& clip,
                               drc::ErrorSink& sink) {
  Gate& g = Gate::get();
  std::unique_lock lock(g.mutex);
  if (g.armed) {
    g.armed = false;
    g.entered = true;
    g.cv.notify_all();
    while (!in.cancel.requested()) {
      lock.unlock();
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      lock.lock();
    }
    g.cancelled = true;
    return drc::CheckOutcome::cancelled;
  }
  lock.unlock();
  return drc::test::recording_kernel(in, check, clip, sink);
}

}  // namespace

TEST_CASE("Interrupted check leaves no partial result", "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  db::CellDef& c = s.cell("c");
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  s.paint(c, "poly", db::Rect{0, 0, 4, 4});
  REQUIRE(c.plane(tech::kPlaneDrcError).paint(db::Rect{1, 1, 2, 2}, drc::error_paint_row(tech::kErrorS)).ok());
  drc::check_this(s.ed, c, db::Rect{0, 0, 4, 4});
  const auto checks_before = drc::test::checks(c);
  const auto errors_before = drc::test::errors(c);

  SECTION("by the worker's cancel flag") {
    const std::atomic<bool> cancel{true};
    CHECK_FALSE(drc::process_square(s.ed, drc::CancelToken{&cancel, nullptr}).has_value());
  }
  SECTION("by a command waiting for the database lock") {
    std::atomic<bool> holding{false};
    std::atomic<bool> release{false};
    std::thread reader([&] {
      const db::DatabaseLock::Shared shared(s.ed.database_lock);
      holding = true;
      while (!release) {
        std::this_thread::yield();
      }
    });
    while (!holding) {
      std::this_thread::yield();
    }
    std::thread writer([&] { const db::DatabaseLock::Exclusive exclusive(s.ed.database_lock); });
    while (!s.ed.database_lock.writer_waiting()) {
      std::this_thread::yield();
    }
    CHECK_FALSE(drc::process_square(s.ed, drc::CancelToken{nullptr, &s.ed.database_lock}).has_value());
    release = true;
    reader.join();
    writer.join();
  }
  CHECK(drc::test::checks(c) == checks_before);
  CHECK(drc::test::errors(c) == errors_before);
  // The square is checked again later.
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(drc::test::checks(c).empty());
  CHECK(drc::test::errors(c) == std::vector<ErrorTile>{{{0, 0, 4, 4}, tech::kErrorP}});
}

TEST_CASE("Edit during a worker check discards the square",
          "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  db::CellDef& c = s.cell("c");
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  EventLog log;
  s.ed.drc.events.add(log);
  edit(s, c, "poly", db::Rect{0, 0, 4, 4});

  SECTION("an edit while the square is being checked cancels it") {
    Gate& g = Gate::get();
    {
      const std::lock_guard lock(g.mutex);
      g.entered = false;
      g.cancelled = false;
      g.armed = true;
    }
    const drc::ScopedKernel kernel(&gated_kernel);
    drc::BackgroundChecker& worker = *s.ed.drc.worker;
    worker.start(s.ed);
    worker.wake();
    {
      std::unique_lock lock(g.mutex);
      REQUIRE(g.cv.wait_for(lock, std::chrono::milliseconds(kTimeoutMs), [&] { return g.entered; }));
    }
    edit(s, c, "poly", db::Rect{10, 0, 14, 4});  // waits for the worker to drop its square
    {
      const std::lock_guard lock(g.mutex);
      CHECK(g.cancelled);
    }
    CHECK(errors_locked(s, c).empty());  // nothing of the dropped square was applied
    worker.wake();
    REQUIRE(worker.run_until_idle(s.ed, kTimeoutMs));
    worker.stop();
    CHECK(log.squares.size() == 1);  // only the square checked after the edit
  }
  SECTION("an edit after the square finished discards its result") {
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    drc::BackgroundChecker& worker = *s.ed.drc.worker;
    worker.set_ready_callback([&] {
      const std::lock_guard lock(m);
      ready = true;
      cv.notify_all();
    });
    worker.start(s.ed);
    worker.wake();
    {
      std::unique_lock lock(m);
      REQUIRE(cv.wait_for(lock, std::chrono::milliseconds(kTimeoutMs), [&] { return ready; }));
    }
    edit(s, c, "poly", db::Rect{10, 0, 14, 4});
    CHECK(worker.commit_ready(s.ed) == 0);  // the generation changed: dropped
    CHECK(errors_locked(s, c).empty());
    CHECK_FALSE(checks_locked(s, c).empty());  // its check tiles stay for a retry
    worker.wake();
    REQUIRE(worker.run_until_idle(s.ed, kTimeoutMs));
    worker.stop();
  }
  CHECK(drc::test::checks(c).empty());
  CHECK(drc::test::errors(c) == std::vector<ErrorTile>{{{0, 0, 4, 4}, tech::kErrorP}, {{10, 0, 14, 4}, tech::kErrorP}});
  s.ed.drc.events.remove(log);
}

TEST_CASE("Children before parents with the worker", "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  s.set_rules(s.rules(1, 16).build());
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& top = s.cell("top");
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  EventLog log;
  s.ed.drc.events.add(log);
  s.paint(leaf, "poly", db::Rect{0, 0, 40, 4});
  s.place(top, leaf, db::translation(50, 50));
  drc::BackgroundChecker& worker = *s.ed.drc.worker;
  worker.start(s.ed);
  edit(s, leaf, "poly", db::Rect{0, 0, 40, 4});
  worker.wake();
  REQUIRE(worker.run_until_idle(s.ed, kTimeoutMs));
  worker.stop();
  REQUIRE_FALSE(log.squares.empty());
  const auto first_top = std::find(log.squares.begin(), log.squares.end(), &top);
  REQUIRE(first_top != log.squares.end());
  CHECK(std::find(first_top, log.squares.end(), &leaf) == log.squares.end());
  CHECK(drc::test::errors(top) == std::vector<ErrorTile>{{{50, 50, 90, 54}, tech::kErrorP}});
  s.ed.drc.events.remove(log);
}

TEST_CASE("Busy and idle transitions", "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  db::CellDef& c = s.cell("c");
  const drc::test::Recorder rec;
  EventLog log;
  s.ed.drc.events.add(log);
  drc::BackgroundChecker& worker = *s.ed.drc.worker;
  worker.start(s.ed);
  worker.wake();  // empty list: no transition
  REQUIRE(worker.run_until_idle(s.ed, kTimeoutMs));
  CHECK(log.states.empty());
  edit(s, c, "poly", db::Rect{0, 0, 4, 4});
  worker.wake();
  REQUIRE(worker.run_until_idle(s.ed, kTimeoutMs));
  CHECK(log.states == std::vector<drc::CheckerState>{drc::CheckerState::busy, drc::CheckerState::idle});

  SECTION("background checking off: the worker leaves the list alone") {
    s.ed.drc.background = drc::BackgroundMode::off;
    edit(s, c, "poly", db::Rect{10, 0, 14, 4});
    worker.wake();
    REQUIRE(worker.run_until_idle(s.ed, kTimeoutMs));
    CHECK_FALSE(s.ed.drc.pending.empty());
    CHECK(log.states.size() == 2);
  }
  worker.stop();
  s.ed.drc.events.remove(log);
}

TEST_CASE("Racing random edits against the worker gives catch-up's error planes",
          "[cap:drc-engine][req:Pending list and background scheduling][tsan]") {
  struct Op {
    std::size_t cell;
    bool erase;
    db::Rect r;
  };
  std::mt19937 rng(20261110);
  std::uniform_int_distribution<int> coord(-60, 60);
  std::uniform_int_distribution<int> size(1, 25);
  std::uniform_int_distribution<std::size_t> which(0, 2);
  std::uniform_int_distribution<int> kind(0, 3);
  std::vector<Op> ops;
  for (int i = 0; i < 150; ++i) {
    const int x = coord(rng);
    const int y = coord(rng);
    ops.push_back(Op{which(rng), kind(rng) == 0, db::Rect{x, y, x + size(rng), y + size(rng)}});
  }
  const drc::test::Recorder rec(tech::TypeMask::of(drc::test::engine_tech()->find_type("poly").id.value()) |
                                tech::TypeMask::of(drc::test::engine_tech()->find_type("metal1").id.value()));

  auto run = [&](bool racing) {
    auto s = std::make_unique<Session>();
    s->set_rules(s->rules(3, 32).build());
    std::vector<db::CellDef*> cells{&s->cell("a"), &s->cell("b"), &s->cell("c")};
    drc::BackgroundChecker& worker = *s->ed.drc.worker;
    if (racing) {
      worker.start(s->ed);
    }
    std::mt19937 pause(7);
    for (const Op& op : ops) {
      edit(*s, *cells[op.cell], op.erase ? "metal1" : (op.r.x0 % 2 == 0 ? "metal1" : "poly"), op.r, op.erase);
      if (racing) {
        worker.wake();
        if (pause() % 3 == 0) {
          (void)worker.commit_ready(s->ed);
        }
        if (pause() % 5 == 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(pause() % 300));
        }
      }
    }
    if (racing) {
      REQUIRE(worker.run_until_idle(s->ed, 60000));
      worker.stop();
    } else {
      REQUIRE(drc::catchup(s->ed).ok());
    }
    std::vector<std::vector<ErrorTile>> planes;
    for (const db::CellDef* c : cells) {
      planes.push_back(drc::test::errors(*c));
      CHECK(drc::test::checks(*c).empty());
    }
    CHECK(s->ed.drc.pending.empty());
    return planes;
  };
  const auto raced = run(true);
  const auto caught_up = run(false);
  CHECK(raced == caught_up);
}

namespace {

std::atomic<std::int64_t> g_first_call_ns{0};

drc::CheckOutcome timing_kernel(const drc::CheckInput& in, const db::Rect& check, const db::Rect& clip,
                                drc::ErrorSink& sink) {
  std::int64_t expected = 0;
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  (void)g_first_call_ns.compare_exchange_strong(expected,
                                                std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  (void)in;
  (void)check;
  (void)clip;
  (void)sink;
  return drc::CheckOutcome::completed;
}

}  // namespace

// Design D16 "Background start latency": from the end of a paint command
// (edit and wake) to the first kernel call of the worker, on a 100 x 100
// array of a cell. Hidden: run with `[benchmark]` to print the numbers.
TEST_CASE("Background start latency on a 100 x 100 array", "[.][benchmark]") {
  Session s;
  s.set_rules(s.rules(3, 400).build());
  db::CellDef& cell = s.cell("cell");
  db::CellDef& top = s.cell("top");
  s.paint(cell, "metal1", db::Rect{0, 0, 40, 4});
  s.paint(cell, "poly", db::Rect{0, 10, 4, 40});
  db::ArrayInfo a;
  a.xhi = 99;
  a.yhi = 99;
  a.xsep = 46;
  a.ysep = 54;
  s.place(top, cell, db::kIdentity, a);
  const drc::ScopedKernel kernel(&timing_kernel);
  drc::BackgroundChecker& worker = *s.ed.drc.worker;
  worker.start(s.ed);
  std::vector<double> ms;
  for (int i = 0; i < 20; ++i) {
    g_first_call_ns = 0;
    const auto start = std::chrono::steady_clock::now();
    edit(s, top, "metal1", db::Rect{100 * i, 2000, 100 * i + 10, 2010});
    worker.wake();
    while (g_first_call_ns.load() == 0) {
      (void)worker.commit_ready(s.ed);
      std::this_thread::yield();
    }
    const auto first = std::chrono::steady_clock::time_point{std::chrono::nanoseconds{g_first_call_ns.load()}};
    ms.push_back(std::chrono::duration<double, std::milli>(first - start).count());
    REQUIRE(worker.run_until_idle(s.ed, kTimeoutMs));
  }
  worker.stop();
  std::sort(ms.begin(), ms.end());
  WARN("background start latency (ms): median " << ms[ms.size() / 2] << ", max " << ms.back());
  CHECK(ms.back() < 100.0);
}
