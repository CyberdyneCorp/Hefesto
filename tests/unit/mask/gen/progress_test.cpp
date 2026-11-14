// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Progress and cancellation (mask-hierarchical-generation "Progress
// reporting and interruptibility", Q-MASK-414): the throttled reports with
// a test clock, cancellation from the progress callback, and the temporary
// output file that a cancelled write discards.
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>

#include "gen_fixture.hpp"
#include "hefesto/mask/gen/throttle.hpp"
#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::mask::test {

using std::chrono::seconds;

TEST_CASE("The progress throttle follows Magic's reporting rule",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  platform::FixedClock clock;
  std::vector<double> reports;
  Progress progress{[&](double fraction) { reports.push_back(fraction); }, nullptr};
  ProgressThrottle throttle(&progress, &clock);
  throttle.step(1, 10);  // never for the first square
  clock.advance(seconds(6));
  throttle.step(1, 10);
  CHECK(reports.empty());
  throttle.step(2, 10);
  REQUIRE(reports.size() == 1);
  CHECK(reports.back() == 0.2);
  throttle.step(3, 10);  // the five-second timer restarted
  CHECK(reports.size() == 1);
  clock.advance(seconds(5));
  throttle.step(4, 10);
  REQUIRE(reports.size() == 2);
  CHECK(reports.back() == 0.4);
  throttle.finish();
  CHECK(reports.back() == 1.0);
  CHECK(throttle.reported());

  std::vector<double> quiet;
  Progress silent{[&](double fraction) { quiet.push_back(fraction); }, nullptr};
  ProgressThrottle fast(&silent, &clock);
  fast.step(2, 2);  // done before the timer expired: nothing, not even 100 %
  fast.finish();
  CHECK(quiet.empty());
}

namespace {

// A row of children 105 apart whose m1 gaps all interact.
struct LongRow {
  GenFixture& f;
  std::size_t layer;
  db::CellDef& top;

  LongRow(GenFixture& fx, int count)
      : f(fx),
        layer(fx.layer("M1X", {types_op(OpKind::op_or, {fx.m1}), op(OpKind::grow, 30), op(OpKind::shrink, 30)})),
        top(fx.cell("top")) {
    db::CellDef& child = f.cell("child");
    f.paint(child, f.m1, Rect{0, 10, 100, 90});
    for (int i = 0; i < count; ++i) {
      f.place(top, child, translate(105 * i, 0));
    }
  }
};

}  // namespace

TEST_CASE("A long write shows progress", "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  GenFixture f;
  LongRow row(f, 12);
  platform::FixedClock clock;
  std::vector<double> reports;
  Progress progress{[&](double fraction) { reports.push_back(fraction); }, nullptr};
  f.ctx.progress = &progress;
  f.ctx.clock = &clock;
  GenerationSource source = f.source();
  const InteractionFinder shared = source.find_interaction;
  source.find_interaction = [&](const db::CellDef& cell, const Rect& area, std::int64_t r) {
    clock.advance(seconds(6));  // every square takes six seconds
    return shared(cell, area, r);
  };
  const LayerPlanes out = generate_hierarchical(source, row.top, f.style, f.ctx, HierarchyOptions{});
  REQUIRE(reports.size() >= 2);
  CHECK(reports.back() == 1.0);
  for (std::size_t i = 1; i < reports.size(); ++i) {
    CHECK(reports[i] >= reports[i - 1]);
  }
  // Progress does not change the output: eleven seams.
  CHECK(GenFixture::rects(out, row.layer).size() == 11);
}

TEST_CASE("A cancelled write leaves no file", "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  GenFixture f;
  LongRow row(f, 12);
  std::atomic<bool> stop{false};
  int squares = 0;
  Progress progress{{}, &stop};
  f.ctx.progress = &progress;
  GenerationSource source = f.source();
  const InteractionFinder shared = source.find_interaction;
  source.find_interaction = [&](const db::CellDef& cell, const Rect& area, std::int64_t r) {
    if (++squares == 2) {
      stop = true;  // the user cancels from the progress host
    }
    return shared(cell, area, r);
  };
  platform::MemoryFileSystem fs;
  {
    OutputFile file(fs, "/w/chip.gds");
    REQUIRE(fs.write_atomic(file.temporary(), "partial").ok());
    const LayerPlanes out = generate_hierarchical(source, row.top, f.style, f.ctx, HierarchyOptions{});
    CHECK(f.ctx.cancelled());
    CHECK(squares == 2);  // generation stopped at the next square
    file.discard();
  }
  CHECK_FALSE(fs.exists("/w/chip.gds"));
  CHECK_FALSE(fs.exists("/w/.chip.gds.hefesto-tmp"));
}

TEST_CASE("A completed write is renamed into place",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/chip.gds", "old").ok());
  {
    OutputFile file(fs, "/w/chip.gds");
    CHECK(file.temporary() == platform::Path{"/w/.chip.gds.hefesto-tmp"});
    REQUIRE(fs.write_atomic(file.temporary(), "new").ok());
    REQUIRE(file.commit().ok());
  }
  CHECK(fs.read("/w/chip.gds").value() == "new");
  CHECK_FALSE(fs.exists("/w/.chip.gds.hefesto-tmp"));
  {
    OutputFile abandoned(fs, "/w/chip.gds");
    REQUIRE(fs.write_atomic(abandoned.temporary(), "lost").ok());
  }  // neither committed nor discarded: removed, the target kept
  CHECK(fs.read("/w/chip.gds").value() == "new");
  CHECK_FALSE(fs.exists("/w/.chip.gds.hefesto-tmp"));
}

}  // namespace hefesto::mask::test
