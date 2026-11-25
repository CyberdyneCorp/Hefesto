// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Coupling to the substrate node and the `cap` records (tasks.md 6.6), with
// EX-016's layout and Q-EXTRACT-7's canonical order. Owner: EXTCAP.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "cap_fixture.hpp"

namespace {

using hefesto::test::cap::Layout;
using hefesto::test::cap::lg;
using hefesto::test::cap::style;
namespace extract = hefesto::extract;

}  // namespace

TEST_CASE("EX-016: coupling to the substrate node becomes node capacitance",
          "[cap:extract-capacitance][req:Coupling to the substrate node]") {
  Layout l(style(" substrate *psd,space,pwell well\n defaultareacap metal1 metal1 well 30\n"));
  l.paint("pwell", {0, 0, 20, 20});
  l.paint("metal1", {5, 5, 15, 15});
  auto nodes = l.find_nodes();
  // EXTDEV's substrate node: every tile of the well plane, space included.
  nodes.push_back(extract::NodeRegion{});
  const std::size_t sub = nodes.size() - 1;
  l.give_plane("well", sub);
  const std::size_t m = l.node_at("metal1", 10, 10);
  REQUIRE(extract::substrate_caps(nodes, l.inputs()).ok());
  extract::CouplingTable table;
  REQUIRE(extract::find_coupling(nodes, l.inputs(), table).ok());
  extract::relocate_substrate_coupling(nodes, sub, table);
  CHECK(nodes[m].substrate_cap == 3000.0);
  for (const auto& [key, value] : table.pairs()) {
    if (key.first == sub || key.second == sub) {
      CHECK(value == 0.0);
    }
  }
  std::vector<std::string> names(nodes.size(), "x");
  names[m] = "m";
  names[sub] = "well_n1073741817_n1073741817#";
  CHECK(extract::coupling_records(table, names, l.style().cscale).empty());
}

TEST_CASE("relocation moves every non-zero substrate pair and nothing else",
          "[cap:extract-capacitance][req:Coupling to the substrate node]") {
  extract::NodeRegionList nodes(4);
  nodes[1].substrate_cap = 5;
  extract::CouplingTable table;
  table.add(0, 1, 10);  // node 0 is the substrate
  table.add(2, 0, 7);
  table.add(1, 2, 3);
  extract::relocate_substrate_coupling(nodes, 0, table);
  CHECK(nodes[1].substrate_cap == 15);
  CHECK(nodes[2].substrate_cap == 7);
  CHECK(table.value(0, 1) == 0);
  CHECK(table.value(0, 2) == 0);
  CHECK(table.value(1, 2) == 3);
  extract::relocate_substrate_coupling(nodes, std::nullopt, table);  // no substrate node
  CHECK(table.value(1, 2) == 3);
}

TEST_CASE("cap records are name-keyed, smaller name first, divided by cscale, zeros dropped",
          "[cap:extract-capacitance][req:Coupling record output]") {
  extract::CouplingTable table;
  table.add(0, 1, 200);
  table.add(2, 1, 1234567);
  table.add(0, 2, 0);
  const std::vector<std::string> names{"w2", "w1", "a"};
  const auto records = extract::coupling_records(table, names, 1000);
  REQUIRE(records.size() == 2);
  CHECK(records[0] == extract::CouplingRecord{"a", "w1", 1234.567});
  CHECK(records[1] == extract::CouplingRecord{"w1", "w2", 0.2});
  CHECK(lg(records[0].value) == "1234.57");
}

TEST_CASE("node capacitance output clamps negative values to zero",
          "[cap:extract-capacitance][req:Coupling record output]") {
  CHECK(extract::node_cap_output(-1e-9, 1) == 0.0);
  CHECK(extract::node_cap_output(1760, 1000) == 1.76);
}
