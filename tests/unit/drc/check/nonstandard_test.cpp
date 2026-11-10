// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-semantics "Maxwidth rules", "Area rule", "Rectangle size
// rule", "Off-grid rule" and "Angle rules", with the replicated quirks
// Q-DRC-8 (area aborts at K edges), Q-DRC-19 (exclude types ignored by
// bend_illegal/both), Q-DRC-21 (start tiles) in both magic-compat modes and
// the Q-DRC-9 off-grid rounding per mode.

#include "check_fixture.hpp"

using namespace hefesto;
using namespace hefesto::drc::test;
using drc::RuleSetBuilder;

#define MAXWIDTH "[cap:drc-rule-semantics][req:Maxwidth rules]"
#define AREA "[cap:drc-rule-semantics][req:Area rule]"
#define RECTSIZE "[cap:drc-rule-semantics][req:Rectangle size rule]"
#define OFFGRID "[cap:drc-rule-semantics][req:Off-grid rule]"
#define ANGLES "[cap:drc-rule-semantics][req:Angle rules]"

TEST_CASE("slotting rule (spec scenario)", MAXWIDTH) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 12, 12});
  RuleSetBuilder b;
  add_maxwidth(b, mask_of({l.t.m1}), l.t.metal1, 10, drc::flags::kBends, b.why("slot"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}) ==
          std::vector<Rect>{Rect{0, 0, 12, 11}, Rect{0, 0, 11, 12}});
  }
  // A 10 x 10 square has no 11 x 11 square: clean.
  Layout ok;
  ok.paint(ok.t.m1, Rect{0, 0, 10, 10});
  CHECK(run_check(ok, *rules).empty());
}

TEST_CASE("bend_illegal L-shape (spec scenario)", MAXWIDTH) {
  Layout l;
  l.paint(l.t.pmc, Rect{0, 0, 4, 10});
  l.paint(l.t.pmc, Rect{0, 0, 10, 4});
  RuleSetBuilder b;
  add_maxwidth(b, mask_of({l.t.pmc}), l.t.active, 4, 0, b.why("pmc"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}) ==
          std::vector<Rect>{Rect{0, 0, 10, 4}, Rect{0, 4, 4, 10}});
  }
  // A straight 4-wide bar is legal.
  Layout bar;
  bar.paint(bar.t.pmc, Rect{0, 0, 4, 30});
  CHECK(run_check(bar, *rules).empty());
}

TEST_CASE("maxwidth both flags a region wider or higher than the limit", MAXWIDTH) {
  Layout l;
  l.paint(l.t.pmc, Rect{0, 0, 3, 10});
  RuleSetBuilder both;
  add_maxwidth(both, mask_of({l.t.pmc}), l.t.active, 4, drc::flags::kMaxWidthBoth, both.why("both"));
  CHECK(run_check(l, *both.build()) == std::vector<Rect>{Rect{0, 0, 3, 10}});
  RuleSetBuilder illegal;
  add_maxwidth(illegal, mask_of({l.t.pmc}), l.t.active, 4, 0, illegal.why("bend_illegal"));
  CHECK(run_check(l, *illegal.build()).empty());
}

TEST_CASE("exclude types drop bend_ok candidates but not bend_illegal or both (Q-DRC-19)", MAXWIDTH) {
  Layout l;
  l.paint(l.t.pmc, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{-5, -5, 15, 15});  // the exclude layer covers the region
  const TypeMask exclude = mask_of({l.t.m1});
  for (const bool compat : {true, false}) {
    RuleSetBuilder ok_bends;
    add_maxwidth(ok_bends, mask_of({l.t.pmc}), l.t.active, 4, drc::flags::kBends, ok_bends.why("x"), &exclude,
                 l.t.metal1);
    CHECK(run_check(l, *ok_bends.build(), Options{.magic_compat = compat}).empty());
    RuleSetBuilder illegal;
    add_maxwidth(illegal, mask_of({l.t.pmc}), l.t.active, 4, 0, illegal.why("x"), &exclude, l.t.metal1);
    CHECK(run_check(l, *illegal.build(), Options{.magic_compat = compat}) == std::vector<Rect>{Rect{0, 0, 10, 10}});
    RuleSetBuilder both;
    add_maxwidth(both, mask_of({l.t.pmc}), l.t.active, 4, drc::flags::kMaxWidthBoth, both.why("x"), &exclude,
                 l.t.metal1);
    CHECK(run_check(l, *both.build(), Options{.magic_compat = compat}) == std::vector<Rect>{Rect{0, 0, 10, 10}});
  }
  // Without the exclude layer the bend_ok rule reports.
  Layout bare;
  bare.paint(bare.t.pmc, Rect{0, 0, 10, 10});
  RuleSetBuilder ok_bends;
  add_maxwidth(ok_bends, mask_of({bare.t.pmc}), bare.t.active, 4, drc::flags::kBends, ok_bends.why("x"), &exclude,
               bare.t.metal1);
  CHECK_FALSE(run_check(bare, *ok_bends.build()).empty());
}

