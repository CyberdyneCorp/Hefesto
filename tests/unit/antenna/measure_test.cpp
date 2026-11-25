// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The antenna measurement and ratio test (antenna/measure.hpp; capability
// antenna-check "Per-plane progressive evaluation", "Antenna area
// measurement", "Antenna ratio test"; tasks.md 15.3): the check planes
// (Q-MISC-6 fix), the restricted trace (Q-MISC-7 per mode), the surface,
// sidewall and contact-cut areas (Q-MISC-4 per mode, Q-MISC-3) and the
// gate-only, diffusion and cumulative tests (Q-MISC-1 per mode).
// Owner: ANTENNA.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "antenna_fixture.hpp"

using namespace antenna_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

namespace {

std::vector<tech::PlaneId> planes_of(const std::vector<antenna::CheckPlane>& checks) {
  std::vector<tech::PlaneId> out;
  for (const antenna::CheckPlane& c : checks) {
    out.push_back(c.plane);
  }
  return out;
}

antenna::CheckPlane check_at(const Session& s, std::string_view plane) {
  const antenna::RuleView rules = s.rules();
  return antenna::CheckPlane{s.plane(plane), rules.plane_order(s.plane(plane))};
}

// Any tile of `type` on `plane` of the traced net.
bool traced(const antenna::TracedNet& net, tech::PlaneId plane, tech::TileType type) {
  bool found = false;
  (void)net.plane(plane).enumerate(db::whole_plane(), tech::TypeMask::of(type), [&](const db::TileView&) {
    found = true;
    return false;
  });
  return found;
}

// gate nfet, poly, pc, metal1, via1 to a metal2 plate.
void gate_to_metal2(Session& s) {
  s.paint("nfet", db::Rect{0, 0, 2, 10});
  s.paint("poly", db::Rect{2, 0, 6, 10});
  s.paint("pc", db::Rect{6, 0, 10, 10});
  s.paint("metal1", db::Rect{10, 0, 20, 10});
  s.paint("via1", db::Rect{20, 0, 30, 10});
  s.paint("metal2", db::Rect{30, -100, 200, 100});
}

}  // namespace

TEST_CASE("check planes of a three-metal stack", "[cap:antenna-check][req:Per-plane progressive evaluation]") {
  Session s;
  const auto checks = antenna::check_planes(s.rules(), s.technology(), s.plane("active"));
  CHECK(planes_of(checks) == std::vector<tech::PlaneId>{s.plane("metal1"), s.plane("metal2"), s.plane("metal3")});
}

// Q-MISC-6 (fix, both modes), regression: an order no plane has is skipped
// instead of checking the previous plane again (Magic) or an uninitialised one.
TEST_CASE("check planes skip a gap in the plane order", "[cap:antenna-check][req:Per-plane progressive evaluation]") {
  Session s;
  extract::ExtStyle style = *s.editor.extract.current;
  style.plane_order[s.plane("metal1")] = 2;
  style.plane_order[s.plane("metal2")] = 4;  // no plane has order 3
  style.plane_order[s.plane("metal3")] = 5;
  const antenna::RuleView rules = antenna::RuleView::from_style(style);
  const auto checks = antenna::check_planes(rules, s.technology(), s.plane("active"));
  CHECK(planes_of(checks) == std::vector<tech::PlaneId>{s.plane("metal1"), s.plane("metal2"), s.plane("metal3")});
  REQUIRE(checks.size() == 3);
  CHECK(checks[1].order == 4);
}

TEST_CASE("the trace stops at the check plane", "[cap:antenna-check][req:Per-plane progressive evaluation]") {
  // capc (declared before via1) is the first contact homed on metal1, so
  // with magic-compat ON only it is restricted; OFF restricts via1 as well.
  Session s(tech_text(partial_rules()), false);
  gate_to_metal2(s);
  const antenna::RuleView rules = s.rules();
  const auto at_m1 = s.trace(db::Rect{0, 0, 1, 1}, "nfet", rules, check_at(s, "metal1"), false);
  CHECK(traced(at_m1, s.plane("metal1"), s.type("metal1")));
  CHECK_FALSE(traced(at_m1, s.plane("metal2"), s.type("metal2")));
  const auto at_m2 = s.trace(db::Rect{0, 0, 1, 1}, "nfet", rules, check_at(s, "metal2"), false);
  CHECK(traced(at_m2, s.plane("metal2"), s.type("metal2")));
}

