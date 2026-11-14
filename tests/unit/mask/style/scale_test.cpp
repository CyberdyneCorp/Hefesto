// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Unit normalisation, the CIF reducer, OutputStyle::rescale, the halo
// radius (Q-MASK-18 in both modes) and the contact cut size query
// (tasks.md 3.3).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "style_fixture.hpp"

namespace mask = hefesto::mask;
namespace tech = hefesto::tech;
using mask::OpKind;
using mask::test::compat_mode;
using mask::test::read;

TEST_CASE("a nanometre style is reduced to its smallest units", "[cap:mask-output-styles][req:Unit normalisation]") {
  // Scenario "Nanometre style reduced".
  const auto r = read("style s\nscalefactor 10 nanometers\nlayer A metal1\ngrow 50");
  CHECK(r.style().scale.scale_factor == 1);
  CHECK(r.style().scale.expander == 1);
  CHECK(r.style().layers[0].ops[1].distance == 5);
}

TEST_CASE("an odd scale factor doubles the units for half-grid contacts",
          "[cap:mask-output-styles][req:Unit normalisation]") {
  // Scenario "Odd scale with everything even".
  const auto even = read("style s\nscalefactor 9\nlayer A metal1\ngrow 30");
  CHECK(even.style().scale == mask::StyleScale{9, 1, 3});
  CHECK(even.style().layers[0].ops[1].distance == 30);
  // Scenario "Odd scale doubling preserved by squares", computed as the
  // normalisation procedure states (each value is multiplied by d = 2
  // before it is halved): 44 * 2 / 2 / 2 = 22 still lets the factor 2
  // reduce, while 42 * 2 / 2 / 2 = 21 blocks it.
  const auto sq44 = read("style s\nscalefactor 9\nlayer A pc\nsquares 12 44 44");
  CHECK(sq44.style().scale.scale_factor == 9);
  CHECK(sq44.style().scale.expander == 1);
  CHECK(sq44.style().layers[0].ops[1].values == std::vector<std::int32_t>{12, 44, 44});
  const auto sq42 = read("style s\nscalefactor 9\nlayer A pc\nsquares 12 42 42");
  CHECK(sq42.style().scale.scale_factor == 18);
  CHECK(sq42.style().scale.expander == 2);
  CHECK(sq42.style().layers[0].ops[1].values == std::vector<std::int32_t>{24, 84, 84});
  // An even scale factor with an odd squares size doubles too (d is odd).
  const auto odd_size = read("style s\nscalefactor 2\nlayer A pc\nsquares 5 17 17");
  CHECK(odd_size.style().scale == mask::StyleScale{4, 2, 1});
  CHECK(odd_size.style().layers[0].ops[1].values == std::vector<std::int32_t>{10, 34, 34});
}

TEST_CASE("bloat-all blocks the unit reduction", "[cap:mask-output-styles][req:Unit normalisation]") {
  // Scenario "bloat-all blocks reduction" (Q-MASK-13, both modes).
  const auto r = read("style s\nscalefactor 10 nanometers\nlayer A metal1\ngrow 50\nbloat-all metal1 metal2");
  CHECK(r.style().scale.scale_factor == 10);
  CHECK(r.style().scale.expander == 10);
  CHECK(r.style().layers[0].ops[1].distance == 50);
  for (const bool compat : {true, false}) {
    const auto again = r.style().rescale(1, 1, compat_mode(compat));
    CHECK(again->scale == r.style().scale);
  }
}

TEST_CASE("rescale multiplies every operand and reduces again", "[cap:mask-output-styles][req:Unit normalisation]") {
  const auto r = read("style s\nscalefactor 10 nanometers\nlayer A pc\ngrow 50\nsquares-grid 10 20 30 5 5\n"
                      "bridge 40 60\nbloat-or pc metal1 30\nslots 10 20 40 0 0 0\nbloat-all metal1 metal2");
  const mask::OutputStyle& s = r.style();
  REQUIRE(s.scale == mask::StyleScale{10, 10, 1});
  // The internal grid becomes 2 times finer: every operand is multiplied
  // by 2, then the common factor 2 is divided out again, except from
  // bloat-all's flags (Q-MASK-13).
  const mask::RescaleStep step = mask::rescale_step(s, 1, 2, compat_mode(true));
  CHECK(step.multiplier == 2);
  CHECK(step.reduced == 2);
  const mask::OutputStyle& t = *step.style;
  CHECK(t.scale.scale_factor == 5);
  CHECK(t.scale.expander == 10);
  CHECK(t.layers[0].ops[1].distance == 50);
  CHECK(t.layers[0].ops[2].values == std::vector<std::int32_t>{10, 20, 30, 5, 5});
  CHECK(t.layers[0].ops[3].distance == 40);
  CHECK(t.layers[0].ops[3].values == std::vector<std::int32_t>{60});
  CHECK(t.layers[0].ops[4].bloat->distance[r.type("metal1")] == 30);
  CHECK(t.layers[0].ops[5].values == std::vector<std::int32_t>{10, 20, 40, 0, 0, 0, 0, 0});
  CHECK(t.layers[0].ops[6].bloat->distance[r.type("metal2")] == 2);
  // The source style is unchanged (styles are immutable).
  CHECK(s.scale.scale_factor == 10);
  CHECK(s.layers[0].ops[6].bloat->distance[r.type("metal2")] == 1);
  // The odd scale factor 5 doubles n and d before the next step.
  const auto coarse = mask::rescale_step(t, 2, 1, compat_mode(true));
  CHECK(coarse.multiplier == 2);
  CHECK(coarse.reduced == 2);
  CHECK(coarse.style->scale.scale_factor == 10);
  CHECK(coarse.style->scale.expander == 10);
}

