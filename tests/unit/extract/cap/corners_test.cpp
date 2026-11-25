// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Corner fringe to substrate and corner coupling (tasks.md 6.5), with the
// values Magic printed for EX-013 and Q-EXTRACT-24 per mode. Owner: EXTCAP.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cap_fixture.hpp"

namespace {

using Catch::Approx;
using hefesto::test::cap::Layout;
using hefesto::test::cap::lg;
using hefesto::test::cap::style;
namespace extract = hefesto::extract;

// Magic's exts_overlapMult: (float)cap * 0.02, stored as float.
double mult(double cap) {
  return static_cast<double>(static_cast<float>(static_cast<double>(static_cast<float>(cap)) * 0.02));
}

const std::string kEx013 =
    " areacap metal2 10\n perimc metal2 space 9\n overlap metal2 metal1 20\n sideoverlap metal2 space metal1 6\n"
    " sidehalo 10\n";

struct Ex013 {
  double ell = 0;
  double lone = 0;
  double coupling = 0;
};

Ex013 run_ex013(bool corners, double factor) {
  Layout l(style(kEx013));
  l.paint("metal2", {0, 0, 10, 4});
  l.paint("metal2", {0, 4, 4, 10});
  l.paint("metal2", {100, 100, 110, 110});
  l.paint("metal1", {-20, -20, 30, 30});
  l.options().fringehalo = true;
  l.options().corners = corners;
  l.options().corner_factor = factor;
  const auto r = l.extract_all();
  Ex013 out;
  out.ell = r.nodes[l.node_at("metal2", 2, 2)].substrate_cap;
  out.lone = r.nodes[l.node_at("metal2", 105, 105)].substrate_cap;
  out.coupling = r.coupling.value(l.node_at("metal2", 2, 2), l.node_at("metal1", -15, -15));
  return out;
}

}  // namespace

TEST_CASE("EX-013: convex and concave corners add and remove c*k/m to substrate",
          "[cap:extract-capacitance][req:Corner fringe capacitance to substrate]") {
  const Ex013 none = run_ex013(false, 1.0);
  CHECK(lg(none.ell) == "360");
  CHECK(lg(none.lone) == "1360");
  CHECK(lg(none.coupling) == "1280");
  const Ex013 one = run_ex013(true, 1.0);
  CHECK(lg(one.ell) == "540");  // 5 convex - 1 concave = 4 * 9 / 0.2
  CHECK(lg(one.lone) == "1540");
  CHECK(lg(one.coupling) == "1280");
  const Ex013 half = run_ex013(true, 0.5);
  CHECK(lg(half.ell) == "450");
  CHECK(lg(half.lone) == "1450");
  CHECK(lg(half.coupling) == "1280");
}

namespace {

// An ndiff L whose notch holds ndiffh, which connects to ndiff (same node)
// and has a perimeter rule against it: the notch corner's free quadrant is
// not space.
double notch(bool magic_compat) {
  Layout l(style(" areacap ndiff 30\n perimc ndiff space 20\n perimc ndiff ndiffh 5\n sidehalo 10\n"),
           magic_compat);
  l.paint("ndiff", {0, 0, 10, 4});
  l.paint("ndiff", {0, 4, 4, 10});
  l.paint("ndiffh", {4, 4, 6, 6});
  l.options().fringehalo = true;
  l.options().corners = true;
  const auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  return r.nodes[0].substrate_cap;
}

}  // namespace

TEST_CASE("Q-EXTRACT-24 with magic-compat ON: a corner is concave by the type test",
          "[cap:extract-capacitance][req:Corner fringe capacitance to substrate]") {
  const double on = notch(true);
  const double off = notch(false);
  // The ndiff | ndiffh segment's left end: ON subtracts 5 * (1 / 0.6).
  const double m = mult(30);
  CHECK(off - on == Approx(5.0 * (1.0 / m)).epsilon(1e-12));
}

TEST_CASE("Q-EXTRACT-24 with magic-compat OFF: a concave corner needs a free quadrant of space",
          "[cap:extract-capacitance][req:Corner fringe capacitance to substrate]") {
  Layout l(style(" areacap ndiff 30\n perimc ndiff space 20\n sidehalo 10\n"), false);
  l.paint("ndiff", {0, 0, 10, 4});
  l.paint("ndiff", {0, 4, 4, 10});
  l.options().fringehalo = true;
  l.options().corners = true;
  const auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  // Free quadrant of space: OFF still finds the concave corner (5 convex, 1
  // concave), as ON does.
  const double m = mult(30);
  const double base = 64 * 30.0 + 40 * 20.0;
  CHECK(r.nodes[0].substrate_cap == Approx(base + 4 * 20.0 * (1.0 / m)).epsilon(1e-12));
}

namespace {

double corner_coupling(bool corners) {
  Layout l(style(" perimc metal2 space 9\n overlap metal2 metal1 20\n sideoverlap metal2 space metal1 6\n"
                 " sidewall metal2 space space metal2 40\n sidehalo 10\n"));
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  l.options().fringehalo = true;
  l.options().corners = corners;
  const auto r = l.extract_all();
  return r.coupling.value(l.node_at("metal2", 5, 5), l.node_at("metal1", -15, -15));
}

}  // namespace

TEST_CASE("convex corners over a plate add four corner terms to the coupling",
          "[cap:extract-capacitance][req:Corner fringe coupling]") {
  const double mc = mult(20);
  const double f = extract::fringe_fraction(mc, 10);
  const double without = corner_coupling(false);
  const double with = corner_coupling(true);
  CHECK(with - without == Approx(4 * 6 * (1.0 / mc) * f * f).epsilon(1e-12));
}

TEST_CASE("corner_caps runs the corner kind alone with its factor",
          "[cap:extract-capacitance][req:Corner fringe coupling]") {
  Layout l(style(" perimc metal2 space 9\n overlap metal2 metal1 20\n sideoverlap metal2 space metal1 6\n"
                 " sidewall metal2 space space metal2 40\n sidehalo 10\n"));
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  auto nodes = l.find_nodes();
  extract::CouplingTable table;
  REQUIRE(extract::corner_caps(nodes, l.inputs(), 0.5, table).ok());
  const double mc = mult(20);
  const double f = extract::fringe_fraction(mc, 10);
  CHECK(table.value(l.node_at("metal2", 5, 5), l.node_at("metal1", -15, -15)) ==
        Approx(4 * 6 * (0.5 / mc) * f * f).epsilon(1e-12));
}
