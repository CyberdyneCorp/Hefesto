// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator statements (tasks.md 3.2): every operator with valid and invalid
// argument counts and values, the typed operands of LayerOp, the automatic
// stacked-contact inclusion and the squares-to-grid conversion.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "style_fixture.hpp"

namespace mask = hefesto::mask;
namespace tech = hefesto::tech;
using mask::OpKind;
using mask::test::read;

namespace {

/// The operators of layer A after `ops`, in a style with scale factor 2 and
/// expander 1: the unit normalisation keeps the values as written unless a
/// squares or slots size or separation is odd (which these tests avoid).
struct Parsed {
  mask::test::Read r;
  [[nodiscard]] const std::vector<mask::LayerOp>& ops() const { return r.style().layers.back().ops; }
};

Parsed parse(const std::string& ops) {
  return Parsed{read("style s\nscalefactor 2\ntemplayer T poly\nlayer A metal1\n" + ops + "\nbloat-all metal1 metal2")};
}

/// Parses `line` alone after the layer: the operator, or nothing, and the messages.
struct One {
  std::optional<mask::LayerOp> op;
  std::vector<std::string> messages;
};

One one(const std::string& line) {
  const Parsed p = parse(line);
  One out;
  // ops: the layer's own `or metal1`, the line's operator (if kept), bloat-all.
  if (p.ops().size() == 3) {
    out.op = p.ops()[1];
  }
  out.messages = p.r.texts();
  return out;
}

void check_rejected(const std::string& line, const std::string& message) {
  INFO(line);
  const One o = one(line);
  CHECK_FALSE(o.op.has_value());
  CHECK(o.messages == std::vector<std::string>{message});
}

std::string wrong(const std::string& word) {
  return "Wrong number of arguments in " + word + " statement.";
}

}  // namespace

TEST_CASE("boolean and interaction operators take one layer list",
          "[cap:mask-output-styles][req:Operator statement arguments]") {
  for (const std::string word : {"and", "and-not", "or", "overlapping", "nonoverlapping", "interacting",
                                 "noninteracting"}) {
    INFO(word);
    const One o = one(word + " T,poly");
    REQUIRE(o.op.has_value());
    CHECK(o.messages.empty());
    CHECK(o.op->cif_layers == std::vector<std::size_t>{0});
    check_rejected(word, wrong(word));
    check_rejected(word + " a b", wrong(word));
  }
  CHECK(one("and T").op->kind == OpKind::op_and);
  CHECK(one("and-not T").op->kind == OpKind::and_not);
  CHECK(one("overlapping T").op->interact == mask::InteractFlags{false, false});
  CHECK(one("nonoverlapping T").op->interact == mask::InteractFlags{true, false});
  CHECK(one("interacting T").op->interact == mask::InteractFlags{false, true});
  CHECK(one("noninteracting T").op->interact == mask::InteractFlags{true, true});
  CHECK(one("interacting T").op->kind == OpKind::interacting);
}

TEST_CASE("grow and shrink operators take one positive distance",
          "[cap:mask-output-styles][req:Operator statement arguments]") {
  const std::vector<std::pair<std::string, OpKind>> words{
      {"grow", OpKind::grow}, {"grow-min", OpKind::grow_min}, {"grow-grid", OpKind::grow_grid},
      {"shrink", OpKind::shrink}};
  for (const auto& [word, kind] : words) {
    INFO(word);
    const One o = one(word + " 7");
    REQUIRE(o.op.has_value());
    CHECK(o.op->kind == kind);
    CHECK(o.op->distance == 7);
    check_rejected(word + " 0", "Grow/shrink distance must be greater than zero.");
    check_rejected(word, wrong(word));
    check_rejected(word + " 1 2", wrong(word));
  }
}

TEST_CASE("close takes an optional positive area", "[cap:mask-output-styles][req:Operator statement arguments]") {
  CHECK(one("close").op->distance == 0);  // close every enclosed area
  CHECK(one("close 40").op->distance == 40);
  check_rejected("close -1", "Grow/shrink distance must be greater than zero.");
  check_rejected("close 1 2", wrong("close"));
}

