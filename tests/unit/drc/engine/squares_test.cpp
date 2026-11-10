// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-engine: "Pending list and background scheduling" (ordering, the
// synchronous loop), "Step square processing", "Interaction check over an
// area", "Forced recheck of an area", "Flat check".

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "engine_fixture.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::ErrorTile;
using drc::test::Session;

TEST_CASE("Negative coordinates", "[cap:drc-engine][req:Step square processing]") {
  Session s;
  s.set_rules(s.rules(0, 100).build());
  db::CellDef& c = s.cell("c");
  drc::test::paint_check(c, *s.tech, db::Rect{-30, 250, -20, 260});
  drc::check_this(s.ed, c, std::nullopt);
  const auto r = drc::process_square(s.ed);
  REQUIRE(r.has_value());
  CHECK(r->square == db::Rect{-100, 200, 0, 300});
  CHECK(r->erase_box == db::Rect{-30, 250, -20, 260});
}

TEST_CASE("Step square: erase box, check box and the atomic update", "[cap:drc-engine][req:Step square processing]") {
  Session s;
  s.set_rules(s.rules(3, 100).build());
  db::CellDef& c = s.cell("c");
  const tech::TileType poly = s.type("poly");
  const drc::test::Recorder rec(tech::TypeMask::of(poly));
  s.paint(c, "poly", db::Rect{20, 20, 30, 30});
  // Check tiles in two squares; the first square's erase box covers both
  // of its tiles, at full extent, clipped to the square.
  drc::test::paint_check(c, *s.tech, db::Rect{10, 10, 40, 40});
  drc::test::paint_check(c, *s.tech, db::Rect{60, 90, 120, 95});
  // Old errors: error_p inside B is erased, error_s inside B+ is erased,
  // error_s outside B+ but inside S stays.
  REQUIRE(c.plane(tech::kPlaneDrcError).paint(db::Rect{0, 0, 100, 5}, drc::error_paint_row(tech::kErrorS)).ok());
  REQUIRE(c.plane(tech::kPlaneDrcError).paint(db::Rect{12, 12, 14, 14}, drc::error_paint_row(tech::kErrorP)).ok());
  REQUIRE(c.plane(tech::kPlaneDrcError).paint(db::Rect{8, 8, 9, 9}, drc::error_paint_row(tech::kErrorS)).ok());
  drc::check_this(s.ed, c, std::nullopt);
  const auto r = drc::process_square(s.ed);
  REQUIRE(r.has_value());
  CHECK(r->square == db::Rect{0, 0, 100, 100});
  CHECK(r->erase_box == db::Rect{10, 10, 100, 95});
  CHECK(r->check_box == db::Rect{7, 7, 100, 98});
  CHECK(r->errors == std::vector<ErrorTile>{{{20, 20, 30, 30}, tech::kErrorP}});
  // Nothing changes until the commit.
  CHECK(drc::test::errors(c).size() == 3);
  c.flags().modified = false;
  c.flags().needs_new_timestamp = false;
  const auto stamp = c.timestamp();
  drc::commit(s.ed, *r);
  CHECK(drc::test::checks(c) == std::vector<ErrorTile>{{{100, 90, 120, 95}, tech::kCheckPaint}});
  CHECK(drc::test::errors(c) == std::vector<ErrorTile>{{{0, 0, 100, 5}, tech::kErrorS},
                                                        {{20, 20, 30, 30}, tech::kErrorP}});
  CHECK(c.flags().modified);
  CHECK_FALSE(c.flags().needs_new_timestamp);
  CHECK(c.timestamp() == stamp);
}

TEST_CASE("Step square: non-editable and vendor-GDS cells are not marked modified",
          "[cap:drc-engine][req:Step square processing]") {
  Session s;
  db::CellDef& c = s.cell("c");
  const bool vendor = GENERATE(false, true);
  (vendor ? c.flags().vendor_gds : c.flags().no_edit) = true;
  c.flags().modified = false;
  drc::check_this(s.ed, c, db::Rect{0, 0, 5, 5});
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(drc::test::checks(c).empty());
  CHECK_FALSE(c.flags().modified);
}

TEST_CASE("Children before parents and the list order", "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  s.set_rules(s.rules(1, 1000).build());
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& top = s.cell("top");
  s.paint(leaf, "poly", db::Rect{0, 0, 2, 10});
  s.place(top, leaf, db::translation(50, 50));
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  drc::check_this(s.ed, leaf, db::Rect{0, 0, 2, 10});
  std::vector<const db::CellDef*> order;
  while (auto r = drc::process_square(s.ed)) {
    order.push_back(r->cell);
    drc::commit(s.ed, *r);
  }
  // leaf's check area [-1,11]x[-1,3] spans four squares of the 1000 grid,
  // then leaf is knocked off; top's one square, then top is knocked off.
  CHECK(order == std::vector<const db::CellDef*>{&leaf, &leaf, &leaf, &leaf, &leaf, &top, &top});
  CHECK(s.ed.drc.pending.empty());
  // top copied leaf's new error up (why 4) when it was checked.
  CHECK(drc::test::errors(top) == std::vector<ErrorTile>{{{50, 50, 52, 60}, tech::kErrorP}});
}

