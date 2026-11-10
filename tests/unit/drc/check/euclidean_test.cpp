// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-semantics "Euclidean corner filter": corner exemptions, the
// Q-DRC-20 radius in both magic-compat modes, the split-tile fallback,
// derived contact borders, and the pinned results of the floating-point
// formulas (design D3: identical on x86_64 and aarch64).

#include "check_fixture.hpp"
#include "hefesto/drc/check/euclidean.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"

using namespace hefesto;
using namespace hefesto::drc::test;
using drc::RuleSetBuilder;

#define EUCLID "[cap:drc-rule-semantics][req:Euclidean corner filter]"

namespace {

std::shared_ptr<const drc::RuleSet> m1_spacing(std::int32_t d, drc::RuleFlags extra = 0) {
  const CheckTech& t = check_tech();
  RuleSetBuilder b;
  add_spacing(b, mask_of({t.m1}), t.metal1, d, b.why("s"), extra);
  return b.build();
}

}  // namespace

TEST_CASE("Euclidean corner pass (spec scenario)", EUCLID) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{12, 12, 20, 20});
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *m1_spacing(3), Options{.magic_compat = compat, .euclidean = true}) ==
          std::vector<Rect>{Rect{12, 12, 13, 13}});
  }
}

TEST_CASE("Euclidean corner exempt (spec scenario)", EUCLID) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{14, 13, 30, 30});
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *m1_spacing(5), Options{.magic_compat = compat, .euclidean = true}).empty());
  }
}

TEST_CASE("Manhattan corner with Euclidean disabled (spec scenario)", EUCLID) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{14, 13, 30, 30});
  CHECK(run_check(l, *m1_spacing(5)) == std::vector<Rect>{Rect{14, 13, 15, 15}});
  // A rule with MANHATTAN (manhattan_dist) ignores `drc euclidean on`.
  CHECK(run_check(l, *m1_spacing(5, drc::flags::kManhattan), Options{.euclidean = true}) ==
        std::vector<Rect>{Rect{14, 13, 15, 15}});
}

TEST_CASE("corner distance above 4095 with magic-compat ON (spec scenario)", EUCLID) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{3600, 3600, 3700, 3700});
  CHECK(run_check(l, *m1_spacing(5000), Options{.magic_compat = true, .euclidean = true}) ==
        std::vector<Rect>{Rect{3600, 3600, 3700, 3700}});
}

TEST_CASE("corner distance above 4095 with magic-compat OFF (spec scenario)", EUCLID) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{3600, 3600, 3700, 3700});
  CHECK(run_check(l, *m1_spacing(5000), Options{.magic_compat = false, .euclidean = true}).empty());
}

TEST_CASE("corner radius at 4095 and 4096 (Q-DRC-20 pinned)", EUCLID) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{3000, 3000, 3100, 3100});
  // c = 4095 keeps all 12 bits: exempt in both modes.
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *m1_spacing(4095), Options{.magic_compat = compat, .euclidean = true}).empty());
  }
  // c = 4096 packs to s = 0 with magic-compat ON: the corner point is E's
  // corner and the candidate is reported; OFF keeps s = 4096 and exempts it.
  CHECK(run_check(l, *m1_spacing(4096), Options{.magic_compat = true, .euclidean = true}) ==
        std::vector<Rect>{Rect{3000, 3000, 3100, 3100}});
  CHECK(run_check(l, *m1_spacing(4096), Options{.magic_compat = false, .euclidean = true}).empty());
}

TEST_CASE("a split candidate whose diagonal faces the corner is measured to the diagonal", EUCLID) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  // `\` tile with m1 in its upper-right triangle: the diagonal x + y = 36
  // is sqrt(128) from P = (10,10), beyond s = 10.
  l.triangle(l.t.m1, Rect{13, 13, 23, 23}, db::Diagonal::nw_se, true);
  CHECK(run_check(l, *m1_spacing(10), Options{.euclidean = true}).empty());
  CHECK(run_check(l, *m1_spacing(10)) == std::vector<Rect>{Rect{13, 13, 20, 20}});
}

