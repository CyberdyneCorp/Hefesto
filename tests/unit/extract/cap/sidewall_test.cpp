// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Sidewall coupling and nearest-neighbour shadowing (tasks.md 6.3), with
// the values Magic printed for EX-006, EX-007 and EX-008, and the pinned
// single-precision denominator of Q-EXTRACT-20. Owner: EXTCAP.

#include <catch2/catch_test_macros.hpp>

#include "cap_fixture.hpp"

namespace {

using hefesto::test::cap::Layout;
using hefesto::test::cap::lg;
using hefesto::test::cap::style;
namespace extract = hefesto::extract;

Layout::Result two_wires(Layout& l) {
  l.paint("metal1", {0, 0, 10, 2});
  l.paint("metal1", {0, 4, 10, 6});
  l.options().fringehalo = false;
  return l.extract_all();
}

}  // namespace

TEST_CASE("EX-006: two parallel wires couple 2 * C/2 * L / s",
          "[cap:extract-capacitance][req:Sidewall coupling value]") {
  Layout l(style(" sidewall metal1 space space metal1 40\n sidehalo 4\n"));
  const auto r = two_wires(l);
  const std::size_t w1 = l.node_at("metal1", 5, 1);
  const std::size_t w2 = l.node_at("metal1", 5, 5);
  CHECK(r.coupling.value(w1, w2) == 200.0);
  CHECK(lg(r.coupling.value(w1, w2)) == "200");
}

TEST_CASE("EX-007: a sidewall offset adds to the separation",
          "[cap:extract-capacitance][req:Sidewall coupling value]") {
  Layout l(style(" sidewall metal1 space space metal1 40 0.5\n sidehalo 4\n"));
  const auto r = two_wires(l);
  CHECK(r.coupling.value(l.node_at("metal1", 5, 1), l.node_at("metal1", 5, 5)) == 160.0);
}

TEST_CASE("Q-EXTRACT-20: sep + offset is formed in single precision (pinned)",
          "[cap:extract-capacitance][req:Sidewall coupling value]") {
  // sep 3, offset 0.1: (float)3 + 0.1f = 3.0999999046325684, not 3.1.
  Layout l(style(" sidewall metal1 space space metal1 40 0.1\n sidehalo 4\n"));
  l.paint("metal1", {0, 0, 10, 2});
  l.paint("metal1", {0, 5, 10, 7});
  l.options().fringehalo = false;
  const auto r = l.extract_all();
  const double one_visit = 0x1.021084a63a12fp+6;  // 20 * 10 / 3.0999999046325684
  CHECK(one_visit != 200.0 / 3.1);
  CHECK(r.coupling.value(l.node_at("metal1", 5, 1), l.node_at("metal1", 5, 6)) == one_visit + one_visit);
}

TEST_CASE("EX-008: the middle wire shadows the third",
          "[cap:extract-capacitance][req:Nearest-neighbor shadowing along an edge]") {
  Layout l(style(" sidewall metal1 space space metal1 40\n sidehalo 10\n"));
  l.paint("metal1", {0, 0, 10, 2});
  l.paint("metal1", {0, 4, 10, 6});
  l.paint("metal1", {0, 8, 10, 10});
  l.options().fringehalo = false;
  const auto r = l.extract_all();
  const std::size_t a = l.node_at("metal1", 5, 1);
  const std::size_t b = l.node_at("metal1", 5, 5);
  const std::size_t c = l.node_at("metal1", 5, 9);
  CHECK(r.coupling.value(a, b) == 200.0);
  CHECK(r.coupling.value(b, c) == 200.0);
  CHECK(r.coupling.value(a, c) == 0.0);
}

TEST_CASE("a partly covering neighbour splits the edge; the rest sees the next wire",
          "[cap:extract-capacitance][req:Nearest-neighbor shadowing along an edge]") {
  Layout l(style(" sidewall metal1 space space metal1 40\n sidehalo 10\n"));
  l.paint("metal1", {0, 0, 10, 2});  // A
  l.paint("metal1", {0, 4, 4, 6});   // B covers x 0..4 of A's top edge
  l.paint("metal1", {0, 8, 10, 10});  // C
  l.options().fringehalo = false;
  const auto r = l.extract_all();
  const std::size_t a = l.node_at("metal1", 5, 1);
  const std::size_t b = l.node_at("metal1", 2, 5);
  const std::size_t c = l.node_at("metal1", 5, 9);
  // A-B: 20*4/2 from each edge; A-C over x 4..10: 20*6/6 from each edge.
  CHECK(r.coupling.value(a, b) == 80.0);
  CHECK(r.coupling.value(a, c) == 40.0);
  // B-C: 20*4/2 twice.
  CHECK(r.coupling.value(b, c) == 80.0);
}

TEST_CASE("edges with no sidewall target are skipped with the fringe halo on",
          "[cap:extract-capacitance][req:Sidewall edge enumeration]") {
  // EX-010's situation: sideoverlap rules only, fringe halo on: no fringe.
  Layout l(style(" sideoverlap metal2 space metal1 6\n perimc metal2 space 9\n overlap metal2 metal1 20\n"
                 " sidehalo 10\n"));
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("metal1", {-20, -20, 30, 30});
  l.options().fringehalo = true;
  const auto r = l.extract_all();
  const std::size_t top = l.node_at("metal2", 5, 5);
  const std::size_t plate = l.node_at("metal1", -15, -15);
  CHECK(r.nodes[top].substrate_cap == 360.0);
  CHECK(r.coupling.value(top, plate) == 2000.0);
}

TEST_CASE("sidewall_caps runs the sidewall kind alone",
          "[cap:extract-capacitance][req:Sidewall edge enumeration]") {
  Layout l(style(" sidewall metal1 space space metal1 40\n sidehalo 4\n"));
  l.paint("metal1", {0, 0, 10, 2});
  l.paint("metal1", {0, 4, 10, 6});
  l.options().fringehalo = false;
  const auto nodes = l.find_nodes();
  extract::CouplingTable table;
  REQUIRE(extract::sidewall_caps(nodes, l.inputs(), table).ok());
  CHECK(table.value(l.node_at("metal1", 5, 1), l.node_at("metal1", 5, 5)) == 200.0);
}

TEST_CASE("a clip area limits the edges searched for sidewall coupling",
          "[cap:extract-capacitance][req:Sidewall edge enumeration]") {
  Layout l(style(" sidewall metal1 space space metal1 40\n sidehalo 4\n"));
  l.paint("metal1", {0, 0, 10, 2});
  l.paint("metal1", {0, 4, 10, 6});
  l.options().fringehalo = false;
  auto nodes = l.find_nodes();
  auto in = l.inputs();
  in.clip = hefesto::db::Rect{0, 0, 5, 3};
  extract::CouplingTable table;
  REQUIRE(extract::find_coupling(nodes, in, table).ok());
  // Only the lower wire's top edge, clipped to x 0..5, is searched.
  CHECK(table.value(l.node_at("metal1", 5, 1), l.node_at("metal1", 5, 5)) == 50.0);
}
