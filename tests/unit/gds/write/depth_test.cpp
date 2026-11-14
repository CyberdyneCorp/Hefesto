// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Write preconditions and subtree loading": the hierarchy depth
// limit (kMaxSubtreeDepth, GDS-WRITE-014) of the recursive walks (subtree
// loading, the abstract-view check, the structure traversal, generation),
// each run on a secondary thread with platform::kDeepRecursionStackBytes,
// as a host that runs the write off the command thread must (task 12.2; a
// std::thread's default 512 KiB stack on macOS overflows well before the
// limit), and the cost of loading a wide, deep hierarchy.
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>

#include "hefesto/platform/stack_thread.hpp"
#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

// c0 -> c1 -> ... -> c<levels>, each use at the origin; the leaf has paint.
db::CellDef& chain(Fixture& f, std::size_t levels) {
  db::CellDef* child = &f.cell("c" + std::to_string(levels));
  f.paint(*child, f.m1, db::Rect{0, 0, 2, 2});
  for (std::size_t i = levels; i-- > 0;) {
    db::CellDef& parent = f.cell("c" + std::to_string(i));
    f.place(parent, *child);
    child = &parent;
  }
  return *child;
}

// The write of `root` on a secondary thread with the stack the writers
// document.
void write_on_thread(Fixture& f, db::CellDef& root) {
  platform::run_on_stack(platform::kDeepRecursionStackBytes, [&f, &root] { (void)f.write(root); });
}

TEST_CASE("a hierarchy at the depth limit is written on a secondary thread",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& root = chain(f, gds::kMaxSubtreeDepth);
    write_on_thread(f, root);
    CHECK_FALSE(f.has_code("GDS-WRITE-014"));
    CHECK(structure_names(decode(f.file())).size() == gds::kMaxSubtreeDepth + 1);
  }
}

TEST_CASE("a hierarchy one level deeper than the limit fails the write",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& root = chain(f, gds::kMaxSubtreeDepth + 1);
    write_on_thread(f, root);
    CHECK_FALSE(f.last.ok());
    CHECK(f.has_code("GDS-WRITE-014"));
  }
}

TEST_CASE("subtree loading of a wide, deep hierarchy visits each cell once",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  // Regression: dbReadAreaFunc re-descends into every use, so 40 levels of
  // two uses each visited about 2^40 cells before a covered cell was
  // remembered. Each level's cell is read once, in the same order.
  Fixture f;
  constexpr std::size_t kLevels = 40;
  db::CellDef* child = &f.cell("w" + std::to_string(kLevels));
  f.paint(*child, f.m1, db::Rect{0, 0, 2, 2});
  for (std::size_t i = kLevels; i-- > 0;) {
    db::CellDef& parent = f.cell("w" + std::to_string(i));
    f.place(parent, *child, {}, {}, "a");
    f.place(parent, *child, db::translation(1, 0), {}, "b");
    child = &parent;
  }
  db::CellDef& root = *child;
  std::vector<std::string> reads;
  f.ed.cells.for_each([&](db::CellDef& def) {
    if (&def != &root) {
      def.flags().available = false;
    }
  });
  gds::SubtreeLoader loader;
  loader.read = [&reads](db::CellDef& def) {
    reads.push_back(def.name());
    def.flags().available = true;
    return true;
  };
  const auto start = std::chrono::steady_clock::now();
  const gds::SubtreeResult result = gds::load_subtree(root, loader, true, f.compat);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(result.ok());
  REQUIRE(reads.size() == kLevels);
  for (std::size_t i = 0; i < kLevels; ++i) {
    CHECK(reads[i] == "w" + std::to_string(i + 1));
  }
  CHECK(elapsed < std::chrono::seconds(10));
}

TEST_CASE("subtree loading stops when the host cancels",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  Fixture f;
  db::CellDef& root = chain(f, 8);
  std::atomic<bool> cancel{true};
  gds::SubtreeLoader loader;
  loader.cancel = &cancel;
  const gds::SubtreeResult result = gds::load_subtree(root, loader, true, f.compat);
  CHECK(result.cancelled);
  CHECK_FALSE(result.ok());
  mask::Progress progress;
  progress.cancel = &cancel;
  REQUIRE(f.fs.write_atomic("/out.gds", "previous").ok());
  f.install();
  f.last = gds::write(f.ed, root, "/out.gds", f.options, &progress);
  CHECK_FALSE(f.last.ok());
  CHECK(f.has_code("GDS-WRITE-013"));
  CHECK(f.file() == "previous");
}

}  // namespace
}  // namespace gds_write_test
