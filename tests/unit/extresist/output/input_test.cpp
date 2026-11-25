// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extresist input model (extresist/output/output.hpp read_input();
// capability extresist-command "Input model"; tasks.md 14.2): node
// scaling, devices, ports, couplings, the parents' connections and
// Q-NETLIST-29 per mode with its guards. Owner: RESCMD.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include "../command/resist_fixture.hpp"

using namespace resist_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

TEST_CASE("node resistance and capacitance scaling", "[cap:extresist-command][req:Input model]") {
  Session s(true, true, tech_text(1000));
  const auto m = read_model(s, "node \"n\" 25 3000 0 0 metal1\nnode \"inf\" -1 0 0 0 metal1\n");
  REQUIRE(m.ok());
  const extresist::InputNode& n = m.value().nodes[0];
  CHECK(n.resistance == 25000.0);
  CHECK(n.cap == 3000.0);
  CHECK(n.status.node_record);
  CHECK(n.location == db::Point{0, 0});
  CHECK(n.tile_type == s.type("metal1"));
  // A negative value is infinite (Magic converts INFINITY to an int).
  CHECK(std::isinf(m.value().nodes[1].resistance));
}

TEST_CASE("an attribute applies to the last node record with magic-compat ON", "[cap:extresist-command][req:Input model]") {
  Session s;
  const auto m = read_model(s, "node \"a\" 0 0 0 0 poly\nnode \"b\" 0 0 5 0 poly\nattr a 0 0 0 0 poly \"res:skip\"\n", true);
  REQUIRE(m.ok());
  CHECK_FALSE(m.value().nodes[0].status.skip);
  CHECK(m.value().nodes[1].status.skip);
}

TEST_CASE("an attribute applies to the named node with magic-compat OFF", "[cap:extresist-command][req:Input model]") {
  Session s(false);
  std::vector<std::string> messages;
  const auto m = read_model(s,
                            "node \"a\" 0 0 0 0 poly\nnode \"b\" 0 0 5 0 poly\nattr a 0 0 0 0 poly \"res:skip\"\n"
                            "attr nosuch 0 0 0 0 poly \"res:force\"\n",
                            false, {}, &messages);
  REQUIRE(m.ok());
  CHECK(m.value().nodes[0].status.skip);
  CHECK_FALSE(m.value().nodes[1].status.skip);
  CHECK(has(messages, "Warning: attribute for unknown node nosuch ignored"));
}

// Regression (Q-NETLIST-29 guards, both modes): Magic reads an
// uninitialised node and overflows a 20-byte buffer here.
TEST_CASE("an attribute before any node record and a long res:min", "[cap:extresist-command][req:Input model]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    std::vector<std::string> messages;
    const std::string digits(64, '1');
    const auto m = read_model(s,
                              "attr a 0 0 0 0 poly \"res:skip\"\nnode \"a\" 0 0 0 0 poly\n"
                              "attr a 0 0 0 0 poly \"res:min=" + digits + "\"\n",
                              compat, {}, &messages);
    REQUIRE(m.ok());
    CHECK_FALSE(m.value().nodes[0].status.skip);
    CHECK(m.value().nodes[0].status.minsize);
    CHECK(m.value().nodes[0].min_resistance > 1e60);
    CHECK(messages.size() == 1U);
  }
}

TEST_CASE("res:force, res:skip conflicts and res:drive", "[cap:extresist-command][req:Input model]") {
  Session s;
  std::vector<std::string> messages;
  const auto m = read_model(s,
                            "node \"a\" 0 0 0 0 poly\nattr a 0 0 0 0 poly \"res:force\"\n"
                            "attr a 0 0 0 0 poly \"res:skip\"\nattr a 3 4 0 0 poly \"res:drive\"\n",
                            true, {}, &messages);
  REQUIRE(m.ok());
  const extresist::InputNode& a = m.value().nodes[0];
  CHECK(a.status.force);
  CHECK_FALSE(a.status.skip);
  CHECK(has(messages, "Warning: Node a is both forced and skipped"));
  CHECK(a.status.drive_located);
  REQUIRE(a.drive_points.size() == 1);
  CHECK(a.drive_points[0].rect == db::Rect{3, 4, 3, 4});
}

