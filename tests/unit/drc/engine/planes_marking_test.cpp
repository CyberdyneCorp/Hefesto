// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-engine: "DRC check and error planes", "Marking changed areas",
// "Halo override".

#include <catch2/catch_test_macros.hpp>

#include "engine_fixture.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::ErrorTile;
using drc::test::Session;

TEST_CASE("Error type composition", "[cap:drc-engine][req:DRC check and error planes]") {
  db::Plane plane;
  REQUIRE(plane.paint(db::Rect{0, 0, 10, 10}, drc::error_paint_row(tech::kErrorS)).ok());
  REQUIRE(plane.paint(db::Rect{5, 0, 15, 10}, drc::error_paint_row(tech::kErrorP)).ok());
  std::vector<ErrorTile> got;
  plane.enumerate(db::whole_plane(), ~tech::TypeMask::of(tech::kSpace), [&](const db::TileView& t) {
    got.push_back(ErrorTile{t.area, t.type});
    return true;
  });
  CHECK(got == std::vector<ErrorTile>{{{0, 0, 5, 10}, tech::kErrorS},
                                      {{5, 0, 10, 10}, tech::kErrorPS},
                                      {{10, 0, 15, 10}, tech::kErrorP}});

  SECTION("erasing P from PS leaves S, erasing S from PS leaves P") {
    CHECK(drc::error_erase_row(tech::kErrorP)[tech::kErrorPS] == tech::kErrorS);
    CHECK(drc::error_erase_row(tech::kErrorS)[tech::kErrorPS] == tech::kErrorP);
    CHECK(drc::error_erase_row(tech::kErrorP)[tech::kErrorP] == tech::kSpace);
    CHECK(drc::error_paint_row(tech::kErrorS)[tech::kErrorP] == tech::kErrorPS);
  }
  SECTION("the engine's tables agree with the technology's built-in error-plane tables") {
    const auto t = drc::test::engine_tech();
    for (const tech::TileType paint : {tech::kErrorP, tech::kErrorS, tech::kErrorPS}) {
      for (const tech::TileType have : {tech::kSpace, tech::kErrorP, tech::kErrorS, tech::kErrorPS}) {
        CHECK(drc::error_paint_row(paint)[have] == db::paint_row(*t, tech::kPlaneDrcError, paint)[have]);
        CHECK(drc::error_erase_row(paint)[have] == db::erase_row(*t, tech::kPlaneDrcError, paint)[have]);
      }
    }
  }
}

TEST_CASE("The checker paints checkpaint only; a checksubcell area read from a file is still checked",
          "[cap:drc-engine][req:DRC check and error planes]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "poly", db::Rect{0, 0, 10, 10});
  drc::check_this(s.ed, c, db::Rect{0, 0, 10, 10});
  for (const ErrorTile& t : drc::test::checks(c)) {
    CHECK(t.type == tech::kCheckPaint);
  }
  db::CellDef& read = s.cell("read");
  drc::test::paint_check(read, *s.tech, db::Rect{0, 0, 20, 20}, tech::kCheckSubcell);
  drc::check_this(s.ed, read, std::nullopt);  // the cell is enqueued (a later mark or `drc check`)
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(drc::test::checks(read).empty());
  CHECK(drc::test::checks(c).empty());
}

TEST_CASE("Error tiles are written and kept in the error plane only",
          "[cap:drc-engine][req:DRC check and error planes]") {
  Session s;
  const drc::test::Recorder rec(tech::TypeMask::of(s.type("poly")));
  db::CellDef& c = s.cell("c");
  s.paint(c, "poly", db::Rect{0, 0, 4, 4});
  drc::check_this(s.ed, c, db::Rect{0, 0, 4, 4});
  REQUIRE(drc::catchup(s.ed).ok());
  CHECK(drc::test::errors(c) == std::vector<ErrorTile>{{{0, 0, 4, 4}, tech::kErrorP}});
  // The paint planes are untouched.
  CHECK(drc::test::tiles(c, *s.tech->home_plane(s.type("poly")), ~tech::TypeMask::of(tech::kSpace)) ==
        std::vector<ErrorTile>{{{0, 0, 4, 4}, s.type("poly")}});
}