TEST_CASE("The pending list keeps DRCCheckThis's order and its last-entry quirk",
          "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  db::CellDef& a = s.cell("a");
  db::CellDef& b = s.cell("b");
  drc::PendingList& p = s.ed.drc.pending;
  CHECK(p.append(a));
  CHECK(p.append(b));
  CHECK_FALSE(p.append(a));  // listed before the last entry
  CHECK(p.append(b));        // the last entry is never compared: appended again
  REQUIRE(p.size() == 3);
  CHECK(p.remove(b));        // every entry of a deleted cell goes
  CHECK(p.size() == 1);
  p.pop_front();
  CHECK(p.empty());
}

TEST_CASE("Catch-up publishes busy and idle once around the work",
          "[cap:drc-engine][req:Pending list and background scheduling]") {
  struct Log final : drc::DrcObserver {
    std::vector<drc::CheckerState> states;
    int squares = 0;
    void on_state(drc::CheckerState st) override { states.push_back(st); }
    void on_square_committed(const db::CellDef&, const db::Rect&) override { ++squares; }
  } log;
  Session s;
  s.ed.drc.events.add(log);
  REQUIRE(drc::catchup(s.ed).ok());  // nothing pending: no transition
  CHECK(log.states.empty());
  db::CellDef& c = s.cell("c");
  drc::check_this(s.ed, c, db::Rect{0, 0, 5, 5});
  s.ed.drc.background = drc::BackgroundMode::off;  // catchup runs regardless
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(log.states == std::vector<drc::CheckerState>{drc::CheckerState::busy, drc::CheckerState::idle});
  CHECK(log.squares == 1);
  CHECK(s.ed.drc.pending.empty());
  s.ed.drc.events.remove(log);
}

TEST_CASE("The background state is \"not set\" during a command", "[cap:drc-engine][req:Pending list and background scheduling]") {
  Session s;
  auto saved = drc::begin_command(s.ed);
  CHECK(s.ed.drc.background == drc::BackgroundMode::not_set);
  drc::end_command(s.ed, saved);
  CHECK(s.ed.drc.background == drc::BackgroundMode::on);
  // `drc off` inside a command sticks; `drc on` from off sticks.
  saved = drc::begin_command(s.ed);
  s.ed.drc.background = drc::BackgroundMode::off;
  drc::end_command(s.ed, saved);
  CHECK(s.ed.drc.background == drc::BackgroundMode::off);
  saved = drc::begin_command(s.ed);
  CHECK(s.ed.drc.background == drc::BackgroundMode::off);
  s.ed.drc.background = drc::BackgroundMode::on;
  drc::end_command(s.ed, saved);
  CHECK(s.ed.drc.background == drc::BackgroundMode::on);
}

TEST_CASE("Cell without subcells", "[cap:drc-engine][req:Interaction check over an area]") {
  Session s;
  s.set_rules(s.rules(2, 1000).build());
  db::CellDef& c = s.cell("c");
  s.paint(c, "poly", db::Rect{12, 12, 14, 30});
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  drc::CollectingErrorSink sink;
  REQUIRE(drc::interaction_check(s.ed, c, db::Rect{0, 0, 1000, 1000}, db::Rect{10, 10, 20, 20}, sink) ==
          drc::CheckOutcome::completed);
  const auto calls = rec.calls();
  REQUIRE(calls.size() == 1);
  CHECK(calls[0].cell == &c);
  CHECK(calls[0].check == db::Rect{8, 8, 22, 22});
  CHECK(calls[0].clip == db::Rect{10, 10, 20, 20});
  // Through a square, the same errors are error_p.
  drc::test::paint_check(c, *s.tech, db::Rect{10, 10, 20, 20});
  drc::check_this(s.ed, c, std::nullopt);
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(drc::test::errors(c) == std::vector<ErrorTile>{{{12, 12, 14, 20}, tech::kErrorP}});
}

TEST_CASE("The interaction grid is offset by half a step", "[cap:drc-engine][req:Interaction check over an area]") {
  Session s;
  s.set_rules(s.rules(0, 100).build());
  db::CellDef& c = s.cell("c");
  const drc::test::Recorder rec;
  drc::CollectingErrorSink sink;
  // Area [0,100]^2 is covered by grid squares from -50 by 100: four pieces.
  (void)drc::interaction_check(s.ed, c, db::Rect{0, 0, 100, 100}, db::Rect{0, 0, 100, 100}, sink);
  std::vector<db::Rect> clips;
  for (const auto& call : rec.calls()) {
    clips.push_back(call.clip);
  }
  CHECK(clips == std::vector<db::Rect>{{0, 0, 50, 50}, {0, 50, 50, 100}, {50, 0, 100, 50}, {50, 50, 100, 100}});
  // C truncating division: (-120 / 100) * 100 - 50 = -150 starts the grid.
  rec.clear();
  (void)drc::interaction_check(s.ed, c, db::Rect{-120, -10, -110, -5}, db::Rect{-120, -10, -110, -5}, sink);
  REQUIRE(rec.calls().size() == 1);
  CHECK(rec.calls()[0].clip == db::Rect{-120, -10, -110, -5});
}