TEST_CASE("a tile whose first segment borders the same layer set is not a start tile (Q-DRC-21)", MAXWIDTH) {
  Layout l;
  l.paint(l.t.pdiff, Rect{0, 0, 6, 6});
  l.paint(l.t.poly, Rect{6, 0, 12, 6});
  RuleSetBuilder b;
  add_maxwidth(b, mask_of({l.t.pdiff, l.t.poly}), l.t.active, 4, 0, b.why("mw"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    // Only the pdiff tile (left edge on space) starts the flood and is reported.
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}) == std::vector<Rect>{Rect{0, 0, 6, 6}});
  }
}

TEST_CASE("small metal island (spec scenario)", AREA) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 4, 4});
  RuleSetBuilder b;
  add_area(b, mask_of({l.t.m1}), l.t.metal1, 20, 4, b.why("area"));
  const auto rules = b.build();
  const Rect k{-10, -10, 14, 14};
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat, .check = k, .clip = k}) ==
          std::vector<Rect>{Rect{0, 0, 4, 4}});
  }
}

TEST_CASE("island at the check boundary (spec scenario, Q-DRC-8)", AREA) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 4, 4});
  RuleSetBuilder b;
  add_area(b, mask_of({l.t.m1}), l.t.metal1, 20, 4, b.why("area"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat, .check = Rect{0, -10, 20, 14}}).empty());
    // A tile edge on any edge of K aborts the fill...
    CHECK(run_check(l, *rules, Options{.magic_compat = compat, .check = Rect{-10, -10, 20, 4}}).empty());
    // ...but crossing K without coinciding does not.
    CHECK(run_check(l, *rules, Options{.magic_compat = compat, .check = Rect{-10, -10, 2, 2}}) ==
          std::vector<Rect>{Rect{0, 0, 4, 4}});
  }
}

TEST_CASE("the area sums every 4-connected tile and reports only the start tile", AREA) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 4, 4});
  l.paint(l.t.m1, Rect{2, 4, 3, 8});  // 4 more: 20 in all
  RuleSetBuilder big;
  add_area(big, mask_of({l.t.m1}), l.t.metal1, 21, 4, big.why("area"));
  // Two tiles have a left edge on space; each is a start tile.
  CHECK(run_check(l, *big.build()) == std::vector<Rect>{Rect{0, 0, 4, 4}, Rect{2, 4, 3, 8}});
  RuleSetBuilder exact;
  add_area(exact, mask_of({l.t.m1}), l.t.metal1, 20, 4, exact.why("area"));
  CHECK(run_check(l, *exact.build()).empty());  // the sum reaches the limit
}

TEST_CASE("odd size required (spec scenario)", RECTSIZE) {
  Layout l;
  l.paint(l.t.poly, Rect{0, 0, 4, 3});
  RuleSetBuilder b;
  add_rect_size(b, mask_of({l.t.poly}), l.t.active, 10, 1, b.why("via size"));
  const auto rules = b.build();
  CHECK(run_check(l, *rules) == std::vector<Rect>{Rect{0, 0, 1, 3}});
  Layout odd;
  odd.paint(odd.t.poly, Rect{0, 0, 5, 3});
  CHECK(run_check(odd, *rules).empty());
  Layout high;
  high.paint(high.t.poly, Rect{0, 0, 5, 4});  // width odd, height even
  CHECK(run_check(high, *rules) == std::vector<Rect>{Rect{0, 0, 5, 1}});
}

TEST_CASE("rectangles larger than the maximum report the excess", RECTSIZE) {
  RuleSetBuilder b;
  const CheckTech& t = check_tech();
  add_rect_size(b, mask_of({t.poly}), t.active, 10, -1, b.why("max"));
  const auto rules = b.build();
  Layout wide;
  wide.paint(t.poly, Rect{0, 0, 14, 3});
  CHECK(run_check(wide, *rules) == std::vector<Rect>{Rect{0, 0, 4, 3}});
  Layout tall;
  tall.paint(t.poly, Rect{0, 0, 3, 13});
  CHECK(run_check(tall, *rules) == std::vector<Rect>{Rect{0, 0, 3, 3}});
  Layout fine;
  fine.paint(t.poly, Rect{0, 0, 10, 10});
  CHECK(run_check(fine, *rules).empty());
}

TEST_CASE("off-grid vertical edge (spec scenario)", OFFGRID) {
  Layout l;
  l.paint(l.t.m1, Rect{3, 0, 10, 10});
  RuleSetBuilder b;
  add_off_grid(b, mask_of({l.t.m1}), l.t.metal1, 5, b.why("grid"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}) == std::vector<Rect>{Rect{0, 0, 3, 10}});
  }
}

