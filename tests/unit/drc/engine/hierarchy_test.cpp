// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-engine: "Interaction area", "Flattening for interaction checks",
// "Copying subcell errors", "Array checks", "Exact-overlap check".

#include <catch2/catch_test_macros.hpp>

#include "engine_fixture.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::ErrorTile;
using drc::test::Session;

namespace {

/// Marks the whole bounding box of `def` (not its children) and checks it.
void check_all(Session& s, db::CellDef& def) {
  drc::check_this(s.ed, def, def.bbox());
  REQUIRE(drc::catchup(s.ed).ok());
}

}  // namespace

TEST_CASE("Subcell far from paint", "[cap:drc-engine][req:Interaction area]") {
  Session s;
  db::CellDef& child = s.cell("child");
  db::CellDef& parent = s.cell("parent");
  s.paint(child, "poly", db::Rect{0, 0, 10, 10});
  s.place(parent, child, db::kIdentity);
  s.paint(parent, "poly", db::Rect{100, 0, 110, 10});
  const auto area = drc::interaction_area(s.ed, parent, db::Rect{-50, -50, 200, 50}, 5);
  REQUIRE(area.ok());
  CHECK_FALSE(area.value().has_value());
}

TEST_CASE("Interaction area of paint near a subcell and of two siblings", "[cap:drc-engine][req:Interaction area]") {
  Session s;
  db::CellDef& child = s.cell("child");
  db::CellDef& parent = s.cell("parent");
  s.paint(child, "poly", db::Rect{0, 0, 10, 10});

  SECTION("parent paint within the halo of the use") {
    s.place(parent, child, db::translation(100, 0));
    s.paint(parent, "poly", db::Rect{112, 0, 120, 10});
    const auto area = drc::interaction_area(s.ed, parent, db::Rect{0, -50, 300, 50}, 5);
    REQUIRE(area.ok());
    REQUIRE(area.value().has_value());
    // (P + h) clipped to the use's halo area: P = [112,0,120,10] -> [107,-5,115,15].
    CHECK(*area.value() == db::Rect{107, -5, 115, 15});
  }
  SECTION("replicated: a use without siblings adds GeoNullRect expanded by the halo") {
    s.place(parent, child, db::kIdentity);
    s.paint(parent, "poly", db::Rect{12, 0, 20, 10});
    const auto area = drc::interaction_area(s.ed, parent, db::Rect{-50, -50, 200, 50}, 5);
    REQUIRE(area.ok());
    REQUIRE(area.value().has_value());
    CHECK(*area.value() == db::Rect{-5, -5, 15, 15});  // [7,-5,15,15] joined by [-5,-5,5,5]
  }
  SECTION("two siblings closer than the halo") {
    s.place(parent, child, db::kIdentity);
    s.place(parent, child, db::translation(11, 0));
    const auto area = drc::interaction_area(s.ed, parent, db::Rect{-50, -50, 200, 50}, 2);
    REQUIRE(area.ok());
    REQUIRE(area.value().has_value());
    CHECK(*area.value() == db::Rect{9, -2, 12, 12});
  }
  SECTION("two siblings farther apart than the halo do not interact") {
    s.place(parent, child, db::kIdentity);
    s.place(parent, child, db::translation(13, 0));
    const auto area = drc::interaction_area(s.ed, parent, db::Rect{-50, -50, 200, 50}, 2);
    REQUIRE(area.ok());
    CHECK_FALSE(area.value().has_value());
  }
  SECTION("no subcell near the area") {
    s.place(parent, child, db::kIdentity);
    const auto area = drc::interaction_area(s.ed, parent, db::Rect{100, 100, 200, 200}, 2);
    REQUIRE(area.ok());
    CHECK_FALSE(area.value().has_value());
  }
}

