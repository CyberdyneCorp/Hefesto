// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-hints: the hint region (data model), the DRC inside test in both
// magic-compat modes (Q-DRC-11, Q-MISC-13) and the M5 library operations
// of design D10.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/hints/hints.hpp"
#include "hefesto/hints/region.hpp"

namespace db = hefesto::db;
namespace hints = hefesto::hints;
using db::Rect;
using Rects = std::vector<Rect>;

#define MODEL "[cap:mask-hints][req:Mask-hint property data model]"
#define SCOPE "[cap:mask-hints][req:Mask hints scope DRC exception and exemption rules]"

TEST_CASE("overlapping hint rectangles merge into maximal horizontal strips", MODEL) {
  const hints::HintRegion region{Rect{0, 0, 10, 10}, Rect{5, 5, 15, 15}};
  CHECK(region.strips() == Rects{{5, 10, 15, 15}, {0, 5, 15, 10}, {0, 0, 10, 5}});
  CHECK(region.bounding_box() == Rect{0, 0, 15, 15});
  // The decomposition is canonical: equal regions compare equal however
  // they were painted.
  hints::HintRegion other;
  other.paint(Rect{5, 5, 15, 15});
  other.paint(Rect{0, 0, 10, 10});
  CHECK(other == region);
  hints::HintRegion abutting;
  abutting.paint(Rect{0, 0, 10, 10});
  abutting.paint(Rect{10, 0, 20, 10});
  CHECK(abutting.strips() == Rects{{0, 0, 20, 10}});
}

TEST_CASE("hint rectangles without area contribute nothing", MODEL) {
  CHECK(hints::HintRegion{Rect{0, 0, 0, 5}}.empty());
  CHECK(hints::HintRegion{Rect{10, 10, 0, 0}}.empty());  // inverted: DBPaintPlane paints nothing
  CHECK(hints::HintRegion{Rect{0, 0, 5, 5}, Rect{8, 0, 4, 5}}.strips() == Rects{{0, 0, 5, 5}});
  const hints::HintRegion none;
  CHECK(none.plane() == nullptr);
  CHECK(none.bounding_box().is_null());
  // A non-empty region carries its plane; copies share it.
  const hints::HintRegion one{Rect{0, 0, 1, 1}};
  const hints::HintRegion copy = one;  // NOLINT(performance-unnecessary-copy-initialization)
  REQUIRE(one.plane() != nullptr);
  CHECK(copy.plane() == one.plane());
}

TEST_CASE("an empty hint value removes the property", MODEL) {
  db::CellDef cell{"c", 8};
  db::set_property(cell, "MASKHINTS_A", db::PropertyValue{hints::HintRegion{Rect{0, 0, 4, 4}}});
  REQUIRE(hints::find_hint(cell, "A") != nullptr);
  REQUIRE(db::change_property(cell, "MASKHINTS_A", std::nullopt));
  CHECK(hints::find_hint(cell, "A") == nullptr);
  CHECK(cell.properties().find("MASKHINTS_A") == nullptr);
}

TEST_CASE("hint regions are the plane values of the property store", MODEL) {
  const db::PropertyValue value = db::coerce_reserved_property("MASKHINTS_SRAM", std::string{"0 0 10 10 5 5 15 15"});
  const auto* region = std::get_if<hints::HintRegion>(&value);
  REQUIRE(region != nullptr);
  CHECK(db::property_text(value) == "5 10 15 15 0 5 15 10 0 0 10 5");
  // An inverted group paints nothing here either.
  const db::PropertyValue inverted = db::coerce_reserved_property("MASKHINTS_X", std::string{"4 4 0 0"});
  REQUIRE(std::holds_alternative<hints::HintRegion>(inverted));
  CHECK(std::get<hints::HintRegion>(inverted).empty());
}

TEST_CASE("hint names and property keys", MODEL) {
  CHECK(hints::hint_name("MASKHINTS_SRAM") == "SRAM");
  CHECK(hints::hint_name("MASKHINTS_") == "");
  CHECK_FALSE(hints::hint_name("SRAM").has_value());
  CHECK_FALSE(hints::hint_name("maskhints_SRAM").has_value());
  CHECK(hints::property_key("SRAM") == "MASKHINTS_SRAM");
}

