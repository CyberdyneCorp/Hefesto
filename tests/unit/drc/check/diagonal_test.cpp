// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-semantics "Diagonal edge checks" (Q-DRC-10): Magic reports
// nothing from a diagonal edge (magic-compat ON); with magic-compat OFF the
// offset-triangle search reports the candidates it finds.

#include "check_fixture.hpp"

using namespace hefesto;
using namespace hefesto::drc::test;
using drc::RuleSetBuilder;

#define DIAGONAL "[cap:drc-rule-semantics][req:Diagonal edge checks]"

namespace {

/// The spec layout: a `/` split tile [0,10]^2 with m1 on its lower-right
/// side and an m1 square [-2,0]x[3,5] whose corner (0,3) is 3/sqrt(2) from
/// the diagonal.
struct SpecLayout : Layout {
  SpecLayout() {
    triangle(t.m1, Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true);
    paint(t.m1, Rect{-2, 3, 0, 5});
  }
};

std::shared_ptr<const drc::RuleSet> spacing3() {
  const CheckTech& t = check_tech();
  RuleSetBuilder b;
  add_spacing(b, mask_of({t.m1}), t.metal1, 3, b.why("s"));
  return b.build();
}

}  // namespace

TEST_CASE("diagonal spacing with magic-compat ON reports nothing from the diagonal (spec scenario)", DIAGONAL) {
  const SpecLayout l;
  const auto errors = run_check(l, *spacing3(), Options{.magic_compat = true});
  CHECK_FALSE(any_overlaps(errors, Rect{-2, 3, 0, 5}));
  drc::CheckInput input;
  input.cell = &l.cell;
  input.technology = l.t.tech.get();
  const auto rules = spacing3();
  input.rules = rules.get();
  drc::CollectingErrorSink sink;
  drc::CheckStatistics stats;
  sink.set_statistics(&stats);
  REQUIRE(drc::basic_check(input, kEverywhere, kEverywhere, sink) == drc::CheckOutcome::completed);
  CHECK(stats.slow > 0);  // the diagonal search is still counted
}

TEST_CASE("diagonal spacing with magic-compat OFF (spec scenario)", DIAGONAL) {
  const SpecLayout l;
  const auto errors = run_check(l, *spacing3(), Options{.magic_compat = false});
  CHECK(contains(errors, Rect{-2, 3, 0, 5}));
}

TEST_CASE("the diagonal search excludes the initiating tile and material beyond the offset", DIAGONAL) {
  Layout l;
  l.triangle(l.t.m1, Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true);
  l.paint(l.t.m1, Rect{-10, 12, -6, 16});  // far from the diagonal
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *spacing3(), Options{.magic_compat = compat}).empty());
  }
}

TEST_CASE("triggers and nonstandard rules are not evaluated on diagonals", DIAGONAL) {
  const SpecLayout l;
  const CheckTech& t = l.t;
  const TypeMask others = on_plane(t.metal1) & ~mask_of({t.m1});
  RuleSetBuilder b;
  const auto why = b.why("t");
  // A trigger that would always fire, then the spacing rule: on the
  // diagonal both are skipped.
  b.add(t.m1, tech::kSpace, make_rule(1, 1, TypeMask{}, others, drc::flags::kTrigger, t.metal1, t.metal1, why));
  b.add(t.m1, tech::kSpace, make_rule(3, 3, others, others, drc::flags::kForward, t.metal1, t.metal1, why));
  CHECK_FALSE(any_overlaps(run_check(l, *b.build(), Options{.magic_compat = false}), Rect{-2, 3, 0, 4}));
}
