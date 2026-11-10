// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-semantics "Standard edge rule check region", "Trigger rules"
// and "Wide-spacing trigger (maximal rectangle search)": the check region
// E with its corner extensions (Q-DRC-6 replicate, Q-DRC-7 compat), the
// candidate search and its run-length filter, trigger records, and the
// maximal-rectangle wide-spacing trigger.

#include "check_fixture.hpp"
#include "hefesto/drc/check/maxrect.hpp"

using namespace hefesto;
using namespace hefesto::drc::test;
using drc::RuleSetBuilder;

#define REGION "[cap:drc-rule-semantics][req:Standard edge rule check region]"
#define TRIGGER "[cap:drc-rule-semantics][req:Trigger rules]"
#define WIDE "[cap:drc-rule-semantics][req:Wide-spacing trigger (maximal rectangle search)]"

TEST_CASE("width violation (spec scenario)", REGION) {
  Layout l;
  l.paint(l.t.poly, Rect{0, 0, 1, 10});
  RuleSetBuilder b;
  add_width(b, mask_of({l.t.poly}), l.t.active, 2, b.why("w"));
  const auto rules = b.build();
  const auto reports = run_reports(l, *rules);
  REQUIRE(reports.size() == 1);
  CHECK(reports[0].area == Rect{1, 0, 2, 10});
  CHECK(rules->why().text(reports[0].rule.why) == "w");
}

TEST_CASE("spacing with a Manhattan corner extension (spec scenario)", REGION) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{11, 12, 20, 20});
  RuleSetBuilder b;
  add_spacing(b, mask_of({l.t.m1}), l.t.metal1, 3, b.why("s"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}) == std::vector<Rect>{Rect{11, 12, 13, 13}});
  }
}

TEST_CASE("touching_ok: abutting shapes merge and are not checked (spec scenario)", REGION) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{10, 0, 20, 5});
  RuleSetBuilder b;
  add_spacing(b, mask_of({l.t.m1}), l.t.metal1, 3, b.why("s"));
  const auto rules = b.build();
  CHECK(run_check(l, *rules).empty());
}

TEST_CASE("OUTSIDE rules search only the strip beyond the distance", REGION) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{12, 0, 20, 10});
  l.paint(l.t.m1, Rect{30, 0, 40, 10});
  RuleSetBuilder b;
  // Material must exist exactly 3 beyond the edge: E = [X+3, X+4].
  b.add(l.t.m1, tech::kSpace,
        make_rule(3, 0, mask_of({l.t.m1}), TypeMask{}, drc::flags::kForward | drc::flags::kOutside, l.t.metal1,
                  l.t.metal1, b.why("outside")));
  const auto errors = run_check(l, *b.build());
  // x = 10: [13,14] holds m1; x = 20 and x = 40: [23,24] and [43,44] are space.
  CHECK_FALSE(any_overlaps(errors, Rect{13, 0, 14, 10}));
  CHECK(contains(errors, Rect{23, 0, 24, 10}));
  CHECK(contains(errors, Rect{43, 0, 44, 10}));
  CHECK_FALSE(any_overlaps(errors, Rect{10, 0, 13, 10}));
}

TEST_CASE("horizontal edges extend corners without the continuation test (Q-DRC-6, both modes)", REGION) {
  // FORWARD|BOTHCORNERS, d = 2, c = 3, OK = {m1}, CORNER = {space}, on edges space|m1.
  const auto build = [](const CheckTech& t) {
    RuleSetBuilder b;
    b.add(tech::kSpace, t.m1,
          make_rule(2, 3, mask_of({t.m1}), mask_of({tech::kSpace}), drc::flags::kForward | drc::flags::kBothCorners,
                    t.metal1, t.metal1, b.why("q6")));
    return b.build();
  };
  for (const bool compat : {true, false}) {
    // Vertical: K cuts the left edge at y = 10; the probe above the cut is the
    // edge's own tiles, the edge continues, so no extension reaches the notch.
    Layout v;
    v.paint(v.t.m1, Rect{0, 0, 10, 20});
    REQUIRE(db::erase(v.cell, *v.t.tech, Rect{0, 11, 2, 12}, mask_of({v.t.m1})).ok());
    const auto vertical = run_check(v, *build(v.t), Options{.magic_compat = compat, .check = Rect{0, 0, 30, 10}});
    CHECK_FALSE(any_overlaps(vertical, Rect{0, 11, 2, 12}));
    // Horizontal (the same layout rotated): K cuts the bottom edge at x = 10;
    // the right end extends by c regardless and finds the notch.
    Layout h;
    h.paint(h.t.m1, Rect{0, 0, 20, 10});
    REQUIRE(db::erase(h.cell, *h.t.tech, Rect{11, 0, 12, 2}, mask_of({h.t.m1})).ok());
    const auto horizontal = run_check(h, *build(h.t), Options{.magic_compat = compat, .check = Rect{0, 0, 10, 30}});
    CHECK(contains(horizontal, Rect{11, 0, 12, 2}));
  }
}

