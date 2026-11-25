// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The FastHenry reference plane and counters, and the geometry
// centerlines (capability extresist-output "FastHenry geometry file",
// "Geometry centerlines"; tasks.md 14.4, the parts that do not need the
// network's widths and heights). Owner: RESCMD.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/extresist/output/output.hpp"

namespace db = hefesto::db;
namespace extresist = hefesto::extresist;

TEST_CASE("the FastHenry reference plane segments", "[cap:extresist-output][req:FastHenry geometry file]") {
  const std::string text = extresist::fasthenry_reference("top", db::Rect{0, 0, 40, 200}, 0.1F);
  CHECK(text ==
        "* FastHenry output for magic cell top\n\n"
        ".Units um\n"
        ".Default rho=0.02 nhinc=3 nwinc=3 rh=2 rw=2\n\n"
        "* Reference plane (substrate, ground)\n"
        "Gsub x1=0.00 y1=0.00 z1=0 x2=4.00 y2=0.00 z2=0\n"
        "+ x3=4.00 y3=20.00 z3=0\n"
        "+ thick=0.1 seg1=4 seg2=10\n"
        "+ Ngp (0.00,0.00,0)\n"
        "\nNsub x=0.00 y=0.00 z=0\n"
        ".Equiv Nsub Ngp\n");
}

// Q-NETLIST-40 (compat): ON, as Magic, the ids restart for each net.
TEST_CASE("FastHenry ids restart per net with magic-compat ON", "[cap:extresist-output][req:FastHenry geometry file]") {
  extresist::FastHenryIds ids;
  ids.next_net(true);
  ids.node += 2;  // N1, N2
  ids.segment += 1;  // E1
  ids.next_net(true);
  CHECK(ids.node == 1);
  CHECK(ids.segment == 1);
}

TEST_CASE("FastHenry ids run across the file with magic-compat OFF",
          "[cap:extresist-output][req:FastHenry geometry file]") {
  extresist::FastHenryIds ids;
  ids.next_net(false);
  ids.node += 2;
  ids.segment += 1;
  ids.next_net(false);
  CHECK(ids.node == 3);  // the second net's nodes are N3, N4
  CHECK(ids.segment == 2);
}

TEST_CASE("a centerline element between unnamed nodes", "[cap:extresist-output][req:Geometry centerlines]") {
  extresist::Network net;
  net.nodes = {extresist::NetNode{db::Point{100, 2}, "", 0}, extresist::NetNode{db::Point{0, 2}, "", 0}};
  net.resistors = {extresist::NetResistor{0, 1, 10.0}};
  std::int32_t next = 1;
  const auto elements = extresist::geometry_elements(net, next);
  REQUIRE(elements.size() == 1);
  CHECK(elements[0].name == "N1_2");
  CHECK(elements[0].from == db::Point{0, 2});
  CHECK(elements[0].to == db::Point{100, 2});
  CHECK(next == 3);
  // A named end keeps its name.
  net.nodes[1].name = "out";
  CHECK(extresist::geometry_elements(net, next)[0].name == "N3_out");
}
