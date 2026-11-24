// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Sideoverlap (fringe) coupling (tasks.md 6.4): the legacy model with
// Q-EXTRACT-18 per mode, the fringe-halo model with Q-EXTRACT-17 per mode,
// and the values Magic printed for EX-009 and EX-012. Owner: EXTCAP.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cap_fixture.hpp"

namespace {

using Catch::Approx;
using hefesto::test::cap::Layout;
using hefesto::test::cap::lg;
using hefesto::test::cap::style;
namespace extract = hefesto::extract;

double mult(double cap) {
  return static_cast<double>(static_cast<float>(static_cast<double>(static_cast<float>(cap)) * 0.02));
}

}  // namespace

TEST_CASE("EX-009: legacy model, an edge over a plate couples and loses its perimeter term",
          "[cap:extract-capacitance][req:Legacy sideoverlap (fringe directly below the edge)]") {
  Layout l(style(" sideoverlap metal2 space metal1 6\n perimc metal2 space 9\n sidehalo 4\n"));
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  l.options().fringehalo = false;
  const auto r = l.extract_all();
  const std::size_t top = l.node_at("metal2", 5, 5);
  const std::size_t plate = l.node_at("metal1", -15, -15);
  CHECK(r.coupling.value(top, plate) == 240.0);  // 4 edges * 6 * 10
  CHECK(r.nodes[top].substrate_cap == 0.0);      // 360 - 4 * 90
}

namespace {

// Two identical sideoverlap rules, a plate under the left half and an
// in-plane neighbour that splits the top edge at x = 3.
Layout::Result two_rule_legacy(Layout& l) {
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal2", {3, 12, 10, 14});
  l.paint("metal1", {-20, -20, 5, 30});
  l.options().fringehalo = false;
  return l.extract_all();
}

const std::string kTwoRuleLegacy =
    " sideoverlap metal2 space metal1 6\n sideoverlap metal2 space metal1 6\n perimc metal2 space 9\n"
    " sidewall metal2 space space metal2 40\n sidehalo 4\n";

}  // namespace

TEST_CASE("Q-EXTRACT-18 with magic-compat ON: the accounted area is capped by the whole-segment overlap",
          "[cap:extract-capacitance][req:Legacy sideoverlap (fringe directly below the edge)]") {
  Layout l(style(kTwoRuleLegacy), true);
  const auto r = two_rule_legacy(l);
  const std::size_t top = l.node_at("metal2", 5, 5);
  // Top edge: sub-segment 3..10 accounts 2 + 2 capped by 5 -> 4; 0..3
  // accounts 3 + 3 capped by 5 -> 5; left edge 10; bottom edge 5.
  CHECK(r.nodes[top].substrate_cap == 360.0 - 9.0 * (4 + 5 + 10 + 5));
}

TEST_CASE("Q-EXTRACT-18 with magic-compat OFF: once per tile, capped by the sub-segment overlap",
          "[cap:extract-capacitance][req:Legacy sideoverlap (fringe directly below the edge)]") {
  Layout l(style(kTwoRuleLegacy), false);
  const auto r = two_rule_legacy(l);
  const std::size_t top = l.node_at("metal2", 5, 5);
  // The single-rule value: 2 + 3 on the top edge, 10 and 5.
  CHECK(r.nodes[top].substrate_cap == 360.0 - 9.0 * (2 + 3 + 10 + 5));
}

TEST_CASE("fringe-halo model: an edge over a plate couples F(m*h) of its length",
          "[cap:extract-capacitance][req:Fringe-halo sideoverlap]") {
  Layout l(style(" sideoverlap metal2 space metal1 6\n perimc metal2 space 9\n overlap metal2 metal1 20\n"
                 " areacap metal2 10\n sidewall metal2 space space metal2 40\n sidehalo 10\n"));
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  const auto r = l.extract_all();
  const std::size_t top = l.node_at("metal2", 5, 5);
  const std::size_t plate = l.node_at("metal1", -15, -15);
  const double fc = extract::fringe_fraction(mult(20), 10);  // m(metal2, metal1) = 0.4
  const double fs = extract::fringe_fraction(mult(10), 10);  // m(metal2, space) = 0.2
  CHECK(r.coupling.value(top, plate) == Approx(2000.0 + 4 * 6 * fc * 10).epsilon(1e-12));
  CHECK(lg(r.coupling.value(top, plate) - 2000.0) == "202.564");
  CHECK(r.nodes[top].substrate_cap == Approx(360.0 - 4 * 9 * fs * 10).epsilon(1e-12));
}

TEST_CASE("EX-012: a same-plane neighbour blocks the fringe (Magic's printed values)",
          "[cap:extract-capacitance][req:Nearest-neighbor shadowing along an edge]") {
  Layout l(style(" sideoverlap metal2 space metal1 6\n perimc metal2 space 9\n overlap metal2 metal1 20\n"
                 " sidehalo 10\n sidewall metal2 space space metal2 40\n"));
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal2", {13, 0, 23, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  const auto r = l.extract_all();
  const std::size_t top = l.node_at("metal2", 5, 5);
  const std::size_t nbr = l.node_at("metal2", 18, 5);
  const std::size_t plate = l.node_at("metal1", -15, -15);
  CHECK(lg(extract::node_cap_output(r.nodes[nbr].substrate_cap, 1)) == "270");
  CHECK(lg(extract::node_cap_output(r.nodes[top].substrate_cap, 1)) == "270");
  CHECK(lg(r.coupling.value(plate, top)) == "2185.38");
  CHECK(lg(r.coupling.value(top, nbr)) == "133.333");
  CHECK(lg(r.coupling.value(plate, nbr)) == "2181.64");
}

