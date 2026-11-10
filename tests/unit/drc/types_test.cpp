// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared DRC types (openspec/changes/m3-drc/design.md D4/D5): the
// frozen rule table built without the parser (as CHECK, ENGINE and COMMANDS
// tests build theirs), the why-text table, the error sink and the events.
// Owner: INTEGRATION. [m3-scaffold] tests until the owners' requirement
// tests cover the same ground.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/drc/events.hpp"
#include "hefesto/drc/types.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;

namespace {

constexpr tech::TileType kM1 = 9;

drc::Rule width_rule(std::int32_t w, drc::WhyIndex why) {
  drc::Rule r;
  r.dist = w;
  r.cdist = w;
  r.ok = tech::TypeMask::of(kM1);
  r.corner = tech::TypeMask::of(kM1);
  r.edge_plane = 6;
  r.check_plane = 6;
  r.why = why;
  return r;
}

}  // namespace

TEST_CASE("the why table starts with Magic's five predefined texts", "[m3-scaffold]") {
  drc::WhyTable why;
  CHECK(why.size() == 5);
  CHECK(why.text(drc::why::kOverlap) == "Can't overlap those layers");
  CHECK(why.text(drc::why::kInSubcell) == "See error definition in the subcell");
  CHECK(why.text(drc::why::kOffGrid) == "This position does not align with the manufacturing grid");
  CHECK(why.text(0).empty());
  CHECK(why.intern("too narrow") == drc::why::kFirstUser);
  CHECK(why.intern("too narrow") == drc::why::kFirstUser);  // identical strings share an index
  CHECK(why.intern("other") == 7);
  CHECK(why.find("other") == 7);
  CHECK(why.find("absent") == 0);
}

TEST_CASE("a rule set built without the parser keeps per-pair order", "[m3-scaffold]") {
  drc::RuleSetBuilder builder;
  const drc::WhyIndex small = builder.why("small");
  const drc::WhyIndex big = builder.why("big");
  builder.style_name("test").add(tech::kSpace, kM1, width_rule(2, small)).add(tech::kSpace, kM1, width_rule(3, big));
  drc::Rule rev = width_rule(3, big);
  rev.flags = drc::flags::kReverse | drc::flags::kSplitTile;
  builder.add(kM1, tech::kSpace, rev);
  builder.halo(drc::Halo{3, 3, 0}).step_size(48).exception_names({"SRAM"});
  const std::shared_ptr<const drc::RuleSet> set = builder.build();

  CHECK(set->style_name() == "test");
  REQUIRE(set->rules(tech::kSpace, kM1).size() == 2);
  CHECK(set->rules(tech::kSpace, kM1)[0].why == small);
  CHECK(set->rules(tech::kSpace, kM1)[1].dist == 3);
  REQUIRE(set->rules(kM1, tech::kSpace).size() == 1);
  CHECK(set->rules(kM1, tech::kSpace)[0].reverse());
  CHECK(set->rules(kM1, tech::kSpace)[0].has(drc::flags::kSplitTile));
  CHECK_FALSE(set->rules(kM1, tech::kSpace)[0].nonstandard());
  CHECK(set->rules(kM1, kM1).empty());
  CHECK(set->rules(300, kM1).empty());
  CHECK(set->all_rules().size() == 3);
  CHECK(set->why().text(big) == "big");
  CHECK(set->halo().value == 3);
  CHECK(set->step_size() == 48);
  REQUIRE(set->exception_names().size() == 1);
  CHECK(set->details() == nullptr);
  // An untouched paint table row is "paint wins".
  CHECK(set->paint_result(6, kM1, tech::kErrorS) == tech::kErrorS);
}

TEST_CASE("the DRC paint table keeps the rows a style sets", "[m3-scaffold]") {
  db::PaintRow row = drc::identity_paint_row(kM1);
  row[10] = tech::kCheckSubcell;  // illegal overlap of m1 over type 10
  drc::RuleSetBuilder builder;
  builder.paint_row(7, kM1, row);
  const auto set = builder.build();
  CHECK(set->paint_result(7, 10, kM1) == tech::kCheckSubcell);
  CHECK(set->paint_result(7, tech::kSpace, kM1) == kM1);
  CHECK(set->paint_result(6, 10, kM1) == kM1);
}

TEST_CASE("an error sink counts reports and collects them in order", "[m3-scaffold]") {
  drc::CollectingErrorSink sink;
  drc::CheckStatistics stats;
  sink.set_statistics(&stats);
  const drc::Rule rule = width_rule(3, 6);
  sink.report(db::Rect{0, 0, 1, 1}, rule);
  sink.report(db::Rect{2, 2, 3, 3}, rule);
  CHECK(sink.count() == 2);
  REQUIRE(sink.errors().size() == 2);
  CHECK(sink.errors()[1].area == db::Rect{2, 2, 3, 3});
  CHECK(sink.statistics() == &stats);
  drc::CheckStatistics more;
  more.tiles = 4;
  stats += more;
  CHECK(stats.tiles == 4);
}

TEST_CASE("DRC events alternate busy and idle and reach every observer", "[m3-scaffold]") {
  struct Recorder final : drc::DrcObserver {
    std::vector<std::string> seen;
    void on_state(drc::CheckerState s) override { seen.emplace_back(s == drc::CheckerState::busy ? "busy" : "idle"); }
    void on_square_committed(const db::CellDef& cell, const db::Rect&) override { seen.push_back("square " + cell.name()); }
    void on_style_changed(const std::string& style) override { seen.push_back("style " + style); }
  };
  drc::DrcEvents events;
  Recorder r;
  events.add(r);
  events.add(r);  // added once
  CHECK(events.size() == 1);
  events.publish_state(drc::CheckerState::idle);  // already idle: dropped
  events.publish_state(drc::CheckerState::busy);
  events.publish_state(drc::CheckerState::busy);
  const db::CellDef cell{"inv", 8};
  events.publish_square(cell, db::Rect{0, 0, 10, 10});
  events.publish_state(drc::CheckerState::idle);
  events.publish_style("drc(full)");
  events.remove(r);
  events.publish_state(drc::CheckerState::busy);
  CHECK(r.seen == std::vector<std::string>{"busy", "square inv", "idle", "style drc(full)"});
}