TEST_CASE("hint values parse as %d groups of four", "[cap:mask-hints][req:Mask hints in .mag files]") {
  const auto parsed = hints::parse_hint_value("0 0 4 4 6 6 8 8");
  REQUIRE(parsed.ok());
  CHECK_FALSE(parsed.value().truncated);
  CHECK(hints::format_hint_value(parsed.value().region) == "6 6 8 8 0 0 4 4");
  // Trailing values are dropped and flagged.
  const auto partial = hints::parse_hint_value("0 0 10 10 20 20");
  REQUIRE(partial.ok());
  CHECK(partial.value().truncated);
  CHECK(partial.value().region.strips() == Rects{{0, 0, 10, 10}});
  // sscanf("%d") reads a token as far as it is an integer.
  const auto leading = hints::parse_hint_value("0 +0 4.5 4x");
  REQUIRE(leading.ok());
  CHECK(leading.value().region.strips() == Rects{{0, 0, 4, 4}});
  // A token that does not start with an integer fails with the rest of the text.
  const auto bad = hints::parse_hint_value("0 0 abc 9");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().text == "Mask-hint \"abc 9\" has non-integer values!");
  // Scale up, then down, truncating toward zero (as Magic's int division).
  CHECK(hints::parse_hint_value("0 0 9 9", 1, 2).value().region.strips() == Rects{{0, 0, 4, 4}});
  CHECK(hints::parse_hint_value("-9 -9 3 3", 2, 4).value().region.strips() == Rects{{-4, -4, 1, 1}});
}

TEST_CASE("hint regions transform, translate, scale, clip and subtract",
          "[cap:mask-hints][req:Copying, flattening and transforming hints]") {
  const hints::HintRegion child{Rect{0, 0, 10, 20}};
  // (x, y) -> (-y + 100, x): "Flatten in place with a rotated instance".
  CHECK(child.transformed(db::Transform{0, -1, 100, 1, 0, 0}).strips() == Rects{{80, 0, 100, 10}});
  CHECK(child.translated(1, 2).strips() == Rects{{1, 2, 11, 22}});
  CHECK(child.clipped(Rect{5, 5, 50, 50}).strips() == Rects{{5, 5, 10, 20}});
  // "Grid rescale": 2/1.
  CHECK(hints::HintRegion{Rect{0, 0, 5, 5}}.scaled(2, 1).strips() == Rects{{0, 0, 10, 10}});
  // DBScaleValue rounds down, also below zero; strips scaled to nothing vanish.
  CHECK(hints::HintRegion{Rect{-3, -3, 3, 3}}.scaled(1, 2).strips() == Rects{{-2, -2, 1, 1}});
  CHECK(hints::HintRegion{Rect{0, 0, 1, 1}}.scaled(1, 2).empty());
  CHECK(hints::HintRegion{Rect{0, 0, 10, 10}}.minus(hints::HintRegion{Rect{0, 0, 4, 4}}).strips() ==
        Rects{{0, 4, 10, 10}, {4, 0, 10, 4}});
}

namespace {

// The hint of the spec scenarios plus a metal1 wire's edges.
hints::Segment vertical(db::Coord x, db::Coord lo, db::Coord hi) {
  return hints::Segment{true, x, lo, hi};
}
hints::Segment horizontal(db::Coord y, db::Coord lo, db::Coord hi) {
  return hints::Segment{false, y, lo, hi};
}

}  // namespace

TEST_CASE("exception scope: an edge crossing the hint is inside, a far one is not", SCOPE) {
  // "Exception rule only for edges inside the hint": hint (-5,10)-(5,30),
  // wire A (0,0)-(2,40), wire B (100,0)-(102,40); same in both modes.
  const hints::HintRegion hint{Rect{-5, 10, 5, 30}};
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    CHECK(hints::inside(hint, vertical(0, 0, 40), compat));        // left edge of A
    CHECK_FALSE(hints::inside(hint, horizontal(0, 0, 2), compat));  // bottom edge of A
    CHECK_FALSE(hints::inside(hint, vertical(100, 0, 40), compat));
    CHECK_FALSE(hints::inside(hint, horizontal(0, 100, 102), compat));
    CHECK_FALSE(hints::inside(hints::HintRegion{}, vertical(0, 0, 40), compat));
  }
}