TEST_CASE("Illegal overlap between cells", "[cap:drc-engine][req:Flattening for interaction checks]") {
  Session s;
  const tech::TileType poly = s.type("poly");
  const tech::TileType ndiff = s.type("ndiff");
  const tech::PlaneId active = *s.tech->home_plane(poly);
  drc::RuleSetBuilder b = s.rules(1, 1000);
  db::PaintRow poly_row = drc::identity_paint_row(poly);
  db::PaintRow ndiff_row = drc::identity_paint_row(ndiff);
  poly_row[ndiff] = tech::kErrorS;
  ndiff_row[poly] = tech::kErrorS;
  b.paint_row(active, poly, poly_row).paint_row(active, ndiff, ndiff_row);
  s.set_rules(b.build());
  const drc::test::Recorder rec;
  db::CellDef& child = s.cell("child");
  db::CellDef& parent = s.cell("parent");
  s.paint(child, "ndiff", db::Rect{5, -5, 15, 5});
  s.paint(parent, "poly", db::Rect{0, 0, 10, 2});
  s.place(parent, child, db::kIdentity);
  drc::check_this(s.ed, parent, parent.bbox());
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(drc::test::errors(parent) == std::vector<ErrorTile>{{{5, 0, 10, 2}, tech::kErrorS}});
  // The check ran on the flattened buffer, never listed in the library.
  bool flattened = false;
  for (const auto& call : rec.calls()) {
    flattened = flattened || call.flattened;
  }
  CHECK(flattened);
  CHECK(s.ed.cells.find("__DRC__yank") == nullptr);
}

TEST_CASE("Child error appears in parent", "[cap:drc-engine][req:Copying subcell errors]") {
  Session s;
  db::CellDef& child = s.cell("child");
  db::CellDef& parent = s.cell("parent");
  s.paint(child, "poly", db::Rect{0, 0, 2, 10});
  REQUIRE(child.plane(tech::kPlaneDrcError).paint(db::Rect{0, 0, 2, 10}, drc::error_paint_row(tech::kErrorP)).ok());
  s.place(parent, child, db::translation(50, 50));
  check_all(s, parent);
  CHECK(drc::test::errors(parent) == std::vector<ErrorTile>{{{50, 50, 52, 60}, tech::kErrorP}});
}

TEST_CASE("Copied errors: every error type, one level, each array element",
          "[cap:drc-engine][req:Copying subcell errors]") {
  Session s;
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& mid = s.cell("mid");
  db::CellDef& top = s.cell("top");
  s.paint(leaf, "poly", db::Rect{0, 0, 4, 4});
  REQUIRE(leaf.plane(tech::kPlaneDrcError).paint(db::Rect{0, 0, 1, 1}, drc::error_paint_row(tech::kErrorS)).ok());
  s.place(mid, leaf, db::kIdentity);
  db::ArrayInfo a;
  a.xhi = 1;
  a.xsep = 100;
  s.place(top, mid, db::kIdentity, a);
  // mid has not been checked: its error plane is empty, so top copies nothing
  // from the leaf (one level only).
  check_all(s, top);
  CHECK(drc::test::errors(top).empty());
  REQUIRE(mid.plane(tech::kPlaneDrcError).paint(db::Rect{2, 2, 3, 3}, drc::error_paint_row(tech::kErrorPS)).ok());
  check_all(s, top);
  CHECK(drc::test::errors(top) == std::vector<ErrorTile>{{{2, 2, 3, 3}, tech::kErrorP}, {{102, 2, 103, 3}, tech::kErrorP}});
}