TEST_CASE("run-length corner on a vertical edge with magic-compat ON (spec scenario)", REGION) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 12, 30});
  l.paint(l.t.m1, Rect{14, 30, 40, 60});
  RuleSetBuilder b;
  add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 20, 4, b.why("wide"));
  const auto rules = b.build();
  CHECK(run_check(l, *rules, Options{.magic_compat = true}) ==
        std::vector<Rect>{Rect{10, 10, 12, 30}, Rect{14, 30, 16, 50}});
}

TEST_CASE("run-length corner on a vertical edge with magic-compat OFF (spec scenario)", REGION) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 12, 30});
  l.paint(l.t.m1, Rect{14, 30, 40, 60});
  RuleSetBuilder b;
  add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 20, 4, b.why("wide"));
  const auto rules = b.build();
  CHECK(run_check(l, *rules, Options{.magic_compat = false}).empty());
}

TEST_CASE("the same layout rotated by 90 degrees reports nothing in both modes (Q-DRC-7)", REGION) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 30, 12});
  l.paint(l.t.m1, Rect{30, 14, 60, 40});
  RuleSetBuilder b;
  add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 20, 4, b.why("wide"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}).empty());
  }
}

TEST_CASE("run-length candidates shorter than the run length both ways are dropped", REGION) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 30, 30});
  l.paint(l.t.m1, Rect{32, 0, 40, 10});   // 10 high facing the block: shorter than 20
  l.paint(l.t.m1, Rect{32, 15, 40, 30});  // 15 high: shorter too
  RuleSetBuilder b;
  add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 20, 4, b.why("wide"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK_FALSE(any_overlaps(run_check(l, *rules, Options{.magic_compat = compat}), Rect{30, 0, 32, 30}));
  }
  Layout tall;
  tall.paint(tall.t.m1, Rect{0, 0, 30, 30});
  tall.paint(tall.t.m1, Rect{32, 0, 40, 25});  // 25 high: reported
  for (const bool compat : {true, false}) {
    CHECK(contains(run_check(tall, *rules, Options{.magic_compat = compat}), Rect{32, 0, 34, 25}));
  }
}

TEST_CASE("surround_ok spacing: the trigger guards its rule (spec scenario)", TRIGGER) {
  const CheckTech& t = check_tech();
  RuleSetBuilder b;
  add_surround_spacing(b, mask_of({t.nwell}), t.well, mask_of({t.pdiff}), t.active, 5, b.why("s"));
  const auto rules = b.build();

  // pdiff continues across the nwell edge for y in [0,4]; the trigger
  // records the non-pdiff part of the strip beyond the edge (y in [4,20]),
  // and the triggered rule checks y in [max(4-5,0), 20] = [0,20], which
  // includes the straddling pdiff.
  Layout l;
  l.paint(t.nwell, Rect{0, 0, 10, 20});
  l.paint(t.pdiff, Rect{8, 0, 14, 4});
  CHECK(run_check(l, *rules) == std::vector<Rect>{Rect{10, 0, 14, 4}});

  // A pdiff 3 beyond the edge in y in [10,12] is reported too.
  l.paint(t.pdiff, Rect{13, 10, 20, 12});
  CHECK(contains(run_check(l, *rules), Rect{13, 10, 15, 12}));

  // pdiff covering the whole strip beyond the edge: nothing is recorded, so
  // the spacing rule is skipped and nothing is reported.
  Layout covered;
  covered.paint(t.nwell, Rect{0, 0, 10, 20});
  covered.paint(t.pdiff, Rect{-5, -5, 15, 25});
  CHECK(run_check(covered, *rules).empty());

  // pdiff surrounded by nwell: the trigger fires but nothing is within 5 beyond.
  Layout inside;
  inside.paint(t.nwell, Rect{0, 0, 10, 20});
  inside.paint(t.pdiff, Rect{2, 2, 8, 18});
  CHECK(run_check(inside, *rules).empty());
}