TEST_CASE("Recheck whole design", "[cap:drc-engine][req:Forced recheck of an area]") {
  Session s;
  s.set_rules(s.rules(2, 1000).build());
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& mid = s.cell("mid");
  db::CellDef& top = s.cell("top");
  s.paint(leaf, "poly", db::Rect{0, 0, 10, 10});
  s.place(mid, leaf, db::translation(5, 0));
  s.paint(mid, "metal1", db::Rect{0, 0, 3, 3});
  s.place(top, mid, db::translation(100, 100));
  REQUIRE(drc::force_recheck(s.ed, top, top.bbox()).ok());
  CHECK(drc::test::checks(leaf) == std::vector<ErrorTile>{{{-2, -2, 12, 12}, tech::kCheckPaint}});
  CHECK(drc::test::checks(mid) == std::vector<ErrorTile>{{{-2, -2, 17, 12}, tech::kCheckPaint}});
  CHECK(drc::test::checks(top) == std::vector<ErrorTile>{{{98, 98, 117, 112}, tech::kCheckPaint}});
  const auto pending = s.ed.drc.pending.cells();
  REQUIRE(pending.size() == 3);
  CHECK(pending[0] == &leaf);
  CHECK(pending[1] == &mid);
  CHECK(pending[2] == &top);
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")) | tech::TypeMask::of(s.type("metal1")));
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(drc::test::checks(leaf).empty());
  CHECK(drc::test::checks(mid).empty());
  CHECK(drc::test::checks(top).empty());
  CHECK(drc::test::errors(leaf) == std::vector<ErrorTile>{{{0, 0, 10, 10}, tech::kErrorP}});
}

TEST_CASE("Forced recheck: a cell that cannot be read marks nothing", "[cap:drc-engine][req:Forced recheck of an area]") {
  Session s;
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& top = s.cell("top");
  s.paint(leaf, "poly", db::Rect{0, 0, 10, 10});
  s.place(top, leaf, db::kIdentity);
  leaf.flags().available = false;
  const auto st = drc::force_recheck(s.ed, top, top.bbox(), [](db::CellDef&) { return false; });
  REQUIRE_FALSE(st.ok());
  CHECK(st.error().code == "DRC-ENG-001");
  CHECK(st.error().text == "Failure to read in entire subtree of cell.\nFailed on cell leaf.");
  CHECK(drc::test::checks(top).empty());
  CHECK(s.ed.drc.pending.empty());
  // A reader that succeeds lets the recheck go on.
  int reads = 0;
  REQUIRE(drc::force_recheck(s.ed, top, top.bbox(), [&](db::CellDef& d) {
            d.flags().available = true;
            return ++reads > 0;
          }).ok());
  CHECK(reads == 1);
  CHECK(s.ed.drc.pending.size() == 2);
}

TEST_CASE("Flat check count", "[cap:drc-engine][req:Flat check]") {
  Session s;
  s.set_rules(s.rules(2, 1000).build());
  db::CellDef& c = s.cell("c");
  s.paint(c, "poly", db::Rect{0, 0, 1, 10});
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  const auto n = drc::flat_check(s.ed, c, c.bbox());
  REQUIRE(n.ok());
  CHECK(n.value() == 1);
  CHECK(drc::test::errors(c).empty());  // no error plane changes
  REQUIRE(rec.calls().size() == 1);
  CHECK(rec.calls()[0].flattened);
  CHECK(rec.calls()[0].check == db::Rect{-2, -2, 3, 12});
  CHECK(rec.calls()[0].clip == db::Rect{0, 0, 1, 10});
}

TEST_CASE("Flat check chunks of 300 from the lower-left corner", "[cap:drc-engine][req:Flat check]") {
  Session s;
  s.set_rules(s.rules(1, 1000).build());
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& top = s.cell("top");
  s.paint(leaf, "poly", db::Rect{0, 0, 700, 10});
  s.place(top, leaf, db::translation(0, 0));
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  const auto n = drc::flat_check(s.ed, top, db::Rect{0, 0, 700, 10});
  REQUIRE(n.ok());
  CHECK(n.value() == 3);  // the child's paint is flattened into each chunk
  std::vector<db::Rect> clips;
  for (const auto& call : rec.calls()) {
    clips.push_back(call.clip);
  }
  CHECK(clips == std::vector<db::Rect>{{0, 0, 300, 10}, {300, 0, 600, 10}, {600, 0, 700, 10}});
}
