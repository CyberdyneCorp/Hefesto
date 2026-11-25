// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 round 3 additions to the flattening (openspec/changes/m7-extraction
// tasks.md 11.3, 12.1, 19.1): the root's port entries with their indices
// in node-table order, the local area and perimeter of device terminals
// and substrates, and the non-Tcl global rule of the standalone programs.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ext_fixture.hpp"

namespace extflat = hefesto::extflat;
using extflat::test::ExtFixture;
using extflat::test::GoldenCase;
using extflat::test::nltest_options;
using extflat::test::terminal_nodes;
using Strings = std::vector<std::string>;

namespace {

std::vector<std::pair<std::string, std::int32_t>> heads(const extflat::Netlist& n) {
  std::vector<std::pair<std::string, std::int32_t>> out;
  for (const extflat::PortEntry& e : n.root_ports) {
    out.emplace_back(e.chain.front().name, e.chain.front().index);
  }
  return out;
}

}  // namespace

// Regression (tasks.md 11.3): the netlist carried no port indices, so the
// writer numbered every root port as implicit, in reverse node order
// (`.subckt inv out in` for NL-SPICE-05).
TEST_CASE("the root's port entries carry their indices in node-table order",
          "[cap:ext-flattening-read][req:Use, port, cap, resist, attr and connect records]") {
  GoldenCase g{"NL-SPICE-05"};
  const auto n = g.flat("inv", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  const auto ports = heads(n.value());
  REQUIRE(ports.size() == 2);
  CHECK(std::find(ports.begin(), ports.end(), std::pair<std::string, std::int32_t>{"in", 0}) != ports.end());
  CHECK(std::find(ports.begin(), ports.end(), std::pair<std::string, std::int32_t>{"out", 1}) != ports.end());
  for (const extflat::PortEntry& e : n.value().root_ports) {
    CHECK(e.canonical == e.chain.front());
  }
}

TEST_CASE("every name of a port node is an entry with the node's canonical name",
          "[cap:ext-flattening-read][req:Use, port, cap, resist, attr and connect records]") {
  ExtFixture f;
  f.file("cell", "node a 0 0 0 0 m1\nnode b 0 0 0 10 m1\nnode c 0 0 0 20 m1\nequiv a b\nport b 2 0 10 1 11 m1\n");
  const auto n = f.flat("cell");
  REQUIRE(n.ok());
  // Both names of the port node (the top-level port `b` is canonical), and
  // not the node `c`, which is no port.
  REQUIRE(n.value().root_ports.size() == 2);
  for (const extflat::PortEntry& e : n.value().root_ports) {
    CHECK(e.canonical.name == "b");
    CHECK(e.chain.front().name != "c");
  }
}

// Regression (tasks.md 12.1, NL-SIM-02): ext2sim derives a bipolar
// device's L and W from its substrate node's area and perimeter, which
// the flattening did not pass on (`b b e c 0 0 0 0` for `8 5 0 0`).
TEST_CASE("device terminals and substrates carry their local area and perimeter",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  GoldenCase g{"NL-SIM-02"};
  auto options = nltest_options();
  options.subcircuits = false;
  const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(n.ok());
  const extflat::FlatDevice* bjt = nullptr;
  for (const extflat::FlatDevice& d : n.value().devices) {
    if (d.device_class == "bjt") {
      bjt = &d;
    }
  }
  REQUIRE(bjt != nullptr);
  REQUIRE_FALSE(bjt->substrate_area_perim.empty());
  // node "c" ... ndiff 40 26: area 40, perimeter 26 in the first class,
  // from which ext2sim writes `8 5`.
  CHECK(bjt->substrate_area_perim.front() == std::pair<std::int32_t, std::int32_t>{40, 26});
  CHECK(bjt->terminals.front().local_node == "top/b");
}

// Regression (tasks.md 19.1): hefesto-extcheck read every file twice to
// find the leaves ending in `!`; ReadOptions::bang_globals applies the
// non-Tcl EFHNIsGlob rule in one read.
TEST_CASE("bang_globals makes every leaf ending in ! global in one read",
          "[cap:ext-flattening-hierarchy][req:Global name merging]") {
  GoldenCase g{"NL-FLAT-12"};
  SECTION("without it (the Tcl rule, globalNets empty) x1/vdd! and x2/vdd! stay apart") {
    const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
    REQUIRE(n.ok());
    CHECK(terminal_nodes(n.value().devices[0]) == Strings{"x1/o", "x1/vdd!", "x1/o"});
  }
  SECTION("with it they merge into vdd!, with the split-global message") {
    auto options = nltest_options();
    options.bang_globals = true;
    const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, options);
    REQUIRE(n.ok());
    CHECK(terminal_nodes(n.value().devices[0]) == Strings{"x1/o", "vdd!", "x1/o"});
    CHECK(terminal_nodes(n.value().devices[1]) == Strings{"x2/o", "vdd!", "x2/o"});
    CHECK_FALSE(g.texts("EXTFLAT-READ-008").empty());
  }
}