TEST_CASE("Abutting array elements", "[cap:drc-engine][req:Array checks]") {
  Session s;
  const tech::TileType m1 = s.type("metal1");
  s.set_rules(s.rules(2, 1000).build());
  db::CellDef& cell = s.cell("cell");
  db::CellDef& parent = s.cell("parent");
  s.paint(cell, "metal1", db::Rect{1, 0, 2, 10});
  s.paint(cell, "metal1", db::Rect{9, 0, 10, 10});
  REQUIRE(cell.bbox() == db::Rect{1, 0, 10, 10});
  s.paint(cell, "poly", db::Rect{0, 0, 1, 1});  // the bounding box is [0,10]^2
  REQUIRE(cell.bbox() == db::Rect{0, 0, 10, 10});
  db::ArrayInfo a;
  a.xhi = 1;
  a.xsep = 10;
  s.place(parent, cell, db::kIdentity, a);
  const drc::test::Recorder rec(tech::TypeMask::of(m1));
  drc::check_this(s.ed, parent, parent.bbox());
  REQUIRE(drc::catchup(s.ed).ok());
  std::vector<db::Rect> regions;
  for (const auto& call : rec.calls()) {
    if (call.flattened) {
      regions.push_back(call.clip);
      CHECK(call.check == db::expand(call.clip, 2));
    }
  }
  CHECK(regions == std::vector<db::Rect>{{8, 0, 12, 10}, {8, 8, 12, 10}});
  const auto errs = drc::test::errors(parent);
  CHECK(std::find(errs.begin(), errs.end(), ErrorTile{{11, 0, 12, 10}, tech::kErrorP}) != errs.end());
  for (const ErrorTile& e : errs) {
    CHECK(e.type == tech::kErrorP);
  }
}

TEST_CASE("Array regions A and C for a column array", "[cap:drc-engine][req:Array checks]") {
  Session s;
  s.set_rules(s.rules(2, 1000).build());
  db::CellDef& cell = s.cell("cell");
  db::CellDef& parent = s.cell("parent");
  s.paint(cell, "metal1", db::Rect{0, 0, 10, 10});
  db::ArrayInfo a;
  a.yhi = 2;
  a.ysep = 9;  // overlapping rows
  s.place(parent, cell, db::kIdentity, a);
  const drc::test::Recorder rec;
  drc::check_this(s.ed, parent, parent.bbox());
  REQUIRE(drc::catchup(s.ed).ok());
  std::vector<db::Rect> regions;
  for (const auto& call : rec.calls()) {
    if (call.flattened) {
      regions.push_back(call.clip);
    }
  }
  // ysep 9 < ysize 10 + h: A = [0, 7, 12, 12], C = [8, 7, 10, 12]; the
  // array is one column wide, so B and D (xsep = h + 10) are not checked.
  CHECK(regions == std::vector<db::Rect>{{0, 7, 12, 12}, {8, 7, 10, 12}});
}

TEST_CASE("Partial contact overlap", "[cap:drc-engine][req:Exact-overlap check]") {
  Session s;
  const tech::TileType m2c = s.type("m2c");
  s.set_rules(s.rules(1, 1000).exact_overlap_types(tech::TypeMask::of(m2c)).build());
  db::CellDef& a = s.cell("a");
  db::CellDef& b = s.cell("b");
  db::CellDef& parent = s.cell("parent");
  s.paint(a, "m2c", db::Rect{0, 0, 4, 4});
  s.paint(b, "m2c", db::Rect{0, 0, 4, 4});
  s.place(parent, a, db::kIdentity);

  SECTION("a partial overlap is an error with why 3") {
    s.place(parent, b, db::translation(2, 0));
    const drc::test::Recorder rec;
    drc::CollectingErrorSink sink;
    (void)drc::interaction_check(s.ed, parent, db::Rect{-500, -500, 500, 500}, parent.bbox(), sink);
    REQUIRE_FALSE(sink.errors().empty());
    for (const auto& e : sink.errors()) {
      CHECK(e.rule.why == drc::why::kSubcellOverlap);
      CHECK(db::surround(db::Rect{0, 0, 6, 4}, e.area));
    }
    drc::check_this(s.ed, parent, parent.bbox());
    REQUIRE(drc::catchup(s.ed).ok());
    for (const ErrorTile& e : drc::test::errors(parent)) {
      CHECK(e.type == tech::kErrorS);
    }
    CHECK_FALSE(drc::test::errors(parent).empty());
  }
  SECTION("an exact overlap is legal") {
    s.place(parent, b, db::kIdentity);
    const drc::test::Recorder rec;
    drc::CollectingErrorSink sink;
    (void)drc::interaction_check(s.ed, parent, db::Rect{-500, -500, 500, 500}, parent.bbox(), sink);
    CHECK(sink.errors().empty());
  }
}