namespace {

// Two identical shielded sideoverlap rules; a metal2 shield over half of
// the right edge's fringe area above the plate (EX-011's layout).
Layout::Result two_rule_halo(Layout& l) {
  l.paint("metal3", {0, 0, 10, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  l.paint("metal2", {10, 0, 15, 10});
  return l.extract_all();
}

const std::string kTwoRuleHalo =
    " sideoverlap metal3 space metal1 6 metal2\n sideoverlap metal3 space metal1 6 metal2\n"
    " perimc metal3 space 9\n overlap metal3 metal1 20\n areacap metal3 10\n"
    " sidewall metal3 space space metal3 40\n sidehalo 10\n";

}  // namespace

TEST_CASE("Q-EXTRACT-17: duplicate rules sum the shielded fraction with magic-compat ON only",
          "[cap:extract-capacitance][req:Fringe-halo sideoverlap]") {
  Layout on(style(kTwoRuleHalo), true);
  const auto r_on = two_rule_halo(on);
  Layout off(style(kTwoRuleHalo), false);
  const auto r_off = two_rule_halo(off);
  const double on_cap = r_on.nodes[on.node_at("metal3", 5, 5)].substrate_cap;
  const double off_cap = r_off.nodes[off.node_at("metal3", 5, 5)].substrate_cap;
  // The shield piece x 10..15 of the right edge shields F_ms(5) - F_ms(0)
  // of the substrate fringe over the whole edge. The substrate loses
  // perimc * (sfrac - subfrac) * length; ON counts the shielded fraction
  // once per rule (twice), so its node keeps 9 * sub * 10 more.
  const double sub = extract::fringe_fraction(mult(10), 5);
  CHECK(on_cap - off_cap == Approx(9.0 * sub * 10).epsilon(1e-9));
  // The coupling does not depend on the mode.
  const std::size_t top = on.node_at("metal3", 5, 5);
  const std::size_t plate = on.node_at("metal1", -15, -15);
  CHECK(r_on.coupling.value(top, plate) == r_off.coupling.value(off.node_at("metal3", 5, 5),
                                                                 off.node_at("metal1", -15, -15)));
}

TEST_CASE("sideoverlap_caps runs the sideoverlap kind alone with the given model",
          "[cap:extract-capacitance][req:Legacy sideoverlap (fringe directly below the edge)]") {
  Layout l(style(" sideoverlap metal2 space metal1 6\n perimc metal2 space 9\n sidehalo 4\n"));
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  auto nodes = l.find_nodes();
  extract::CouplingTable table;
  REQUIRE(extract::sideoverlap_caps(nodes, l.inputs(), extract::FringeModel::legacy, table).ok());
  CHECK(table.value(l.node_at("metal2", 5, 5), l.node_at("metal1", -15, -15)) == 240.0);
}

// Q-EXTRACT-36 (e), both modes: the inside node of an edge is the left side
// of its tile (ExtGetRegion(tile, 0)); the left side of a two-sided split
// tile can be unvisited (Q-EXTRACT-31), and Magic then subtracts through a
// CLIENTDEFAULT region. An edge whose inside has no node subtracts nothing.
// The lookup here gives the metal2 tile no node, as such a side has none.
TEST_CASE("Q-EXTRACT-36 (e): an edge whose inside tile has no node subtracts no substrate capacitance",
          "[cap:extract-capacitance][req:Legacy sideoverlap (fringe directly below the edge)]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Layout l(style(" sideoverlap metal2 space metal1 6\n perimc metal2 space 9\n perimc metal1 space 3\n sidehalo 4\n"),
             on);
    l.paint("metal1", {-20, -20, 30, 30});
    l.paint("metal2", {0, 0, 10, 10});
    l.options().fringehalo = false;
    extract::NodeRegionList nodes = l.find_nodes();
    REQUIRE(nodes.size() == 2);
    const std::size_t plate = l.node_at("metal1", -15, -15);
    const hefesto::tech::PlaneId metal2 = l.plane("metal2");
    const hefesto::db::CellDef& cell = l.cell();
    extract::CapInputs in = l.inputs();
    in.node_of = [&cell, metal2, count = nodes.size()](hefesto::tech::PlaneId plane, const hefesto::db::TileView& tile,
                                                        bool) -> std::optional<std::size_t> {
      if (plane == metal2) {
        return std::nullopt;
      }
      return extract::client_node(cell.plane(plane), tile, count);
    };
    for (extract::NodeRegion& n : nodes) {
      n.substrate_cap = 1000.0;
    }
    extract::CouplingTable coupling;
    REQUIRE(extract::find_coupling(nodes, in, coupling).ok());
    CHECK(nodes[plate].substrate_cap == 1000.0);
    CHECK(nodes[1 - plate].substrate_cap == 1000.0);
    CHECK(coupling.value(0, 1) == 0.0);
  }
}
