// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::resolve_probe (design D4 "Two namings, one node"; simulation-ui-and-
// crossprobe "Probing a layout net", step 3): the `layout_name` index first,
// then the map entry whose point lies on the selected node. The net-name
// maps are built by hand (SIMDECK builds them from the netlister, task 13.4).
//
// Owner: SIMPROBE.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/sim/probe/net_name_map.hpp"
#include "hefesto/sim/probe/probe.hpp"
#include "probe_fixture.hpp"

namespace sim = hefesto::sim;
namespace db = hefesto::db;
using hefesto::test::simprobe::Layout;

namespace {

sim::NetNameEntry entry(std::string layout, std::string spice, db::Point p, std::vector<std::string> path = {}) {
  sim::NetNameEntry e;
  e.layout_name = std::move(layout);
  e.spice_name = spice;
  e.sim_name = "xdut." + spice;
  e.point = sim::LayoutPoint{std::move(path), p};
  return e;
}

sim::OnSelectedNode predicate(const sim::LayoutSelection& s) {
  auto on_node = sim::selected_node_predicate(s);
  REQUIRE(on_node.ok());
  return on_node.value();
}

}  // namespace

TEST_CASE("Netlist name differs from the node name", "[cap:simulation-ui-and-crossprobe][req:Probing a layout net]") {
  // DUT `top`: nets labelled OUT and out (the netlist renames the second
  // out__c1, Q-SIM-6), and an unlabelled node gated by a transistor whose
  // map entry was named `@=g100,50` at another point of the node.
  Layout l;
  db::CellDef& top = l.cell("top");
  l.paint(top, "metal1", {0, 0, 10, 2});
  l.label(top, "OUT", "metal1", {1, 1, 1, 1});
  l.paint(top, "metal1", {0, 10, 10, 12});
  l.label(top, "out", "metal1", {1, 11, 1, 11});
  l.paint(top, "metal1", {0, 20, 30, 22});  // unlabelled
  const sim::NetNameMap map{{
      entry("OUT", "OUT", {1, 1}),
      entry("out", "out__c1", {1, 11}),
      entry("@=g100,50", "a_100_50#", {25, 21}),
  }};

  SECTION("the net labelled out resolves to out__c1 through layout_name") {
    const sim::LayoutSelection s = l.select(top, {}, {5, 11});
    const auto found = sim::resolve_probe(map, s, predicate(s));
    REQUIRE(found.ok());
    CHECK(found.value().spice_name == "out__c1");
    CHECK(found.value().sim_name == "xdut.out__c1");
  }
  SECTION("the unlabelled node resolves by connectivity to its terminal-named entry") {
    const sim::LayoutSelection s = l.select(top, {}, {2, 21});
    REQUIRE(l.name(s).ends_with("#"));  // the node name is not the map's layout_name
    const auto found = sim::resolve_probe(map, s, predicate(s));
    REQUIRE(found.ok());
    CHECK(found.value().spice_name == "a_100_50#");
  }
  SECTION("a node with no entry is not in the netlist") {
    l.paint(top, "metal1", {0, 30, 10, 32});
    l.label(top, "spare", "metal1", {1, 31, 1, 31});
    const sim::LayoutSelection s = l.select(top, {}, {5, 31});
    const auto found = sim::resolve_probe(map, s, predicate(s));
    REQUIRE_FALSE(found.ok());
    CHECK(found.error().code == "SIM-PROBE-001");
    CHECK(found.error().text == "Net spare is not in the netlist of this run");
  }
}

TEST_CASE("The connectivity fallback follows the node into instances",
          "[cap:simulation-ui-and-crossprobe][req:Probing a layout net]") {
  Layout l;
  db::CellDef& inv = l.cell("inv");
  l.paint(inv, "metal1", {0, 0, 10, 2});
  db::CellDef& buf = l.cell("buf");
  Layout::place(buf, inv, "inv_0", db::translation(100, 0));
  Layout::place(buf, inv, "inv_1", db::translation(200, 0));
  const sim::NetNameMap map{{
      entry("inv_1/other#", "inv_1/a_0_0#", {3, 1}, {"inv_1"}),
      entry("inv_0/other#", "inv_0/a_0_0#", {3, 1}, {"inv_0"}),
  }};
  const sim::LayoutSelection s = l.select(buf, {"inv_0"}, {7, 1});
  const auto found = sim::resolve_probe(map, s, predicate(s));
  REQUIRE(found.ok());
  CHECK(found.value().spice_name == "inv_0/a_0_0#");
}

TEST_CASE("Deep hierarchy: a 600-byte name resolves through the map",
          "[cap:simulation-ui-and-crossprobe][req:Probe names without length limits]") {
  Layout l;
  db::CellDef& leaf = l.cell("leaf");
  l.paint(leaf, "metal1", {0, 0, 10, 2});
  l.label(leaf, "net", "metal1", {1, 1, 1, 1});
  db::CellDef* child = &leaf;
  std::vector<std::string> path;
  std::string name = "net";
  for (int level = 0; level < 12; ++level) {
    db::CellDef& parent = l.cell("deep" + std::to_string(level));
    const std::string id = std::string(48, 'x') + std::to_string(level);
    Layout::place(parent, *child, id, db::kIdentity);
    path.insert(path.begin(), id);
    name = id + "/" + name;
    child = &parent;
  }
  REQUIRE(name.size() >= 600);
  const sim::NetNameMap map{{entry(name, "deepnet", {1, 1}, path)}};
  const sim::LayoutSelection s = l.select(*child, path, {5, 1});
  const auto found = sim::resolve_probe(map, s, nullptr);
  REQUIRE(found.ok());
  CHECK(found.value().layout_name == name);
}
