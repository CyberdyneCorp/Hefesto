// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ext-flattening-hierarchy: EFFlatBuild with EF_FLATNODES | EF_NOFLATSUBCKT
// and EFVisitNodes, through read_flat on hand-written `.ext` files.
// Owner: EXTSUB.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ext_fixture.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace extflat = hefesto::extflat;
using extflat::test::all_names;
using extflat::test::ExtFixture;
using extflat::test::names;
using extflat::test::node_named;
using Strings = std::vector<std::string>;

TEST_CASE("uses are flattened in Magic's use-table order", "[cap:ext-flattening-hierarchy][req:Container iteration order]") {
  ExtFixture f;
  f.file("top", "use c u3 1 0 0 0 1 0\nuse c u1 1 0 0 0 1 0\nuse c u2 1 0 0 0 1 0\n");
  f.file("c", "node n 0 0 0 0 m1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  hefesto::util::MagicHashTable<int> table{32};
  for (const char* id : {"u3", "u1", "u2"}) {
    table.find_or_insert(id).value = 0;
  }
  // Each use's nodes are prepended to the flat list: the last use comes first.
  Strings expected;
  for (const auto& id : table.keys()) {
    expected.insert(expected.begin(), id + "/n");
  }
  CHECK(names(n.value()) == expected);
}

TEST_CASE("array elements: x outer, y inner, translated in the child's frame",
          "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  ExtFixture f;
  f.file("top", "use c a[0:1:10][1:0:5] 0 -1 100 1 0 0\n");
  f.file("c", "node n 0 0 0 0 m1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  // Creation order x=0 (y=0, 1), x=1 (y=0, 1); the reversed y range negates
  // its separation; the element offset goes through the use's rotation.
  CHECK(names(n.value()) == Strings{"a[1,1]/n", "a[0,1]/n", "a[1,0]/n", "a[0,0]/n"});
  // Element (x=1, y=0): offset (10*1, -5*(0-1)) = (10, 5) in the child,
  // then x' = -y + 100, y' = x: the unit rectangle (10,5)-(11,6) maps to
  // (94,10)-(95,11).
  CHECK(node_named(n.value(), "a[0,1]/n")->location == hefesto::db::Point{94, 10});
}

TEST_CASE("a connection with subscripts joins the matching elements", "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  ExtFixture f;
  f.file("top", "use c u[0:2:1][0:0:0] 1 0 0 0 1 0\nmerge u[0:1]/a u[1:2]/b\n");
  f.file("c", "node a 0 0 0 0 m1\nnode b 0 0 1 1 m1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  // u[0]/a+u[1]/b and u[1]/a+u[2]/b are joined; u[0]/b and u[2]/a stay alone.
  CHECK(n.value().nodes().size() == 4);
  CHECK(all_names(*node_named(n.value(), "u[1]/b")) == Strings{"u[1]/b", "u[0]/a"});
}

TEST_CASE("an implicit substrate is shared by every instance under its bare name",
          "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  ExtFixture f;
  f.file("top", "use c x1 1 0 0 0 1 0\nuse c x2 1 0 0 0 1 0\n");
  f.file("c", "node g 0 0 0 0 m1\nnode s 0 0 1 1 m1\nnode d 0 0 2 2 m1\n"
              "device mosfet nfet 0 0 1 1 2 8 w_sub g 4 0 s 8 0 d 8 0\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  // w_sub was first named by the device, so it is EF_DEVTERM: one flat node.
  const auto* sub = node_named(n.value(), "w_sub");
  REQUIRE(sub != nullptr);
  CHECK(all_names(*sub) == Strings{"w_sub"});
  CHECK(n.value().nodes().size() == 7);
}

TEST_CASE("merging gives the top-level port's name and place to the kept node",
          "[cap:ext-flattening-hierarchy][req:Node merge semantics]") {
  ExtFixture f;
  f.file("top",
         "port OUT 0 0 0 1 1 m1\n"
         "node a 0 0 5 5 m2\nnode b 0 0 6 6 m2\nequiv a b\n"  // b wins in the cell: b, a
         "merge a OUT\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  REQUIRE(n.value().nodes().size() == 1);
  const auto& node = n.value().nodes()[0];
  CHECK(node.name == "OUT");
  CHECK(node.top_port);
  CHECK(node.location == hefesto::db::Point{0, 0});  // OUT's place: its name won
  CHECK(node.layer == "m1");
}

TEST_CASE("a killed node is not visited", "[cap:ext-flattening-hierarchy][req:Kills]") {
  ExtFixture f;
  f.file("top", "use c u 1 0 0 0 1 0\nnode k 0 0 0 0 m1\n");
  f.file("c", "node n1 0 0 0 0 m1\nnode n2 0 0 1 1 m1\nkillnode n1\nkillnode gone\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(names(n.value()) == Strings{"k", "u/n2"});
  CHECK(f.texts("EXTFLAT-READ-007") == Strings{"kill: no such node u/gone"});
}

TEST_CASE("disconnected global nodes are merged with a warning", "[cap:ext-flattening-hierarchy][req:Global name merging]") {
  ExtFixture f;
  f.file("top", "use c u1 1 0 0 0 1 0\nuse c u2 1 0 0 0 1 0\n");
  f.file("c", "node vdd! 0 0 0 0 m1\nnode vss! 0 0 1 1 m1\n");
  extflat::ReadOptions options;
  options.global_nets = {"vdd!"};
  const auto n = f.read("top", options);
  REQUIRE(n.ok());
  const auto* vdd = node_named(n.value(), "vdd!");
  REQUIRE(vdd != nullptr);
  CHECK(all_names(*vdd).size() == 3);
  CHECK(node_named(n.value(), "u1/vss!") != nullptr);  // not in globalNets: stays split
  CHECK(node_named(n.value(), "u2/vss!") != nullptr);
  const auto lines = f.texts("EXTFLAT-READ-008");
  REQUIRE_FALSE(lines.empty());
  CHECK(lines.front() == "*** Global name vdd! not fully connected:");
}

TEST_CASE("a substrate node under a global name merges without a warning", "[cap:ext-flattening-hierarchy][req:Global name merging]") {
  ExtFixture f;
  f.file("top", "use c u1 1 0 0 0 1 0\nuse c u2 1 0 0 0 1 0\n");
  f.file("c", "node g 0 0 0 0 m1\nnode s 0 0 1 1 m1\nnode d 0 0 2 2 m1\n"
              "device mosfet nfet 0 0 1 1 2 8 VSS! g 4 0 s 8 0 d 8 0\n");
  extflat::ReadOptions options;
  options.global_nets = {"VSS!"};
  const auto n = f.read("top", options);
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-008").empty());
  CHECK(node_named(n.value(), "VSS!") != nullptr);
}

// A cell's node list is in reverse creation order, and efAddNodes prepends
// each node to the flat list again, so one cell's nodes come out in their
// creation order (a node merged away leaves its place).
TEST_CASE("one cell's nodes are visited in creation order", "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  ExtFixture f;
  f.file("top", "node a 0 0 0 0 m1\nnode b 0 0 0 0 m1\nnode c 0 0 0 0 m1\nequiv c a\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(names(n.value()) == Strings{"b", "c"});
}