// Q-MISC-7 (compat): ON, as Magic, only the first contact homed on the
// check plane (by type number) is restricted.
TEST_CASE("two contact types on the check plane with magic-compat ON",
          "[cap:antenna-check][req:Per-plane progressive evaluation]") {
  Session s;
  const antenna::RuleView rules = s.rules();
  const auto table = antenna::restricted_connect(rules, s.technology(), check_at(s, "metal1"), true);
  CHECK_FALSE(table(s.type("capc")).test(s.type("metal2")));
  CHECK(table(s.type("via1")).test(s.type("metal2")));
  gate_to_metal2(s);
  const auto net = s.trace(db::Rect{0, 0, 1, 1}, "nfet", rules, check_at(s, "metal1"), true);
  CHECK(traced(net, s.plane("metal2"), s.type("metal2")));  // through the unrestricted via1
}

TEST_CASE("two contact types on the check plane with magic-compat OFF",
          "[cap:antenna-check][req:Per-plane progressive evaluation]") {
  Session s;
  const antenna::RuleView rules = s.rules();
  const auto table = antenna::restricted_connect(rules, s.technology(), check_at(s, "metal1"), false);
  CHECK_FALSE(table(s.type("capc")).test(s.type("metal2")));
  CHECK_FALSE(table(s.type("via1")).test(s.type("metal2")));
  CHECK(table(s.type("via1")).test(s.type("metal1")));
  CHECK(table(s.type("metal1")) == s.technology().connects_to(s.type("metal1")));
}

TEST_CASE("gate and tie-down areas of the traced net", "[cap:antenna-check][req:Per-plane progressive evaluation]") {
  Session s(tech_text(kOrder + " model partial surface\n antenna metal1 surface 400 none\n tiedown poly\n" + kDevice));
  gate_to_metal2(s);
  const antenna::RuleView rules = s.rules();
  const antenna::MeasureSetup setup = s.setup(rules);
  const auto net = s.trace(db::Rect{0, 0, 1, 1}, "nfet", rules, check_at(s, "metal1"));
  CHECK(antenna::gate_area(net, setup, s.plane("active")).area == 20);
  CHECK(antenna::gate_area(net, setup, s.plane("active")).last == db::Rect{0, 0, 2, 10});
  CHECK(antenna::tiedown_area(net, setup).area == 40);  // the poly
}

TEST_CASE("surface area of plain metal", "[cap:antenna-check][req:Antenna area measurement]") {
  Session s;
  s.paint("metal1", db::Rect{0, 0, 100, 100});
  const antenna::RuleView rules = s.rules();
  const auto check = check_at(s, "metal1");
  const auto net = s.trace(db::Rect{0, 0, 1, 1}, "metal1", rules, check);
  const auto areas = antenna::antenna_areas(net, s.setup(rules), check);
  CHECK(areas.area[s.type("metal1")] == 10000);
  CHECK(areas.last == db::Rect{0, 0, 100, 100});
}

TEST_CASE("sidewall area of an isolated wire", "[cap:antenna-check][req:Antenna area measurement]") {
  Session s(tech_text(kOrder + " model partial\n height metal1 1.0 1.0\n antenna metal1 sidewall 400 none\n" +
                      kDevice));
  s.paint("metal1", db::Rect{0, 0, 100, 10});
  const antenna::RuleView rules = s.rules();
  const auto check = check_at(s, "metal1");
  const auto net = s.trace(db::Rect{0, 0, 1, 1}, "metal1", rules, check);
  const auto areas = antenna::antenna_areas(net, s.setup(rules), check);
  const float thick = rules.thickness(s.type("metal1"));
  CHECK(areas.area[s.type("metal1")] == static_cast<std::int64_t>(220.0F * thick));
  CHECK(areas.area[s.type("metal1")] > 0);
}