TEST_CASE("bridge and bridge-lim", "[cap:mask-output-styles][req:Operator statement arguments]") {
  const One b = one("bridge 5 6");
  REQUIRE(b.op.has_value());
  CHECK(b.op->distance == 5);
  CHECK(b.op->values == std::vector<std::int32_t>{6});
  const One lim = one("bridge-lim 5 6 T");
  REQUIRE(lim.op.has_value());
  CHECK(lim.op->kind == OpKind::bridge_lim);
  CHECK(lim.op->cif_layers == std::vector<std::size_t>{0});
  check_rejected("bridge 5", wrong("bridge"));
  check_rejected("bridge 0 6", "Bridge distance must be greater than zero.");
  check_rejected("bridge 5 0", "Bridge width must be greater than zero.");
  check_rejected("bridge-lim 5 6", wrong("bridge-lim"));
  check_rejected("bridge-lim -5 6 T", "Bridge distance must be greater than zero.");
}

TEST_CASE("bloat-or, bloat-min and bloat-max", "[cap:mask-output-styles][req:Operator statement arguments]") {
  // Scenario "Bloat distances of source types zeroed".
  const One o = one("bloat-or pdiff,ndiff * 20");
  REQUIRE(o.op.has_value());
  CHECK(o.messages.empty());
  const Parsed p = parse("bloat-or pdiff,ndiff * 20");
  const auto pdiff = p.r.type("pdiff");
  const auto ndiff = p.r.type("ndiff");
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    const bool own = t == pdiff || t == ndiff;
    CHECK(o.op->bloat->distance[t] == (own ? 0 : 20));
  }
  CHECK(o.op->bloat->plane == static_cast<std::int32_t>(*p.r.tech->home_plane(ndiff)));  // `*` adds no plane
  // Pairs, the plane and the negative bloat-or distance.
  const One pairs = one("bloat-max ndiff poly 10 pdiff -3");
  REQUIRE(pairs.op.has_value());
  CHECK(pairs.op->bloat->distance[p.r.type("poly")] == 10);
  CHECK(pairs.op->bloat->distance[pdiff] == -3);
  CHECK(pairs.op->bloat->plane == static_cast<std::int32_t>(*p.r.tech->home_plane(ndiff)));
  const One negative = one("bloat-or ndiff poly -3");
  REQUIRE(negative.op.has_value());
  CHECK(negative.messages == std::vector<std::string>{"Bloat-or distances must not be negative."});
  CHECK(negative.op->bloat->distance[p.r.type("poly")] == 0);
  CHECK(one("bloat-min ndiff T 4").messages == std::vector<std::string>{"Can't use templayers in bloat statement."});
  CHECK(one("bloat-min ndiff metal2 4").messages ==
        std::vector<std::string>{"Not all bloat layers fall in the same plane."});
  check_rejected("bloat-or ndiff poly", wrong("bloat-or"));
  check_rejected("bloat-or ndiff poly 1 pdiff", wrong("bloat-or"));
}

TEST_CASE("bloat-all", "[cap:mask-output-styles][req:Operator statement arguments]") {
  const Parsed p = parse("bloat-all ndiff poly,pdiff 9");
  const mask::LayerOp& op = p.ops()[1];
  CHECK(op.kind == OpKind::bloat_all);
  CHECK(op.distance == 9);
  CHECK(op.bloat->distance[p.r.type("poly")] == 1);
  CHECK(op.bloat->distance[p.r.type("ndiff")] == 0);
  CHECK(op.bloat->plane >= 0);
  // Mask layers: distance 1 per layer, plane -1.
  const One layers = one("bloat-all T T");
  REQUIRE(layers.op.has_value());
  CHECK(layers.op->bloat->plane == -1);
  CHECK(layers.op->bloat->distance[0] == 1);
  CHECK(one("bloat-all ndiff T,poly").messages ==
        std::vector<std::string>{"Can't mix CIF and magic layers in bloat statement."});
  check_rejected("bloat-all ndiff", wrong("bloat-all"));
  check_rejected("bloat-all ndiff poly 0", "Bloat distance must be greater than zero.");
}