TEST_CASE("exception scope: hint touching a bottom edge from below with magic-compat ON", SCOPE) {
  const hints::HintRegion hint{Rect{-10, -10, 30, 0}};
  CHECK_FALSE(hints::inside(hint, horizontal(0, 0, 20), true));
}

TEST_CASE("exception scope: hint touching a bottom edge from below with magic-compat OFF", SCOPE) {
  const hints::HintRegion hint{Rect{-10, -10, 30, 0}};
  CHECK(hints::inside(hint, horizontal(0, 0, 20), false));
}

TEST_CASE("exception scope: an unrelated hint strip changes the result with magic-compat ON", SCOPE) {
  const hints::HintRegion straddling{Rect{5, -5, 15, 5}};
  CHECK(hints::inside(straddling, horizontal(0, 0, 20), true));
  // The far rectangle cuts the strips at y = 0, so the tile holding (0, -1) ends there.
  hints::HintRegion with_far = straddling;
  with_far.paint(Rect{-100, 0, -90, 10});
  CHECK_FALSE(hints::inside(with_far, horizontal(0, 0, 20), true));
}

TEST_CASE("exception scope: an unrelated hint strip has no effect with magic-compat OFF", SCOPE) {
  const hints::HintRegion straddling{Rect{5, -5, 15, 5}};
  hints::HintRegion with_far = straddling;
  with_far.paint(Rect{-100, 0, -90, 10});
  CHECK(hints::inside(straddling, horizontal(0, 0, 20), false));
  CHECK(hints::inside(with_far, horizontal(0, 0, 20), false));
}

TEST_CASE("exception scope: touching sides with magic-compat ON follow Magic's tile walk", SCOPE) {
  // Vertical edge x = 0, y 0..10.
  CHECK(hints::inside(hints::HintRegion{Rect{0, 0, 5, 10}}, vertical(0, 0, 10), true));     // from the right
  CHECK_FALSE(hints::inside(hints::HintRegion{Rect{-5, 0, 0, 10}}, vertical(0, 0, 10), true));  // from the left
  CHECK(hints::inside(hints::HintRegion{Rect{-5, 0, 5, 10}}, vertical(0, 0, 10), true));    // straddling
  CHECK(hints::inside(hints::HintRegion{Rect{0, 9, 5, 20}}, vertical(0, 0, 10), true));     // partly
  // Horizontal edge y = 0, x 0..20: a hint above never counts.
  CHECK_FALSE(hints::inside(hints::HintRegion{Rect{0, 0, 20, 5}}, horizontal(0, 0, 20), true));
  CHECK(hints::inside(hints::HintRegion{Rect{-5, -5, 25, 5}}, horizontal(0, 0, 20), true));  // straddling
  // L-shaped hint covering the edge whose strips break at y = 0 at x = 0.
  const hints::HintRegion l_shape{Rect{-5, -5, 25, 0}, Rect{5, 0, 25, 5}};
  CHECK_FALSE(hints::inside(l_shape, horizontal(0, 0, 20), true));
}

TEST_CASE("exception scope: touching sides with magic-compat OFF use the closed region", SCOPE) {
  CHECK(hints::inside(hints::HintRegion{Rect{0, 0, 5, 10}}, vertical(0, 0, 10), false));
  CHECK(hints::inside(hints::HintRegion{Rect{-5, 0, 0, 10}}, vertical(0, 0, 10), false));
  CHECK(hints::inside(hints::HintRegion{Rect{0, 0, 20, 5}}, horizontal(0, 0, 20), false));
  const hints::HintRegion l_shape{Rect{-5, -5, 25, 0}, Rect{5, 0, 25, 5}};
  CHECK(hints::inside(l_shape, horizontal(0, 0, 20), false));
  // Touching at a single point is not a part of positive length.
  CHECK_FALSE(hints::inside(hints::HintRegion{Rect{-5, 10, 0, 20}}, vertical(0, 0, 10), false));
  CHECK_FALSE(hints::inside(hints::HintRegion{Rect{20, -5, 30, 0}}, horizontal(0, 0, 20), false));
  CHECK_FALSE(hints::inside(hints::HintRegion{Rect{1, 0, 5, 10}}, vertical(0, 0, 10), false));
}