// Q-MISC-4 (compat): `squares 10 20 20` (border 10, size 20, separation
// 20) at scalefactor 10 on a 10 x 10 via1 tile.
TEST_CASE("contact cut area with mixed units with magic-compat ON", "[cap:antenna-check][req:Antenna area measurement]") {
  Session s(tech_text(partial_rules(" antenna via1 surface 400 none\n")));
  s.paint("via1", db::Rect{0, 0, 10, 10});
  const antenna::RuleView rules = s.rules();
  const auto check = check_at(s, "metal1");
  const auto net = s.trace(db::Rect{0, 0, 1, 1}, "via1", rules, check);
  const auto areas = antenna::antenna_areas(net, s.setup(rules, true), check);
  CHECK(areas.area[s.type("metal1")] == 4);  // nx = trunc((10 - 20) / 40) = 0 -> 1, C = 1 * trunc(2.0^2)
  CHECK(areas.area[s.type("via1")] == 4);
}

TEST_CASE("contact cut area with magic-compat OFF", "[cap:antenna-check][req:Antenna area measurement]") {
  Session s(tech_text(partial_rules(" antenna via1 surface 400 none\n")), false);
  s.paint("via1", db::Rect{0, 0, 10, 10});
  const antenna::RuleView rules = s.rules();
  const auto check = check_at(s, "metal1");
  const auto net = s.trace(db::Rect{0, 0, 1, 1}, "via1", rules, check, false);
  const auto areas = antenna::antenna_areas(net, s.setup(rules, false), check);
  CHECK(areas.area[s.type("metal1")] == 16);  // nx = ny = trunc((100 - 20) / 40) = 2, C = trunc(4 * 4.0)
  CHECK(areas.area[s.type("via1")] == 16);
}

TEST_CASE("a contact with no cut rule counts nothing", "[cap:antenna-check][req:Antenna area measurement]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(tech_text(partial_rules(" antenna capc surface 400 none\n")), compat);
    s.paint("capc", db::Rect{0, 0, 10, 10});
    const antenna::RuleView rules = s.rules();
    const auto check = check_at(s, "metal1");
    const auto net = s.trace(db::Rect{0, 0, 1, 1}, "capc", rules, check, compat);
    const auto areas = antenna::antenna_areas(net, s.setup(rules, compat), check);
    CHECK(areas.area[s.type("metal1")] == 0);
    CHECK(areas.area[s.type("capc")] == 0);
  }
}

TEST_CASE("contact cuts: the no-rule and zero-pitch guards", "[cap:antenna-check][req:Antenna area measurement]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const antenna::ContactCuts none = antenna::contact_cuts(10, 10, std::nullopt, compat);
    CHECK(none.area == 0);
    CHECK(none.perimeter == 0);
    const antenna::ContactCuts zero = antenna::contact_cuts(10, 10, antenna::CutRule{0, 0, 0, 10}, compat);
    CHECK(zero.nx == 1);
    CHECK(zero.ny == 1);
  }
  // The sidewall cut perimeter: nx * ny * 4 * s / scalefactor.
  const antenna::ContactCuts big = antenna::contact_cuts(100, 100, antenna::CutRule{20, 20, 10, 10}, true);
  CHECK(big.nx == 2);
  CHECK(big.perimeter == 2 * 2 * 80 / 10);
}

// Q-MISC-3 (fix): a layer with no calculation type contributes no area.
TEST_CASE("a layer with no calculation type counts nothing", "[cap:antenna-check][req:Antenna area measurement]") {
  Session s(tech_text(kOrder + " antenna metal1 400 none\n model partial surface\n" + kDevice));
  s.paint("metal1", db::Rect{0, 0, 100, 100});
  const antenna::RuleView rules = s.rules();
  const auto check = check_at(s, "metal1");
  const auto net = s.trace(db::Rect{0, 0, 1, 1}, "metal1", rules, check);
  CHECK(antenna::antenna_areas(net, s.setup(rules), check).area[s.type("metal1")] == 0);
}

namespace {

std::vector<std::int64_t> metal1_area(const Session& s, std::int64_t area) {
  std::vector<std::int64_t> out(s.technology().type_count(), 0);
  out[s.type("metal1")] = area;
  return out;
}

}  // namespace