TEST_CASE("device records: terminals and W/L", "[cap:extresist-command][req:Input model]") {
  Session s;
  const auto m = read_model(s,
                            "node \"g\" 0 0 0 0 poly\nnode \"s\" 0 0 0 0 ndiff\nnode \"d\" 0 0 0 0 ndiff\n"
                            "device mosfet nfet 10 20 11 21 2 8 \"GND\" \"g\" 4 0 \"s\" 6 0 \"d\" 8 x=1\n");
  REQUIRE(m.ok());
  REQUIRE(m.value().devices.size() == 1);
  const extresist::InputDevice& d = m.value().devices[0];
  CHECK(d.model == "nfet");
  CHECK(d.location == db::Point{10, 20});
  CHECK(d.length == 4);
  CHECK(d.width == 8);
  CHECK(d.wl == 2.0F);
  CHECK(d.drain_attr == "x=1");
  CHECK(d.leading == std::vector<std::string>{"2", "8"});
  CHECK(d.tile_type == s.type("nfet"));
  CHECK(d.style_class == extract::DeviceClass::mosfet);
  REQUIRE(d.gate.has_value());
  CHECK(m.value().nodes[*d.gate].name == "g");
  // GND has no node record: the terminal created it (no NODERECORD).
  const auto gnd = m.value().find("GND");
  REQUIRE(gnd.has_value());
  CHECK_FALSE(m.value().nodes[*gnd].status.node_record);
  CHECK(m.value().nodes[*m.value().find("s")].devices.front().terminal == extresist::Terminal::source);
}

TEST_CASE("ports, couplings and connections", "[cap:extresist-command][req:Input model]") {
  Session s;
  const auto m = read_model(s,
                            "node \"a\" 0 100 0 0 poly\nnode \"b\" 0 200 0 0 metal1\n"
                            "port \"a\" 0 1 2 3 4 poly\ncap \"a\" \"b\" 50\n"
                            "connect 5 6 7 8 metal1 \"b\" \"u1/x\"\n");
  REQUIRE(m.ok());
  const extresist::InputNode& a = m.value().nodes[0];
  CHECK(a.status.port);
  CHECK(a.status.force);
  CHECK(a.status.drive_located);
  CHECK(a.drive_points.front().rect == db::Rect{1, 2, 3, 4});
  CHECK(a.cap == 150.0);  // signal mode: the coupling adds to both nets
  const extresist::InputNode& b = m.value().nodes[1];
  CHECK(b.cap == 250.0);
  REQUIRE(b.sink_points.size() == 1);
  CHECK(b.sink_points.front().rect == db::Rect{5, 6, 7, 8});
}

TEST_CASE("connections from a parent become drive points", "[cap:extresist-command][req:Input model]") {
  Session s;
  const std::string parent =
      "use top u1 1 0 100 0 1 200\n"
      "connect 105 205 107 207 poly \"pnet\" \"u1/a\"\n"
      "connect 0 0 1 1 poly \"pnet\" \"other/a\"\n";
  const auto m = read_model(s, "node \"a\" 0 0 0 0 poly\n", true, {{"chip", parent}});
  REQUIRE(m.ok());
  const extresist::InputNode& a = m.value().nodes[0];
  REQUIRE(a.drive_points.size() == 1);
  CHECK(a.drive_points.front().rect == db::Rect{5, 5, 7, 7});
  CHECK(a.status.force);
  CHECK(a.status.drive_located);
}

TEST_CASE("an unreadable .ext file fails the read", "[cap:extresist-command][req:Input model]") {
  extresist::InputOptions options;
  const auto m = extresist::read_input("nosuch", options);
  REQUIRE_FALSE(m.ok());
  CHECK(m.error().code == "EXTRESIST-OUT-001");
  CHECK(m.error().text == "Cannot open file nosuch.ext");
}