TEST_CASE("triggered ranges are the recorded extents widened by c and clipped to the segment", TRIGGER) {
  Layout l;
  const CheckTech& t = l.t;
  l.paint(t.m1, Rect{0, 0, 10, 40});
  // The trigger looks for m2 right of the m1 edge; the triggered rule then
  // checks pdiff... on metal1 itself: any non-m1 within 2 of the edge over
  // the recorded y-range +- 1.
  l.paint(t.m2, Rect{10, 20, 11, 22});
  RuleSetBuilder b;
  const auto why = b.why("t");
  const TypeMask others = on_plane(t.metal1) & ~mask_of({t.m1});
  b.add(t.m1, tech::kSpace,
        make_rule(1, 0, on_plane(t.metal2) & ~mask_of({t.m2}), TypeMask{}, drc::flags::kTrigger, t.metal2,
                  t.metal1, why));
  b.add(t.m1, tech::kSpace,
        make_rule(2, 1, mask_of({t.m1}), others, drc::flags::kForward, t.metal1, t.metal1, why));
  const auto rules = b.build();
  CHECK(run_check(l, *rules) == std::vector<Rect>{Rect{10, 19, 12, 23}});
}

TEST_CASE("wide metal spacing (spec scenario)", WIDE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 12, 12});
  l.paint(l.t.m1, Rect{14, 0, 30, 2});
  // The WEST search from [0,12]x[0,2] with w = 10 (spec): Q = [2,12]x[-9,11].
  CHECK(drc::maximal_rectangles(l.cell.plane(l.t.metal1), Rect{2, -9, 12, 11}, ~mask_of({l.t.m1}), 10) ==
        std::vector<Rect>{Rect{2, 0, 12, 11}});
  RuleSetBuilder b;
  add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 0, 4, b.why("wide"));
  const auto rules = b.build();
  CHECK(run_check(l, *rules) == std::vector<Rect>{Rect{14, 0, 16, 2}});
}

TEST_CASE("narrow metal does not trigger (spec scenario)", WIDE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 8, 30});
  l.paint(l.t.m1, Rect{10, 0, 18, 30});
  RuleSetBuilder b;
  add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 0, 4, b.why("wide"));
  CHECK(run_check(l, *b.build()).empty());
}

TEST_CASE("option wide-width-noninclusive widens the trigger by one", WIDE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{12, 0, 30, 2});
  const auto build = [&](bool noninclusive) {
    RuleSetBuilder b;
    add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 0, 4, b.why("wide"));
    b.options(drc::StyleOptions{noninclusive});
    return b.build();
  };
  CHECK(run_check(l, *build(false)) == std::vector<Rect>{Rect{12, 0, 14, 2}});
  CHECK(run_check(l, *build(true)).empty());
}

TEST_CASE("a run-length wide trigger needs a long segment and a long candidate", WIDE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 12, 15});  // wide but only 15 long: < run length 20
  l.paint(l.t.m1, Rect{14, 0, 30, 15});
  RuleSetBuilder b;
  add_widespacing(b, mask_of({l.t.m1}), l.t.metal1, 10, 20, 4, b.why("wide"));
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}).empty());
  }
}

TEST_CASE("maximal rectangles are cut by every wrong tile and keep thick pieces", WIDE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 20, 20});
  l.paint(l.t.m2, Rect{0, 0, 1, 1});  // another plane: no effect on metal1
  const db::Plane& plane = l.cell.plane(l.t.metal1);
  const TypeMask wrong = ~mask_of({l.t.m1});
  // Inside the block: unchanged.
  CHECK(drc::maximal_rectangles(plane, Rect{2, 2, 12, 12}, wrong, 10) == std::vector<Rect>{Rect{2, 2, 12, 12}});
  // Straddling the right side: only the left piece of width >= 5 is kept.
  CHECK(drc::maximal_rectangles(plane, Rect{10, 2, 30, 12}, wrong, 5) == std::vector<Rect>{Rect{10, 2, 20, 12}});
  // Too thin after the cut: nothing is left.
  CHECK(drc::maximal_rectangles(plane, Rect{17, 2, 30, 12}, wrong, 5).empty());
  // A hole splits a candidate into overlapping pieces, in Magic's order
  // (top, bottom, left, right).
  REQUIRE(db::erase(l.cell, *l.t.tech, Rect{9, 9, 11, 11}, mask_of({l.t.m1})).ok());
  CHECK(drc::maximal_rectangles(plane, Rect{0, 0, 20, 20}, wrong, 5) ==
        std::vector<Rect>{Rect{0, 11, 20, 20}, Rect{0, 0, 20, 9}, Rect{0, 0, 9, 20}, Rect{11, 0, 20, 20}});
}