TEST_CASE("gate-only violation", "[cap:antenna-check][req:Antenna ratio test]") {
  Session s;
  const antenna::RuleView rules = s.rules();
  const auto e = antenna::evaluate(s.setup(rules), metal1_area(s, 10000), 20, 0, check_at(s, "metal1"));
  CHECK(e.metal == 25.0);
  CHECK(e.metal_violation);
  CHECK_FALSE(e.via_violation);
  CHECK(e.limit == 400.0F);
  CHECK(antenna::effective_ratio(e.limit, e.metal, 20) == 500.0F);
  // No gate area: no violation (Magic avoids the infinite ratio).
  CHECK_FALSE(antenna::evaluate(s.setup(rules), metal1_area(s, 10000), 0, 0, check_at(s, "metal1")).violation());
}

TEST_CASE("diffusion protects the net", "[cap:antenna-check][req:Antenna ratio test]") {
  Session s(tech_text(kOrder + " model partial surface\n antenna metal1 surface 400 2200\n tiedown ndiff\n" + kDevice));
  const antenna::RuleView rules = s.rules();
  const auto e = antenna::evaluate(s.setup(rules), metal1_area(s, 10000), 20, 50, check_at(s, "metal1"));
  CHECK(e.limit == 2600.0F);
  CHECK(e.metal < 3.85);
  CHECK_FALSE(e.violation());
}

TEST_CASE("a layer is exempt when diffusion is present", "[cap:antenna-check][req:Antenna ratio test]") {
  Session s;
  const antenna::RuleView rules = s.rules();
  const auto e = antenna::evaluate(s.setup(rules), metal1_area(s, 1000000), 20, 50, check_at(s, "metal1"));
  CHECK(e.metal == 0.0);
  CHECK_FALSE(e.violation());
}

// Q-MISC-1 (compat): ON, as Magic, the cumulative model sums nothing.
TEST_CASE("the cumulative model with magic-compat ON reports nothing", "[cap:antenna-check][req:Antenna ratio test]") {
  Session s(tech_text(kOrder + " model cumulative surface\n antenna metal1 surface 400 none\n" + kDevice));
  const antenna::RuleView rules = s.rules();
  const auto e = antenna::evaluate(s.setup(rules, true), metal1_area(s, 10000), 20, 0, check_at(s, "metal1"));
  CHECK(e.metal == 0.0);
  CHECK_FALSE(e.violation());
}

TEST_CASE("the cumulative model with magic-compat OFF sums every type", "[cap:antenna-check][req:Antenna ratio test]") {
  Session s(tech_text(kOrder + " model cumulative surface\n antenna metal1 surface 400 none\n"
                      " antenna via2 surface 400 none\n" + kDevice), false);
  const antenna::RuleView rules = s.rules();
  std::vector<std::int64_t> areas = metal1_area(s, 10000);
  areas[s.type("via2")] = 12000;  // a contact homed on metal2: still counted at metal1
  const auto e = antenna::evaluate(s.setup(rules, false), areas, 20, 0, check_at(s, "metal1"));
  CHECK(e.metal == 25.0);
  CHECK(e.via == 30.0);
  CHECK(e.metal_violation);
  CHECK(e.via_violation);
}

TEST_CASE("the cumulative model includes every plane up to the check plane",
          "[cap:antenna-check][req:Per-plane progressive evaluation]") {
  Session s(tech_text(kOrder + " model cumulative surface\n antenna metal1 surface 400 none\n"
                      " antenna metal2 surface 400 none\n" + kDevice));
  const antenna::RuleView rules = s.rules();
  CHECK(antenna::plane_included(rules, s.plane("metal1"), check_at(s, "metal2")));
  CHECK(antenna::plane_included(rules, s.plane("metal2"), check_at(s, "metal2")));
  CHECK_FALSE(antenna::plane_included(rules, s.plane("metal3"), check_at(s, "metal2")));
  Session partial;
  const antenna::RuleView p = partial.rules();
  CHECK_FALSE(antenna::plane_included(p, partial.plane("metal1"), check_at(partial, "metal2")));
  CHECK(antenna::plane_included(p, partial.plane("metal2"), check_at(partial, "metal2")));
}
