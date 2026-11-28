// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The FastHenry reference plane, counters, node alignment, nodes, segments
// and file end, and the geometry centerlines (capability extresist-output
// "FastHenry geometry file", "Geometry centerlines"; tasks.md 14.4).
// Owner: RESCMD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

#include <vector>

#include "hefesto/extract/tech/style.hpp"
#include "hefesto/extresist/output/output.hpp"

namespace db = hefesto::db;
namespace extresist = hefesto::extresist;
namespace tech = hefesto::tech;

namespace {

// NetNode and NetResistor gained fields in M7 R4 (RESNET); these build the
// ones the output tests set.
[[maybe_unused]] extresist::NetNode net_node(db::Point at, std::string name, double cap) {
  extresist::NetNode n;
  n.location = at;
  n.name = std::move(name);
  n.cap = cap;
  return n;
}

[[maybe_unused]] extresist::NetResistor net_resistor(std::size_t a, std::size_t b, double ohms) {
  extresist::NetResistor r;
  r.a = a;
  r.b = b;
  r.resistance = ohms;
  return r;
}

}  // namespace

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
  net.nodes = {net_node(db::Point{100, 2}, "", 0), net_node(db::Point{0, 2}, "", 0)};
  net.resistors = {net_resistor(0, 1, 10.0)};
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

namespace {

// An east-west resistor of type `t` between "a" at (0, 0) and an unnamed
// node at (100, 2), centerline y = 2, width 4.
extresist::Network ew_net(tech::TileType t) {
  extresist::Network net;
  net.nodes = {net_node(db::Point{0, 0}, "a", 0), net_node(db::Point{100, 2}, "", 0)};
  net.nodes[0].named = true;
  net.nodes[0].resistors = {0};
  net.nodes[1].resistors = {0};
  extresist::NetResistor r = net_resistor(0, 1, 10.0);
  r.status = extresist::kStatusEW;
  r.centerline = 2;
  r.width = 4;
  r.type = t;
  net.resistors = {r};
  return net;
}

}  // namespace

// Standing rule (Q-NETLIST-74, compat, approved 2026-11-27): ResAlignNodes' loop
// stops before the last resistor of the list.
TEST_CASE("node alignment skips the last resistor with magic-compat ON",
          "[cap:extresist-output][req:FastHenry geometry file]") {
  extresist::Network net = ew_net(5);
  std::vector<std::string> messages;
  extresist::align_nodes(net, true, messages);
  CHECK(net.nodes[0].location == db::Point{0, 0});
}

TEST_CASE("node alignment moves the ends of every resistor with magic-compat OFF",
          "[cap:extresist-output][req:FastHenry geometry file]") {
  extresist::Network net = ew_net(5);
  std::vector<std::string> messages;
  extresist::align_nodes(net, false, messages);
  CHECK(net.nodes[0].location == db::Point{0, 2});
  CHECK(net.nodes[1].location == db::Point{100, 2});
  CHECK(messages.empty());
}

TEST_CASE("a net's FastHenry nodes and segments", "[cap:extresist-output][req:FastHenry geometry file]") {
  hefesto::extract::ExtStyle style;
  style.heights[5].height = 1.5F;
  style.heights[5].thickness = 0.2F;
  const extresist::GeometryContext ctx{nullptr, &style, nullptr, true};
  const extresist::Network net = ew_net(5);
  extresist::FastHenryIds ids;
  std::vector<std::int32_t> node_ids;
  CHECK(extresist::fasthenry_nodes(net, ctx, ids, node_ids) ==
        "\n* List of nodes in network\nNa x=0.00 y=0.00 z=1.50\nN1 x=100.00 y=2.00 z=1.50\n");
  CHECK(node_ids == std::vector<std::int32_t>{-1, 1});
  CHECK(extresist::fasthenry_segments(net, ctx, ids, node_ids) ==
        "* Segments connecting nodes in network\nE1 Na N1 w=4.00 h=0.20\n");
  CHECK(ids.node == 2);
  CHECK(ids.segment == 2);
}

// Q-NETLIST-21 (fix): the frequency defaults to 10 MHz (the run passes it);
// the port order comment lines and the end.
TEST_CASE("the FastHenry file's end", "[cap:extresist-output][req:FastHenry geometry file]") {
  CHECK(extresist::fasthenry_trailer(1e9, {{1, "in"}, {2, "out"}}) ==
        "\n.freq fmin=1e+09 fmax=1e+09\n\n* Order of arguments to SPICE subcircuit call:\n* 1 in\n* 2 out\n\n.end\n");
  CHECK(extresist::fasthenry_trailer(10e6, {}).starts_with("\n.freq fmin=1e+07 fmax=1e+07\n"));
}
