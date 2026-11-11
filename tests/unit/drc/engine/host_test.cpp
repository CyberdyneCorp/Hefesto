// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::BackgroundHost (design D8 "Hosts"): what an interactive host does
// around the worker. Startup work publishes busy at once, finished squares
// are announced on the worker thread and committed by service() on the
// host's thread, the prompt character follows busy/idle as Magic's console
// prompt does, and destroying the host stops the worker.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "engine_fixture.hpp"
#include "hefesto/drc/engine/host.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::ErrorTile;
using drc::test::Session;

namespace {

constexpr auto kTimeout = std::chrono::seconds(20);

struct StateLog final : drc::DrcObserver {
  std::vector<drc::CheckerState> states;
  void on_state(drc::CheckerState st) override { states.push_back(st); }
};

/// The host's "event loop": counts notifications from the worker thread.
struct Mailbox {
  std::mutex mutex;
  std::condition_variable cv;
  int posted = 0;

  void post() {
    const std::lock_guard lock(mutex);
    ++posted;
    cv.notify_all();
  }
  bool wait() {
    std::unique_lock lock(mutex);
    const bool got = cv.wait_for(lock, kTimeout, [this] { return posted > 0; });
    posted = 0;
    return got;
  }
};

/// Services the host until the checker is idle (the host's event loop).
void run_host_until_idle(drc::BackgroundHost& host, Mailbox& mail) {
  const auto deadline = std::chrono::steady_clock::now() + kTimeout;
  while (host.busy() && std::chrono::steady_clock::now() < deadline) {
    if (mail.wait()) {
      (void)host.service();
    }
  }
}

}  // namespace

TEST_CASE("An interactive host commits on its own thread and switches the prompt",
          "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  db::CellDef& c = s.cell("c");
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  StateLog log;
  s.ed.drc.events.add(log);
  s.paint(c, "poly", db::Rect{0, 0, 4, 4});
  drc::check_this(s.ed, c, db::Rect{0, 0, 4, 4});  // work left by startup

  Mailbox mail;
  {
    drc::BackgroundHost host(s.ed, [&mail] { mail.post(); });
    CHECK(s.ed.drc.worker->running());
    CHECK(host.busy());  // published when the host starts, as Magic's idle loop finding DRCHasWork
    CHECK(host.prompt_char() == ']');
    run_host_until_idle(host, mail);
    CHECK_FALSE(host.busy());
    CHECK(host.prompt_char() == '%');
    CHECK(s.ed.drc.pending.empty());
    {
      const db::DatabaseLock::Exclusive guard(s.ed.database_lock);
      CHECK(drc::test::errors(c) == std::vector<ErrorTile>{{{0, 0, 4, 4}, tech::kErrorP}});
    }
    CHECK(host.service() == 0);  // nothing waits
  }
  CHECK_FALSE(s.ed.drc.worker->running());  // the host stopped the worker
  CHECK(log.states == std::vector<drc::CheckerState>{drc::CheckerState::busy, drc::CheckerState::idle});
  s.ed.drc.events.remove(log);
}

TEST_CASE("An interactive host with nothing pending stays idle",
          "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  StateLog log;
  s.ed.drc.events.add(log);
  {
    const drc::BackgroundHost host(s.ed, [] {});
    CHECK_FALSE(host.busy());
    CHECK(host.prompt_char() == '%');
  }
  CHECK(log.states.empty());
  s.ed.drc.events.remove(log);
}

TEST_CASE("Destroying a busy host drops the square and publishes idle",
          "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  db::CellDef& c = s.cell("c");
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  StateLog log;
  s.ed.drc.events.add(log);
  s.paint(c, "poly", db::Rect{0, 0, 4, 4});
  drc::check_this(s.ed, c, db::Rect{0, 0, 4, 4});
  {
    const drc::BackgroundHost host(s.ed, [] {});  // nobody services it: the square stays uncommitted
    CHECK(host.busy());
  }
  CHECK(log.states == std::vector<drc::CheckerState>{drc::CheckerState::busy, drc::CheckerState::idle});
  CHECK_FALSE(s.ed.drc.pending.empty());  // Magic quitting with work pending
  CHECK(drc::test::errors(c).empty());
  CHECK_FALSE(drc::test::checks(c).empty());
  s.ed.drc.events.remove(log);
}

TEST_CASE("A host started without a worker creates one", "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  s.ed.drc.worker.reset();
  {
    const drc::BackgroundHost host(s.ed, [] {});
    REQUIRE(s.ed.drc.worker != nullptr);
    CHECK(s.ed.drc.worker->running());
  }
  CHECK_FALSE(s.ed.drc.worker->running());
}