TEST_CASE("the CIF reducer", "[cap:mask-output-styles][req:CIF reducer]") {
  // Scenario "Reducer".
  const auto r = read("style s\nscalefactor 100\nlayer A pc\ngrow 50\nsquares 20 40 40");
  CHECK(r.style().scale.scale_factor == 100);
  CHECK(r.style().scale.reducer == 20);
  // Slots values count (sizes and separations halved); the interacting
  // family's flags are skipped (Q-MASK-12, fix).
  const auto slots = read("style s\nscalefactor 100\ntemplayer T pc\nlayer A pc\nslots 50 20 100\n"
                          "interacting T\nnoninteracting T");
  CHECK(slots.style().scale.reducer == 10);
  // bloat-all's flag 1 makes the reducer 1.
  CHECK(read("style s\nscalefactor 100\nlayer A pc\ngrow 50\nbloat-all pc metal2").style().scale.reducer == 1);
}

TEST_CASE("the gridlimit value is the parsing side of squares-grid",
          "[cap:mask-output-styles][req:squares-to-grid conversion by gridlimit]") {
  // The grid values are normalised with the other operands; gridlimit itself is not (Q-MASK-8).
  const auto r = read("style s\nscalefactor 10 nanometers\ngridlimit 20\nlayer A pc\nsquares 40 60 60");
  CHECK(r.style().scale == mask::StyleScale{1, 1, 1});
  CHECK(r.style().grid_limit == 20);
  REQUIRE(r.style().layers[0].ops[1].kind == OpKind::squares_grid);
  CHECK(r.style().layers[0].ops[1].values == std::vector<std::int32_t>{4, 6, 6, 2, 2});
}

TEST_CASE("the interaction radius", "[cap:mask-output-styles][req:Interaction radius]") {
  // Scenario "Radius".
  const auto r = read("style s\nscalefactor 9\nlayer A metal1\ngrow 30\nlayer B metal2\nshrink 10");
  CHECK(r.style().scale.scale_factor == 9);
  CHECK(r.style().halo() == 7);
  CHECK(r.style().compute_halo() == 7);
  // Earlier layers' radii carry through mask-layer references; bloat-or
  // counts; operators after bbox/net/tagged/mask-hints do not.
  const auto chained = read("style s\nscalefactor 2\ntemplayer T metal1\ngrow 4\nlayer A T\ngrow 6\nlayer B metal1\n"
                            "bloat-or metal1 metal2 20\nlayer C metal1\nbbox\ngrow 100");
  CHECK(chained.style().halo() == 2 * 20 / 2 + 1);
}

TEST_CASE("a non-reducing rescale keeps the stale radius only with magic-compat ON",
          "[cap:mask-output-styles][req:Interaction radius][req:Unit normalisation]") {
  // Q-MASK-18 (compat).
  const auto r = read("style s\nscalefactor 10 nanometers\nlayer A metal1\ngrow 50\nbloat-all metal1 metal2");
  REQUIRE(r.style().halo() == 2 * 50 / 10 + 1);
  const auto on = r.style().rescale(2, 1, compat_mode(true));
  CHECK(on->scale.scale_factor == 20);
  CHECK(on->scale.expander == 10);
  CHECK(on->halo() == 11);  // not recomputed (Magic)
  const auto off = r.style().rescale(2, 1, compat_mode(false));
  CHECK(off->scale.scale_factor == 20);
  CHECK(off->halo() == 2 * 50 / 20 + 1);  // recomputed
  CHECK(r.style().rescale(2, 1)->halo() == 11);  // the default is magic-compat ON
}

TEST_CASE("a reducing rescale recomputes the radius in both modes",
          "[cap:mask-output-styles][req:Interaction radius][req:Unit normalisation]") {
  const auto r = read("style s\nscalefactor 10 nanometers\nlayer A metal1\ngrow 50");
  for (const bool compat : {true, false}) {
    const auto s = r.style().rescale(1, 2, compat_mode(compat));
    CHECK(s->halo() == s->compute_halo());
  }
}