TEST_CASE("distances past the usable plane are rejected", "[cap:mask-output-styles][req:Operator statement arguments]") {
  // The operators add distances to plane coordinates in 32 bits; Magic's
  // arithmetic overflows on these (`grow 2000000000`).
  const std::string limit = std::to_string(hefesto::db::kPlaneMax);
  check_rejected("grow 2000000000", "Grow/shrink distance must be at most " + limit + ".");
  check_rejected("shrink " + std::to_string(hefesto::db::kPlaneMax + 1),
                 "Grow/shrink distance must be at most " + limit + ".");
  const One largest = one("grow " + limit);
  REQUIRE(largest.op.has_value());
  CHECK(largest.op->distance == hefesto::db::kPlaneMax);
  check_rejected("bridge 2000000000 6", "Bridge distance and width must be at most " + limit + ".");
  check_rejected("bridge 5 2000000000", "Bridge distance and width must be at most " + limit + ".");
  check_rejected("bloat-all ndiff poly 2000000000", "Bloat distance must be at most " + limit + ".");
  const One bloat = one("bloat-max ndiff poly 2000000000 pdiff -2000000000");
  REQUIRE(bloat.op.has_value());
  CHECK(bloat.messages == std::vector<std::string>{"Bloat distance must be at most " + limit + ".",
                                                   "Bloat distance must be at most " + limit + "."});
  const Parsed p = parse("bloat-max ndiff poly 2000000000 pdiff -2000000000");
  CHECK(bloat.op->bloat->distance[p.r.type("poly")] == hefesto::db::kPlaneMax);
  CHECK(bloat.op->bloat->distance[p.r.type("pdiff")] == -hefesto::db::kPlaneMax);
}

TEST_CASE("net, tagged and mask-hints", "[cap:mask-output-styles][req:Operator statement arguments]") {
  const One net = one("net VDD T");
  REQUIRE(net.op.has_value());
  CHECK(net.op->text == "VDD");
  CHECK(net.op->cif_layers == std::vector<std::size_t>{0});
  CHECK(one("tagged label").op->text == "label");
  CHECK(one("mask-hints MH").op->text == "MH");
  check_rejected("net", wrong("net"));
  check_rejected("tagged a b c", wrong("tagged"));
  check_rejected("mask-hints", wrong("mask-hints"));
  check_rejected("mask-hints a b", wrong("mask-hints"));
}

TEST_CASE("maxrect, orthogonal, bbox, boundary and not-square",
          "[cap:mask-output-styles][req:Operator statement arguments]") {
  CHECK_FALSE(one("maxrect").op->option);
  CHECK(one("maxrect external").op->option);
  CHECK_FALSE(one("maxrect internal").op->option);
  const One badmax = one("maxrect sideways");
  REQUIRE(badmax.op.has_value());  // the default is kept
  CHECK(badmax.messages ==
        std::vector<std::string>{"Maxrect takes only one optional argument \"external\" or \"internal\" (default)."});
  check_rejected("maxrect a b", wrong("maxrect"));
  CHECK(one("orthogonal fill").op->option);
  CHECK_FALSE(one("orthogonal remove").op->option);
  CHECK(one("orthogonal fil").messages ==
        std::vector<std::string>{"Orthogonal takes only one optional argument \"fill\" or \"remove\" (default)."});
  check_rejected("orthogonal a b", wrong("orthogonal"));
  CHECK(one("bbox top").op->option);
  CHECK(one("bbox tops").messages == std::vector<std::string>{"BBox takes only one optional argument \"top\"."});
  check_rejected("bbox a b", wrong("bbox"));
  CHECK(one("boundary").op->kind == OpKind::boundary);
  check_rejected("boundary x", wrong("boundary"));
  CHECK(one("not-square ignored extra").op->kind == OpKind::not_square);
}

TEST_CASE("squares and squares-grid", "[cap:mask-output-styles][req:Operator statement arguments]") {
  // Scenarios "Single-argument squares" and "Odd single-argument squares rejected".
  const One sq = one("squares 4");
  REQUIRE(sq.op.has_value());
  CHECK(sq.op->kind == OpKind::squares);
  CHECK(sq.op->values == std::vector<std::int32_t>{2, 4, 4});
  check_rejected("squares 5", "Squares must have positive even sizes.");
  CHECK(one("squares 1 2 4").op->values == std::vector<std::int32_t>{1, 2, 4});
  check_rejected("squares 1 2", wrong("squares"));
  check_rejected("squares 1 2 3 4", wrong("squares"));
  check_rejected("squares -1 2 3", "Square border must not be negative.");
  check_rejected("squares 1 0 3", "Squares must have positive sizes.");
  check_rejected("squares 1 2 0", "Square separation must be positive.");
  const One grid = one("squares-grid 1 2 3 4 5");
  REQUIRE(grid.op.has_value());
  CHECK(grid.op->kind == OpKind::squares_grid);
  CHECK(grid.op->values == std::vector<std::int32_t>{1, 2, 3, 4, 5});
  CHECK(one("squares-grid 1 2 3 4").op->values == std::vector<std::int32_t>{1, 2, 3, 4, 4});
  CHECK(one("squares-grid 1 2 3").op->values == std::vector<std::int32_t>{1, 2, 3, 1, 1});
  check_rejected("squares-grid 1 2 3 0", "Square grid must be strictly positive.");
  check_rejected("squares-grid 1 2 3 4 0", "Square y-grid must be strictly positive.");
  check_rejected("squares-grid 1 2 3 4 5 6", wrong("squares-grid"));
}

