// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Structure ordering": the use index under the database lock's
// thread model (design D4, D7). Index rebuilds happen during reads, so
// DRC-worker-like threads search (and rebuild) under the shared lock while
// the command thread enumerates under the shared lock and places uses
// under the exclusive lock. Run in the linux-tsan CI leg (ctest), where
// ThreadSanitizer checks the index mutex and the snapshot orders.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstddef>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/db/use_index.hpp"

namespace db = hefesto::db;

namespace {

constexpr int kRounds = 60;
constexpr int kUsesPerRound = 12;  // above the rebuild threshold of 10

/// A DRC worker: partial searches (each may rebuild the bins) under the
/// shared lock, one of them searching the same index again from its
/// visitor, until the command thread is done.
void worker(db::DatabaseLock& lock, const db::CellDef& top, const std::atomic<bool>& done, int seed,
            std::atomic<std::size_t>& found) {
  int k = seed;
  while (!done.load(std::memory_order_acquire)) {
    while (lock.writer_waiting()) {  // as the DRC worker yields to commands
      std::this_thread::yield();
    }
    const db::DatabaseLock::Shared shared(lock);
    const db::Coord x = (k * 37) % 4000;
    const db::Rect area{x, 0, x + 60, 30};
    (void)db::uses_overlapping(top, area, [&](db::CellUse& u) {
      found.fetch_add(1, std::memory_order_relaxed);
      (void)db::uses_overlapping(top, u.bbox, [](db::CellUse&) { return true; });
      return k % 2 == 0;  // odd rounds stop early (undoing late sub-binnings)
    });
    ++k;
  }
}

/// The command thread's read: an enumeration under the shared lock is a
/// consistent snapshot holding every use exactly once.
bool consistent_order(db::DatabaseLock& lock, const db::CellDef& top) {
  const db::DatabaseLock::Shared shared(lock);
  const std::vector<db::CellUse*> order = db::uses_in_search_order(top, hefesto::compat::CompatSettings{});
  const std::set<db::CellUse*> unique(order.begin(), order.end());
  return order.size() == top.uses().size() && unique.size() == order.size();
}

}  // namespace

TEST_CASE("Use index: rebuilds from DRC workers while the command thread enumerates and places",
          "[cap:gds-write][req:Structure ordering]") {
  db::CellDef child("child", 1);
  child.set_bboxes(db::Rect{0, 0, 4, 4}, db::Rect{0, 0, 4, 4});
  db::CellDef top("top", 1);
  db::DatabaseLock lock;
  std::atomic<bool> done{false};
  std::atomic<std::size_t> found{0};
  std::vector<std::thread> workers;
  for (int i = 0; i < 2; ++i) {
    workers.emplace_back([&, i] { worker(lock, top, done, i, found); });
  }
  bool all_consistent = true;
  for (int round = 0; round < kRounds; ++round) {
    {
      const db::DatabaseLock::Exclusive exclusive(lock);
      for (int i = 0; i < kUsesPerRound; ++i) {
        db::PlaceOptions o;
        o.transform = db::translation(5 * (round * kUsesPerRound + i) % 4000, 7 * (i % 4));
        REQUIRE(db::place(top, child, o).ok());
      }
      if (round % 3 == 2) {
        REQUIRE(db::remove_use(*top.uses().front()).ok());
      }
    }
    all_consistent = consistent_order(lock, top) && all_consistent;
  }
  done.store(true, std::memory_order_release);
  for (std::thread& t : workers) {
    t.join();
  }
  CHECK(all_consistent);
  CHECK(top.use_index().size() == top.uses().size());
  CHECK(top.use_index().state_for_test().binned);
}
