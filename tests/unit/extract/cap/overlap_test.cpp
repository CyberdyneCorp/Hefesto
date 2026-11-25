// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Overlap coupling with shielding (tasks.md 6.2), with the values Magic
// printed for EX-004 and EX-005. Owner: EXTCAP.

#include <catch2/catch_test_macros.hpp>

#include "cap_fixture.hpp"

namespace {

using hefesto::test::cap::Layout;
using hefesto::test::cap::style;
namespace extract = hefesto::extract;

}  // namespace

TEST_CASE("EX-004: plate over plate cancels the upper area term and couples the pair",
          "[cap:extract-capacitance][req:Overlap coupling with shielding]") {
  Layout l(style(" areacap metal2 10\n overlap metal2 metal1 20\n"));
  l.paint("metal1", {0, 0, 10, 10});
  l.paint("metal2", {0, 0, 10, 10});
  const auto r = l.extract_all();
  const std::size_t a = l.node_at("metal1", 5, 5);
  const std::size_t b = l.node_at("metal2", 5, 5);
  CHECK(r.nodes[b].substrate_cap == 0.0);
  CHECK(r.nodes[a].substrate_cap == 0.0);
  CHECK(r.coupling.value(a, b) == 2000.0);
  CHECK(r.coupling.pairs().size() == 1);
}

TEST_CASE("EX-005: a partial shield on an intermediate plane reduces the overlap",
          "[cap:extract-capacitance][req:Overlap coupling with shielding]") {
  Layout l(style(" overlap metal3 metal1 8 metal2\n areacap metal3 10\n"));
  l.paint("metal1", {0, 0, 10, 10});
  l.paint("metal2", {0, 0, 10, 4});
  l.paint("metal3", {0, 0, 10, 10});
  const auto r = l.extract_all();
  const std::size_t a = l.node_at("metal1", 5, 5);
  const std::size_t b = l.node_at("metal2", 5, 2);
  const std::size_t c = l.node_at("metal3", 5, 5);
  CHECK(r.coupling.value(a, c) == 480.0);  // 8 * (100 - 40)
  CHECK(r.coupling.value(b, c) == 0.0);    // the shield is an overlap target with no capacitance
  // metal3 loses 10*60 over metal1 and 10*40 over the shield (a target too).
  CHECK(r.nodes[c].substrate_cap == 0.0);
}

TEST_CASE("an overlap of one node with itself only removes the area term",
          "[cap:extract-capacitance][req:Overlap coupling with shielding]") {
  Layout l(style(" areacap metal2 10\n overlap metal2 metal1 20\n"));
  l.paint("metal1", {0, 0, 10, 10});
  l.paint("metal2", {0, 0, 10, 10});
  l.paint("via", {4, 4, 6, 6});
  const auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  CHECK(r.coupling.pairs().empty());
  // metal2 area 96 (the via image counts as metal2's residue, area cap of
  // metal2 too), all of it over metal1 of the same node.
  CHECK(r.nodes[0].substrate_cap == 0.0);
}

TEST_CASE("overlap_caps runs the overlap kind alone",
          "[cap:extract-capacitance][req:Overlap coupling with shielding]") {
  Layout l(style(" areacap metal2 10\n overlap metal2 metal1 20\n sidewall metal2 space space metal2 40\n"
                 " sidehalo 4\n"));
  l.paint("metal1", {0, 0, 10, 10});
  l.paint("metal2", {0, 0, 10, 2});
  l.paint("metal2", {0, 4, 10, 6});
  auto nodes = l.find_nodes();
  extract::CouplingTable table;
  REQUIRE(extract::overlap_caps(nodes, l.inputs(), table).ok());
  const std::size_t a = l.node_at("metal1", 5, 5);
  const std::size_t w1 = l.node_at("metal2", 5, 1);
  const std::size_t w2 = l.node_at("metal2", 5, 5);
  CHECK(table.value(a, w1) == 400.0);
  CHECK(table.value(a, w2) == 400.0);
  CHECK(table.value(w1, w2) == 0.0);  // no sidewall in this pass
}