TEST_CASE("slots", "[cap:mask-output-styles][req:Operator statement arguments]") {
  CHECK(one("slots 1 2 4").op->values == std::vector<std::int32_t>{1, 2, 4, 0, 0, 0, 0, 0});
  CHECK(one("slots 1 2 4 3").op->values == std::vector<std::int32_t>{1, 2, 4, 3, 0, 0, 0, 0});
  CHECK(one("slots 1 2 4 3 6 8").op->values == std::vector<std::int32_t>{1, 2, 4, 3, 6, 8, 0, 0});
  CHECK(one("slots 1 2 4 3 6 8 7 9").op->values == std::vector<std::int32_t>{1, 2, 4, 3, 6, 8, 7, 9});
  // Q-MASK-9: the 6-token form is a wrong argument count, read no further.
  check_rejected("slots 1 2 3 4 5", wrong("slots"));
  check_rejected("slots 1 2", wrong("slots"));
  check_rejected("slots 1 2 3 4 5 6 7 8 9", wrong("slots"));
  check_rejected("slots -1 2 3", "Slot border must be non-negative.");
  check_rejected("slots 1 0 3", "Slot short-side size must be strictly positive.");
  check_rejected("slots 1 2 0", "Slot separation must be strictly positive.");
  check_rejected("slots 1 2 3 -4", "Slot border must be non-negative.");
  check_rejected("slots 1 2 3 4 -5 6", "Slot long-side size must be positive or zero.");
  check_rejected("slots 1 2 3 4 5 -6", "Slot long-side separation must be non-negative.");
  check_rejected("slots 1 2 3 4 5 0",
                 "Slot long-side separation must be strictly positive when long-side size is nonzero");
  check_rejected("slots 1 2 3 4 5 6 -7", "Slot offset must be non-negative.");
  check_rejected("slots 1 2 3 4 5 6 7 -8", "Slot start must be non-negative.");
}

TEST_CASE("an operation needs a current layer", "[cap:mask-output-styles][req:Operator statement arguments]") {
  const auto r = read("style s\nscalefactor 1\ngrow 3\nfrob 3");
  CHECK(r.texts() == std::vector<std::string>{"Must define layer before specifying operation.",
                                              "Must define layer before specifying operation."});
}

TEST_CASE("squares become squares-grid below the grid limit",
          "[cap:mask-output-styles][req:squares-to-grid conversion by gridlimit]") {
  // Scenario "Conversion".
  const auto r = read("style s\nscalefactor 2\nlayer A metal1\nsquares 10 34 34\ngridlimit 5\nlayer B metal1\n"
                      "squares 10 34 34\nsquares-grid 10 34 34 7 2\nbloat-all metal1 metal2");
  const auto& a = r.style().layers[0].ops[1];
  CHECK(a.kind == OpKind::squares);  // parsed before the gridlimit line
  CHECK(a.values == std::vector<std::int32_t>{10, 34, 34});
  const auto& b = r.style().layers[1].ops[1];
  CHECK(b.kind == OpKind::squares_grid);
  CHECK(b.values == std::vector<std::int32_t>{10, 34, 34, 5, 5});
  CHECK(r.style().layers[1].ops[2].values == std::vector<std::int32_t>{10, 34, 34, 7, 5});
}

TEST_CASE("stacked contacts join the operations that select a component",
          "[cap:mask-output-styles][req:Automatic stacked-contact inclusion]") {
  // Scenario "Via stack".
  const auto r = read("style s\nscalefactor 1\nlayer VIA1 m2c\nlayer P pc\nlayer M metal1");
  const auto stacked = r.type("pm12c");
  CHECK(r.style().layers[0].ops[0].types.test(stacked));
  CHECK(r.style().layers[1].ops[0].types.test(stacked));
  CHECK_FALSE(r.style().layers[2].ops[0].types.test(stacked));  // metal1 is not a contact
}