TEST_CASE("Two-level propagation", "[cap:drc-engine][req:Marking changed areas]") {
  Session s;
  s.set_rules(s.rules(3, 1000).build());
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& top = s.cell("top");
  s.paint(leaf, "poly", db::Rect{0, 0, 10, 10});
  s.place(top, leaf, db::translation(100, 0));
  drc::check_this(s.ed, leaf, db::Rect{0, 0, 10, 10});
  CHECK(drc::test::checks(leaf) == std::vector<ErrorTile>{{{-3, -3, 13, 13}, tech::kCheckPaint}});
  CHECK(drc::test::checks(top) == std::vector<ErrorTile>{{{97, -3, 113, 13}, tech::kCheckPaint}});
  const auto pending = s.ed.drc.pending.cells();
  REQUIRE(pending.size() == 2);
  CHECK(pending[0] == &leaf);
  CHECK(pending[1] == &top);
}

TEST_CASE("Marking: arrays, windows, internal cells and no area", "[cap:drc-engine][req:Marking changed areas]") {
  Session s;
  s.set_rules(s.rules(1, 1000).build());
  db::CellDef& leaf = s.cell("leaf");
  db::CellDef& top = s.cell("top");
  s.paint(leaf, "poly", db::Rect{0, 0, 10, 10});

  SECTION("an array use marks the bounding box of every element image") {
    db::ArrayInfo a;
    a.xlo = 0;
    a.xhi = 2;
    a.ylo = 0;
    a.yhi = 1;
    a.xsep = 20;
    a.ysep = 30;
    s.place(top, leaf, db::kIdentity, a);
    drc::check_this(s.ed, leaf, db::Rect{0, 0, 10, 10});
    CHECK(drc::test::checks(top) == std::vector<ErrorTile>{{{-1, -1, 51, 41}, tech::kCheckPaint}});
  }
  SECTION("without an area only the cell is enqueued") {
    s.place(top, leaf, db::kIdentity);
    const auto before = s.ed.drc.worker->generation();
    drc::check_this(s.ed, leaf, std::nullopt);
    CHECK(drc::test::checks(leaf).empty());
    CHECK(drc::test::checks(top).empty());
    REQUIRE(s.ed.drc.pending.size() == 1);
    CHECK(s.ed.drc.pending.front() == &leaf);
    CHECK(s.ed.drc.worker->generation() == before + 1);
  }
  SECTION("internal cells are never marked") {
    leaf.flags().internal = true;
    drc::check_this(s.ed, leaf, db::Rect{0, 0, 10, 10});
    CHECK(drc::test::checks(leaf).empty());
    CHECK(s.ed.drc.pending.empty());
  }
  SECTION("each level re-expands by the halo") {
    db::CellDef& mid = s.cell("mid");
    s.place(mid, leaf, db::kIdentity);
    s.place(top, mid, db::translation(0, 50));
    drc::check_this(s.ed, leaf, db::Rect{0, 0, 10, 10});
    CHECK(drc::test::checks(mid) == std::vector<ErrorTile>{{{-1, -1, 11, 11}, tech::kCheckPaint}});
    CHECK(drc::test::checks(top) == std::vector<ErrorTile>{{{-1, 49, 11, 61}, tech::kCheckPaint}});
  }
}

TEST_CASE("Halo capped at style value", "[cap:drc-engine][req:Halo override]") {
  Session s;
  s.set_rules(s.rules(20, 1000).build());
  s.ed.drc.halo_override = 50;
  CHECK(drc::current_halo(s.ed) == 20);
  s.ed.drc.halo_override = 5;
  CHECK(drc::current_halo(s.ed) == 5);
  db::CellDef& c = s.cell("c");
  drc::check_this(s.ed, c, db::Rect{0, 0, 10, 10});
  CHECK(drc::test::checks(c) == std::vector<ErrorTile>{{{-5, -5, 15, 15}, tech::kCheckPaint}});
}

TEST_CASE("A reduced halo narrows the basic check of a square", "[cap:drc-engine][req:Halo override]") {
  Session s;
  s.set_rules(s.rules(20, 1000).build());
  s.ed.drc.halo_override = 2;
  const drc::test::Recorder rec;
  db::CellDef& c = s.cell("c");
  drc::test::paint_check(c, *s.tech, db::Rect{10, 10, 20, 20});
  drc::check_this(s.ed, c, std::nullopt);
  REQUIRE(drc::catchup(s.ed).ok());
  const auto calls = rec.calls();
  REQUIRE(calls.size() == 1);
  CHECK(calls[0].check == db::Rect{8, 8, 22, 22});
  CHECK(calls[0].clip == db::Rect{10, 10, 20, 20});
}