TEST_CASE("mask-hints output operator scales the hint to output units",
          "[cap:mask-hints][req:Mask-hint output operator]") {
  db::CellDef cell{"c", 8};
  hints::add_input_region(cell, "SRAM", hints::HintRegion{Rect{0, 0, 10, 10}});
  // "Hint adds geometry to an output layer": scalefactor 10.
  CHECK(hints::output_region(cell, "SRAM", 10).strips() == Rects{{0, 0, 100, 100}});
  CHECK(hints::output_region(cell, "NONE", 10).empty());
  // "Hint combined with paint": the caller's union with m1 (0,0)-(4,4).
  hints::HintRegion layer{Rect{0, 0, 4, 4}};
  hints::add_input_region(cell, "MET1", hints::HintRegion{Rect{4, 0, 6, 4}});
  layer.paint(hints::output_region(cell, "MET1", 1));
  CHECK(layer.strips() == Rects{{0, 0, 6, 4}});
}

TEST_CASE("cifinput templayer hints union into the prefixed property",
          "[cap:mask-hints][req:Mask hints from cifinput templayers]") {
  db::CellDef cell{"c", 8};
  db::set_property(cell, "SRAM", db::PropertyValue{std::string{"bare"}});  // never consulted (Q-MASK-308)
  hints::add_input_region(cell, "SRAM", hints::HintRegion{Rect{0, 0, 100, 100}});
  hints::add_input_region(cell, "SRAM", hints::HintRegion{Rect{100, 0, 200, 50}});
  const hints::HintRegion* region = hints::find_hint(cell, "SRAM");
  REQUIRE(region != nullptr);
  CHECK(*region == hints::HintRegion{Rect{0, 0, 100, 100}, Rect{100, 0, 200, 50}});
  CHECK(*cell.properties().find("SRAM") == db::PropertyValue{std::string{"bare"}});
  // A templayer without geometry creates nothing and changes nothing.
  hints::add_input_region(cell, "OTHER", hints::HintRegion{});
  CHECK(hints::find_hint(cell, "OTHER") == nullptr);
  hints::add_input_region(cell, "SRAM", hints::HintRegion{});
  CHECK(*hints::find_hint(cell, "SRAM") == *region);
}

TEST_CASE("gds maskhints keeps the input geometry output generation does not reproduce",
          "[cap:mask-hints][req:Mask hints generated by gds maskhints]") {
  // "Unreproducible input geometry becomes a hint": input MET1 (0,0)-(60,40),
  // generated MET1 from metal1 (0,0)-(4,4) at scalefactor 10.
  const hints::HintRegion input{Rect{0, 0, 60, 40}};
  const hints::HintRegion generated{Rect{0, 0, 40, 40}};
  const hints::HintRegion h = hints::gds_maskhints_region(input, generated, 10);
  CHECK(h.strips() == Rects{{4, 0, 6, 4}});
  // Writing again reproduces the input.
  hints::HintRegion rewritten = generated;
  db::CellDef cell{"c", 8};
  hints::add_input_region(cell, "MET1", h);
  rewritten.paint(hints::output_region(cell, "MET1", 10));
  CHECK(rewritten == input);
  // Coordinates are divided truncating toward zero; fully generated input gives nothing.
  CHECK(hints::gds_maskhints_region(hints::HintRegion{Rect{-15, -15, 15, 15}}, {}, 10).strips() ==
        Rects{{-1, -1, 1, 1}});
  CHECK(hints::gds_maskhints_region(input, input, 10).empty());
}