TEST_CASE("off-grid edge at negative coordinates with magic-compat ON (spec scenario)", OFFGRID) {
  Layout l;
  l.paint(l.t.m1, Rect{-8, -7, -3, 2});
  RuleSetBuilder b;
  add_off_grid(b, mask_of({l.t.m1}), l.t.metal1, 5, b.why("grid"));
  const auto errors = run_check(l, *b.build(), Options{.magic_compat = true});
  CHECK(errors == sorted({Rect{-8, -7, -5, 2}, Rect{-3, -7, 0, 2}, Rect{-8, -7, 0, -5}, Rect{-8, 0, 0, 2}}));
}

TEST_CASE("off-grid edge at negative coordinates with magic-compat OFF (spec scenario)", OFFGRID) {
  Layout l;
  l.paint(l.t.m1, Rect{-8, -7, -3, 2});
  RuleSetBuilder b;
  add_off_grid(b, mask_of({l.t.m1}), l.t.metal1, 5, b.why("grid"));
  const auto errors = run_check(l, *b.build(), Options{.magic_compat = false});
  CHECK(contains(errors, Rect{-10, -10, -8, 2}));
  CHECK(contains(errors, Rect{-5, -10, -3, 2}));
  CHECK_FALSE(contains(errors, Rect{-8, -7, -5, 2}));
}

TEST_CASE("off-grid: pitch 1 never errs and errors are not re-clipped", OFFGRID) {
  Layout l;
  l.paint(l.t.m1, Rect{3, 0, 10, 10});
  RuleSetBuilder one;
  add_off_grid(one, mask_of({l.t.m1}), l.t.metal1, 1, one.why("grid"));
  CHECK(run_check(l, *one.build()).empty());
  RuleSetBuilder five;
  add_off_grid(five, mask_of({l.t.m1}), l.t.metal1, 5, five.why("grid"));
  // The segment is clipped to L first, then snapped outward past L.
  CHECK(run_check(l, *five.build(), Options{.clip = Rect{2, 2, 20, 8}}) == std::vector<Rect>{Rect{0, 0, 3, 8}});
}

TEST_CASE("45-degree check (spec scenario)", ANGLES) {
  RuleSetBuilder b;
  const CheckTech& t = check_tech();
  add_angles(b, mask_of({t.m1}), t.metal1, drc::flags::kAngles45 | drc::flags::kAngles90, b.why("45 only"));
  const auto rules = b.build();
  Layout skewed;
  skewed.triangle(t.m1, Rect{0, 0, 4, 3}, db::Diagonal::ne_sw, true);
  CHECK(run_check(skewed, *rules) == std::vector<Rect>{Rect{0, 0, 4, 3}});
  Layout square;
  square.triangle(t.m1, Rect{0, 0, 3, 3}, db::Diagonal::ne_sw, true);
  CHECK(run_check(square, *rules).empty());
}

TEST_CASE("angles 90 flags every split tile", ANGLES) {
  RuleSetBuilder b;
  const CheckTech& t = check_tech();
  add_angles(b, mask_of({t.m1}), t.metal1, drc::flags::kAngles90, b.why("90 only"));
  const auto rules = b.build();
  Layout square;
  square.triangle(t.m1, Rect{0, 0, 3, 3}, db::Diagonal::nw_se, false);
  CHECK(run_check(square, *rules) == std::vector<Rect>{Rect{0, 0, 3, 3}});
  Layout manhattan;
  manhattan.paint(t.m1, Rect{0, 0, 3, 3});
  CHECK(run_check(manhattan, *rules).empty());
}

TEST_CASE("45-only edge rules ignore candidates inside split tiles", ANGLES) {
  const CheckTech& t = check_tech();
  Layout l;
  l.paint(t.m1, Rect{0, 0, 10, 10});
  l.triangle(t.m1, Rect{10, 0, 14, 4}, db::Diagonal::ne_sw, true);  // bevel: space in its upper-left
  const auto rule = [&](drc::RuleFlags extra) {
    RuleSetBuilder b;
    b.add(t.m1, tech::kSpace, make_rule(1, 0, mask_of({t.m1}), TypeMask{}, drc::flags::kForward | extra, t.metal1,
                                        t.metal1, b.why("bevel")));
    return b.build();
  };
  // A plain edge rule reports the split candidate right of x = 10 ...
  CHECK(contains(run_check(l, *rule(0)), Rect{10, 0, 11, 4}));
  // ... the ANGLES_45 variant drops it.
  CHECK_FALSE(contains(run_check(l, *rule(drc::flags::kAngles45)), Rect{10, 0, 11, 4}));
}
