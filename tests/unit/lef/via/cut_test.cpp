// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cut growing to the output style's contact size (capability lef-read "Cut
// growing", Q-LEFDEF-17; openspec/changes/m6-lef-def-oasis tasks.md 4.2),
// including the warning text of golden case LEFDEF-011.
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>

#include "hefesto/lef/via/cut.hpp"
#include "hefesto/lef/via/store.hpp"
#include "via_fixture.hpp"

namespace hefesto::lef {
namespace {

using test::Context;

TEST_CASE("a via cut grows to the magic contact size", "[cap:lef-read][req:Cut growing]") {
  // Scenario "Cut grown to magic contact size": F = 1, squares size 2 and
  // border 1: C = 8 and E = 4 half units.
  const auto technology = test::via_tech();
  const tech::TileType via = test::type_of(*technology, "m2contact");
  const mask::OutputStyle style = test::squares_style(via, 1, 1, 2, 2);
  Context c{technology, true, &style};
  CHECK(grow_cut(db::Rect{-2, -2, 2, 2}, via, via, c.via()) == db::Rect{-4, -4, 4, 4});
  CHECK(c.texts().empty());
}

TEST_CASE("cut growing rounds up and centres on the truncated midpoint", "[cap:lef-read][req:Cut growing]") {
  const auto technology = test::via_tech();
  const tech::TileType via = test::type_of(*technology, "m2contact");
  // F = 3: C = ceil(8/3) = 3, E = ceil(4/3) = 2.
  const mask::OutputStyle style = test::squares_style(via, 3, 1, 2, 2);
  Context c{technology, true, &style};
  CHECK(grow_cut(db::Rect{-1, -1, 1, 1}, via, via, c.via()) == db::Rect{-1, -1, 2, 2});
  CHECK(c.texts().empty());
  // Centre (5 + 0) / 2 truncates to 2; (-5 + 0) / 2 to -2.
  CHECK(grow_cut(db::Rect{0, -5, 5, 0}, via, via, c.via()) == db::Rect{1, -3, 4, 0});
  CHECK(c.texts().size() == 2);
}

TEST_CASE("cut growing warns with Magic's text when the cut size differs (LEFDEF-011)",
          "[cap:lef-read][req:Cut growing]") {
  const auto technology = test::via_tech();
  const tech::TileType via = test::type_of(*technology, "m2contact");
  const mask::OutputStyle style = test::squares_style(via, 1, 1, 2, 2);
  for (const bool compat : {true, false}) {
    // A LEF VIA with RECTs met1 then a 6 x 6 half-unit cut, smaller than the
    // magic contact (8 x 8).
    Context c{technology, compat, &style};
    ViaTable vias;
    ViaDefinition* entry = begin_via(vias, test::no_layers(), "VIA12", ViaSection::lef_via, c.via());
    add_via_rect(*entry, c.type("metal1"), db::Rect{-10, -10, 10, 10}, c.via());
    add_via_rect(*entry, via, db::Rect{-3, -3, 3, 3}, c.via());
    // magic-compat ON names the entry's normal type at that moment, as
    // Magic prints DBTypeLongNameTbl[lefl->type]; OFF names the cut's type.
    const std::string named = compat ? "metal1" : "m2contact";
    CHECK(c.texts() == std::vector<std::string>{
                           "Cut size for magic type \"" + named + "\" (4 x 4) does not match LEF/DEF",
                           "Via cut size (6 x 6).  Magic layer cut size will be used!"});
    CHECK(entry->type == via);
    CHECK(entry->area == db::Rect{-4, -4, 4, 4});
  }
}

TEST_CASE("cut growing leaves other rectangles alone", "[cap:lef-read][req:Cut growing]") {
  const auto technology = test::via_tech();
  const tech::TileType via = test::type_of(*technology, "m2contact");
  const tech::TileType m3c = test::type_of(*technology, "m3contact");
  const tech::TileType metal1 = test::type_of(*technology, "metal1");
  const mask::OutputStyle style = test::squares_style(via, 1, 1, 2, 2);
  const db::Rect cut{-1, -1, 1, 1};
  {
    Context c{technology, true, &style};
    CHECK(grow_cut(cut, metal1, metal1, c.via()) == cut);  // not a contact
    CHECK(grow_cut(cut, m3c, m3c, c.via()) == cut);        // the style makes no m3contact cut
  }
  Context none{technology, true, nullptr};
  CHECK(grow_cut(cut, via, via, none.via()) == cut);  // no output style
  CHECK(none.texts().empty());
  // An output style whose cut operator follows a `grow` does not count.
  mask::OutputStyle grown = style;
  mask::LayerOp grow;
  grow.kind = mask::OpKind::grow;
  grow.distance = 1;
  grown.layers[0].ops.insert(grown.layers[0].ops.begin() + 1, grow);
  Context c{technology, true, &grown};
  CHECK(grow_cut(cut, via, via, c.via()) == cut);
}

TEST_CASE("regression: cut growing at the coordinate limits saturates instead of overflowing",
          "[cap:lef-read][req:Cut growing]") {
  // LEF rectangles reach +-2^31 half units; the centre and edge sums
  // overflowed int32 (undefined behaviour, fixed in both modes).
  constexpr db::Coord kMax = std::numeric_limits<db::Coord>::max();
  constexpr db::Coord kMin = std::numeric_limits<db::Coord>::min();
  const auto technology = test::via_tech();
  const tech::TileType via = test::type_of(*technology, "m2contact");
  const mask::OutputStyle style = test::squares_style(via, 1, 1, 2, 2);  // C = 8
  for (const bool compat : {true, false}) {
    Context c{technology, compat, &style};
    CHECK(grow_cut(db::Rect{kMax - 1, kMax - 1, kMax, kMax}, via, via, c.via()) ==
          db::Rect{kMax - 5, kMax - 5, kMax, kMax});
    CHECK(grow_cut(db::Rect{kMin, kMin, kMin + 1, kMin + 1}, via, via, c.via()) ==
          db::Rect{kMin, kMin, kMin + 5, kMin + 5});
  }
}

}  // namespace
}  // namespace hefesto::lef
