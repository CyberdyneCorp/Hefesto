// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::NetNameMap (Q-SIM-24): the map is searchable by `layoutName` and by
// `spiceName`, and the two names of one node may differ.
//
// Owner: SIMPROBE (written by SCAFFOLD, tasks.md 4.2).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

#include "hefesto/sim/probe/net_name_map.hpp"

namespace sim = hefesto::sim;

namespace {

sim::NetNameEntry entry(std::string layout, std::string spice, std::string sim_name, std::string rnode = {}) {
  return sim::NetNameEntry{std::move(layout), std::move(spice), std::move(sim_name), {}, std::move(rnode)};
}

}  // namespace

TEST_CASE("net name map finds an entry by layout name and by netlist name",
          "[cap:simulation-netlist-preparation][req:Net name map]") {
  // The scenario "Renamed node keeps its layout name": nets OUT and out,
  // the second renamed out__c1 by the netlister.
  const sim::NetNameMap map{{
      entry("out", "out__c1", "xdut.out__c1"),
      entry("OUT", "OUT", "xdut.out"),
      entry("inv_0/mid", "inv_0/mid", "xdut.xinv_0.mid"),
  }};
  REQUIRE(map.entries().size() == 3);
  CHECK(map.entries()[0].spice_name == "OUT");  // sorted by spice name
  CHECK(map.entries()[2].spice_name == "out__c1");

  const auto* by_layout = map.find_layout_name("out");
  REQUIRE(by_layout != nullptr);
  CHECK(by_layout->spice_name == "out__c1");
  const auto* by_spice = map.find_spice_name("out__c1");
  CHECK(by_spice == by_layout);

  CHECK(map.find_layout_name("Out") == nullptr);  // names are never folded
  CHECK(map.find_spice_name("out") == nullptr);
  CHECK(map.find_layout_name("inv_0/mid")->sim_name == "xdut.xinv_0.mid");
}

TEST_CASE("net name map prefers the net entry over its rnode entries",
          "[cap:simulation-netlist-preparation][req:Net name map]") {
  const sim::NetNameMap map{{
      entry("a", "a", "xdut.a.n2", "n2"),
      entry("a", "a", "xdut.a.n1", "n1"),
      entry("a", "a", "xdut.a"),
  }};
  REQUIRE(map.find_layout_name("a") != nullptr);
  CHECK(map.find_layout_name("a")->rnode.empty());
  CHECK(map.find_spice_name("a")->rnode.empty());
  CHECK(map.entries()[1].rnode == "n1");
}

TEST_CASE("an empty net name map finds nothing", "[cap:simulation-netlist-preparation][req:Net name map]") {
  const sim::NetNameMap map;
  CHECK(map.empty());
  CHECK(map.find_layout_name("a") == nullptr);
  CHECK(map.find_spice_name("a") == nullptr);
}