TEST_CASE("derived borders measure corners between contact cuts", EUCLID) {
  Layout l;
  l.paint(l.t.via, Rect{0, 0, 10, 10});
  l.paint(l.t.via, Rect{12, 12, 20, 20});
  const CheckTech& t = l.t;
  RuleSetBuilder b;
  add_spacing(b, mask_of({t.via}), t.metal1, 3, b.why("via"), drc::flags::kDerivedEdge | drc::flags::kDerivedTarget);
  const auto rules = b.build();
  // Without a cifoutput catalog there is no border: the corner is an error.
  CHECK(contains(run_check(l, *rules, Options{.euclidean = true}), Rect{12, 12, 13, 13}));

  // With the fixture's `squares` border of 1 on each contact (style `test`,
  // scalefactor 1), (2+2)^2 * 2 >= (3+2)^2: the corner is exempt.
  const drc::CifCatalog& catalog = check_cif();
  // The catalog keeps the border in CIF units over the scalefactor as the
  // style rescales them (num / den = 1 internal unit).
  const auto border = catalog.contact_border(t.via);
  REQUIRE(border.has_value());
  REQUIRE(border->num > 0);
  REQUIRE(border->num == border->den);
  for (const bool compat : {true, false}) {
    CHECK_FALSE(any_overlaps(run_check(l, *rules, Options{.magic_compat = compat, .euclidean = true, .cif = &catalog}),
                             Rect{12, 12, 13, 13}));
  }
}

TEST_CASE("point-to-segment distances are pinned (single-precision projection)", EUCLID) {
  CHECK(drc::point_to_segment_squared(0, 0, -5, 5, 5, -5) == 0);
  CHECK(drc::point_to_segment_squared(3, 0, 0, 0, 10, 10) == 5);  // 9 - trunc(4.5)
  CHECK(drc::point_to_segment_squared(5, 5, 0, 0, 10, 0) == 25);
  CHECK(drc::point_to_segment_squared(-3, 4, 0, 0, 10, 0) == 25);   // projection before the start
  CHECK(drc::point_to_segment_squared(13, 4, 0, 0, 10, 0) == 25);   // projection past the end
  CHECK(drc::point_to_segment_squared(100000, 100001, 0, 0, 300000, 299999) == 1345);
  CHECK(drc::point_to_segment_squared(7, 3, 0, 0, 1000003, 999983) == 8);
  CHECK(drc::point_to_segment_squared(123457, 98765, 1, 2, 987654, 876543) == 65296121);
}

TEST_CASE("radial clearance uses 64-bit sums and the border ratio", EUCLID) {
  CHECK(drc::radial_clear(4, 3, 5, 0, 1));         // 16 + 9 >= 25 (equal distances clear)
  CHECK_FALSE(drc::radial_clear(2, 2, 3, 0, 1));   // 8 < 9
  CHECK(drc::radial_clear(2, 2, 3, 2, 1));         // (4^2 + 4^2) >= 5^2
  CHECK_FALSE(drc::radial_clear(3, 3, 5, 1, 2));   // 7^2 + 7^2 < 11^2
  CHECK(drc::radial_clear(3590, 3590, 5000, 0, 1));
  CHECK(drc::radial_clear(1000000, 1000000, 1414213, 0, 1));  // beyond 32 bits
  CHECK_FALSE(drc::radial_clear(1000000, 1000000, 1414214, 0, 1));
}

TEST_CASE("diagonal offsets are pinned (double-precision square root)", "[cap:drc-rule-semantics][req:Diagonal edge checks]") {
  CHECK(drc::diagonal_offset(3, 10, 10) == drc::DiagonalOffset{2, 2});
  CHECK(drc::diagonal_offset(5, 4, 3) == drc::DiagonalOffset{3, 4});
  CHECK(drc::diagonal_offset(5, 3, 4) == drc::DiagonalOffset{4, 3});
  CHECK(drc::diagonal_offset(10, 1, 1) == drc::DiagonalOffset{7, 7});
  CHECK(drc::diagonal_offset(7, 30000, 20) == drc::DiagonalOffset{0, 0});
  CHECK(drc::diagonal_offset(100, 50000, 50000) == drc::DiagonalOffset{70, 70});  // w*w wraps like Magic's int
}
