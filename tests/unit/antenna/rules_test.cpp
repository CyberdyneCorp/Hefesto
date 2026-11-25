// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// antenna::RuleView over the extraction style a technology load installs
// (capability antenna-check "Antenna tech-file keywords grammar", "Antenna
// coefficient unit conversion", "Shared antenna definitions for LEF
// output"; tasks.md 15.1, 15.4). Owner: ANTENNA.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "antenna_fixture.hpp"
#include "hefesto/extract/tech/style_view.hpp"

using namespace antenna_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

TEST_CASE("RuleView: a full antenna line with both diffusion terms",
          "[cap:antenna-check][req:Antenna tech-file keywords grammar]") {
  Session s(tech_text(kOrder + " antenna metal1 sidewall 400 2200 0.5\n" + kDevice));
  const antenna::RuleView rules = s.rules();
  const tech::TileType m1 = s.type("metal1");
  CHECK(rules.calc_type(m1) == extract::AntennaModel::sidewall);
  CHECK(rules.ratio_gate(m1) == 400.0F);
  CHECK(rules.ratio_diff_const(m1) == 2200.0F);
  CHECK(rules.ratio_diff_prop(m1) == 0.5F);
  REQUIRE(rules.rules(m1).has_value());
  CHECK_FALSE(rules.rules(s.type("metal2")).has_value());
  // A type without a rule has Magic's defaults.
  CHECK(rules.calc_type(s.type("metal2")) == extract::AntennaModel::none);
  CHECK(rules.ratio_gate(s.type("metal2")) == 0.0F);
  CHECK(rules.ratio_diff_const(s.type("metal2")) == 0.0F);
}

TEST_CASE("RuleView: the calculation type defaults to the model's, `none` is infinite",
          "[cap:antenna-check][req:Antenna tech-file keywords grammar]") {
  Session s(tech_text(kOrder + " model partial surface\n antenna poly 50 none\n tiedown ndiff\n tiedown pc\n" +
                      kDevice));
  const antenna::RuleView rules = s.rules();
  const tech::TileType poly = s.type("poly");
  CHECK(rules.calc_type(poly) == extract::AntennaModel::surface);
  CHECK(rules.ratio_gate(poly) == 50.0F);
  CHECK(std::isinf(rules.ratio_diff_const(poly)));
  CHECK(rules.ratio_diff_prop(poly) == 0.0F);
  CHECK(rules.model() == (extract::kAntennaPartial | extract::kAntennaSurface));
  CHECK(rules.partial());
  // tiedown lines accumulate; the gate set is the mosfet line's types.
  CHECK(rules.tiedown_types().test(s.type("ndiff")));
  CHECK(rules.tiedown_types().test(s.type("pc")));
  CHECK(rules.gate_types() == tech::TypeMask::of(s.type("nfet")));
}

// Q-MISC-3 (fix): a layer with no calculation type keeps its ratios and
// contributes no antenna area.
TEST_CASE("RuleView: a model line after the antenna line gives no calculation type",
          "[cap:antenna-check][req:Antenna tech-file keywords grammar]") {
  Session s(tech_text(kOrder + " antenna poly 50 none\n model partial surface\n" + kDevice));
  const antenna::RuleView rules = s.rules();
  const tech::TileType poly = s.type("poly");
  CHECK(rules.calc_type(poly) == extract::AntennaModel::none);
  CHECK(rules.ratio_gate(poly) == 50.0F);
  CHECK(std::isinf(rules.ratio_diff_const(poly)));
}

TEST_CASE("RuleView: the plane order and noplaneordering",
          "[cap:antenna-check][req:Antenna tech-file keywords grammar]") {
  Session ordered(tech_text(partial_rules()));
  const antenna::RuleView rules = ordered.rules();
  CHECK(rules.has_plane_order());
  CHECK(rules.plane_order(ordered.plane("active")) == 1);
  CHECK(rules.plane_order(ordered.plane("metal3")) == 4);
  Session none(tech_text(" noplaneordering\n" + kDevice));
  CHECK_FALSE(none.rules().has_plane_order());
}

TEST_CASE("RuleView: micron units scale the proportional ratio by s^2 and the thickness by 1/s",
          "[cap:antenna-check][req:Antenna coefficient unit conversion]") {
  // scalefactor 10, expander 1: s = 10 * 10 / 1000 = 0.1 micron per unit.
  Session s(tech_text(" units microns\n" + kOrder + " height metal1 1.0 0.36\n antenna metal1 surface 400 2200 2.0\n" +
                      kDevice));
  const antenna::RuleView rules = s.rules();
  const tech::TileType m1 = s.type("metal1");
  CHECK(rules.ratio_gate(m1) == 400.0F);
  CHECK(rules.ratio_diff_const(m1) == 2200.0F);
  CHECK(rules.ratio_diff_prop(m1) == Catch::Approx(0.02).epsilon(1e-5));
  CHECK(rules.thickness(m1) == Catch::Approx(3.6).epsilon(1e-5));
}

TEST_CASE("RuleView: without micron units the coefficients are unconverted",
          "[cap:antenna-check][req:Antenna coefficient unit conversion]") {
  Session s(tech_text(kOrder + " antenna metal1 surface 400 2200 2.0\n" + kDevice));
  CHECK(s.rules().ratio_diff_prop(s.type("metal1")) == 2.0F);
}

TEST_CASE("RuleView: LEF's gate and tie-down sets are the antenna check's",
          "[cap:antenna-check][req:Shared antenna definitions for LEF output]") {
  Session s(tech_text(partial_rules()));
  diag::CollectingSink quiet;
  const extract::StyleView lef_style = extract::StyleView::from_technology(s.technology(), quiet);
  const antenna::RuleView lef = antenna::RuleView::from_view(lef_style);
  const antenna::RuleView check = s.rules();
  CHECK(lef.gate_types() == check.gate_types());
  CHECK(lef.tiedown_types() == check.tiedown_types());
  CHECK(lef.gate_types() == tech::TypeMask::of(s.type("nfet")));
  CHECK(lef.tiedown_types() == tech::TypeMask::of(s.type("ndiff")));
  // No extraction style: both sets empty (Q-LEFDEF-40).
  const antenna::RuleView empty = antenna::RuleView::from_view(extract::StyleView{});
  CHECK(empty.gate_types().empty());
  CHECK(empty.tiedown_types().empty());
}