// Q-MASK-29: Magic reduces by min(operand GCD, gcd(scale, expander)), not
// by their GCD, so the scale factor can be truncated. Both modes copy the
// arithmetic; only magic-compat OFF warns.
TEST_CASE("a rescale that truncates the scale factor warns only with magic-compat OFF",
          "[cap:mask-output-styles][req:Unit normalisation]") {
  // Scenario "Scale factor truncated by the reduction": a 10/10 style with
  // bloat-all under scalegrid 1 3 becomes 10/30, the operands share 3, and
  // 10 / 3 is truncated to 3.
  const auto r = read("style s\nscalefactor 10 nanometers\nlayer A metal1\nbloat-all metal1 metal2");
  REQUIRE(r.style().scale.scale_factor == 10);
  REQUIRE(r.style().scale.expander == 10);
  for (const bool compat : {true, false}) {
    const mask::RescaleStep step = mask::rescale_step(r.style(), 1, 3, compat_mode(compat));
    CHECK(step.multiplier == 3);
    CHECK(step.reduced == 3);
    CHECK(step.style->scale.scale_factor == 3);
    CHECK(step.style->scale.expander == 10);
    if (compat) {
      CHECK(step.messages.empty());
    } else {
      REQUIRE(step.messages.size() == 1);
      CHECK(step.messages[0].code == "MASK-STYLE-007");
      CHECK(step.messages[0].native_only);
      CHECK(step.messages[0].text ==
            "the scale factor of output style \"s\" was rounded: 10 is not a multiple of the common factor 3, "
            "so it became 3 (Q-MASK-29).");
    }
  }
  // An exact reduction says nothing in either mode.
  for (const bool compat : {true, false}) {
    CHECK(mask::rescale_step(r.style(), 1, 2, compat_mode(compat)).messages.empty());
  }
}

TEST_CASE("a load that truncates the scale factor warns only with magic-compat OFF",
          "[cap:mask-output-styles][req:Unit normalisation]") {
  // scalefactor 10 angstroms: expander 100, grow 4 shares 4 with it, and
  // gcd(10, 100) = 10 > 4, so Magic divides by 4: 10 / 4 = 2, 100 / 4 = 25.
  const std::string body = "style s\nscalefactor 10 angstroms\nlayer A metal1\ngrow 4";
  for (const bool compat : {true, false}) {
    const auto technology = mask::test::tech_with(body);
    hefesto::diag::CollectingSink sink;
    const mask::OutputStyleSet set = mask::read_output_styles(*technology, sink, compat_mode(compat));
    REQUIRE(set.current_style() != nullptr);
    CHECK(set.current_style()->scale.scale_factor == 2);
    CHECK(set.current_style()->scale.expander == 25);
    CHECK(set.current_style()->layers[0].ops[1].distance == 1);
    const auto messages = sink.take();
    if (compat) {
      CHECK(messages.empty());
    } else {
      REQUIRE(messages.size() == 1);
      CHECK(messages[0].code == "MASK-STYLE-007");
      CHECK(messages[0].text.find("10 is not a multiple of the common factor 4, so it became 2") !=
            std::string::npos);
    }
  }
}

TEST_CASE("the contact cut size query", "[cap:mask-output-styles][req:Contact cut size query]") {
  // Scenario "Contact size", on a style built in mask units.
  mask::OutputStyle style;
  const auto r = read("style s\nscalefactor 2\nlayer A metal1");
  mask::OutputLayer cont;
  cont.name = "CONT";
  mask::LayerOp orop;
  orop.types = tech::TypeMask::of(r.type("pc"));
  mask::LayerOp sq;
  sq.kind = OpKind::squares;
  sq.values = {5, 17, 17};
  cont.ops = {orop, sq};
  style.layers.push_back(cont);
  const auto cut = style.contact_cut(r.type("pc"));
  REQUIRE(cut.has_value());
  CHECK(cut->size() == 27);
  CHECK(cut->edge == 17);
  CHECK(cut->border == 5);
  CHECK(cut->spacing == 17);
  CHECK(style.contact_size(r.type("pc")) == 27);
  CHECK_FALSE(style.contact_size(r.type("m2c")).has_value());
}

TEST_CASE("the contact cut size query walks or and and-not only",
          "[cap:mask-output-styles][req:Contact cut size query]") {
  const auto r = read("style s\nscalefactor 2\ntemplayer T metal1\nlayer V m2c\nand-not T\nslots 2 4 6\n"
                      "layer P pc\ngrow 2\nsquares 2 4 4\nlayer N T,ndc\nsquares 2 4 4\nlayer M metal1\nor ndc\n"
                      "squares-grid 3 4 4 1 1");
  CHECK(r.style().contact_cut(r.type("m2c"))->size() == 4 + 2 * 2);  // slots: short side
  CHECK_FALSE(r.style().contact_cut(r.type("pc")).has_value());      // grow breaks the relation
  // A leading `or` with mask layers ends the walk; a later plain `or` may select the type.
  CHECK(r.style().contact_size(r.type("ndc")) == 4 + 2 * 3);
}
