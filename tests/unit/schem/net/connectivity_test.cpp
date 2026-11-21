// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Connectivity and net naming (capability schematic-netlisting "Connection
// geometry", "Net naming order", "Unnamed nets", "Pass-through and shorted
// symbols", "Bus taps", "Global nets"; Q-SCHEM-16, Q-SCHEM-29; golden cases
// SCHEM-NL-11, NL-19 and NL-22; tasks.md 8.1). Schematics are built in
// memory with the pin positions of the golden inputs' symbols: `res.sym`
// pins P (0,-30) and M (0,30), `lab_pin.sym` and `gnd.sym` one pin at the
// origin, the golden `tap.sym` pins `tap` (0,-20) and `bus` (0,0).
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/connectivity.hpp"

namespace schem = hefesto::schem;
using schem::Point;
using Names = std::vector<std::string>;

namespace {

schem::NetInstance resistor(std::string name, Point at) {
  schem::NetInstance r;
  r.name = std::move(name);
  r.type = "resistor";
  r.pins = {{"P", {at.x, at.y - 30}, 1}, {"M", {at.x, at.y + 30}, 1}};
  return r;
}

schem::NetInstance label(std::string name, Point at, std::optional<std::string> lab, std::string type = "label") {
  schem::NetInstance l;
  l.name = std::move(name);
  l.type = std::move(type);
  l.lab = std::move(lab);
  l.pins = {{"p", at, 1}};
  return l;
}

schem::NetInstance tap(std::string name, Point at, std::string lab) {
  schem::NetInstance t;
  t.name = std::move(name);
  t.type = "bus_tap";
  t.lab = std::move(lab);
  t.pins = {{"tap", {at.x, at.y - 20}, 1}, {"bus", at, 1}};
  return t;
}

schem::NetWire wire(double x1, double y1, double x2, double y2) {
  return {{x1, y1}, {x2, y2}, false};
}

/// The inputs of tests/golden/SCHEM/cases/SCHEM-NL-11/input/nl11.sch.
schem::SchematicNets nl11() {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0),   wire(200, 0, 300, 0),   wire(250, 0, 250, 50),   wire(400, 0, 500, 0),
             wire(450, -50, 450, 50), wire(700, 0, 800, 0), wire(700, 100, 800, 100), wire(700, 200, 800, 200)};
  s.instances = {label("l1", {0, 0}, "A"),      label("l2", {100, 0}, "B"),   resistor("R1", {50, 30}),
                 label("l3", {200, 0}, "T"),    resistor("R2", {250, 80}),    resistor("R3", {400, 30}),
                 resistor("R4", {450, 80}),     resistor("R5", {600, 0}),     label("l4", {700, 0}, "net1"),
                 resistor("R6", {800, 130}),    resistor("R7", {700, 130}),   label("l5", {0, 200}, "0")};
  return s;
}

std::string text(const schem::NetNaming& n, std::size_t inst, std::size_t pin) {
  return schem::net_text(n.pin_nets[inst][pin], n, "");
}

}  // namespace

TEST_CASE("T junction connects", "[cap:schematic-netlisting][req:Connection geometry]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0), wire(50, 0, 50, 50)};
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets[0] == n.wire_nets[1]);
}

TEST_CASE("Crossing does not connect", "[cap:schematic-netlisting][req:Connection geometry]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0), wire(50, -50, 50, 50)};
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets[0] != n.wire_nets[1]);
}

TEST_CASE("Pin on wire interior", "[cap:schematic-netlisting][req:Connection geometry]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0)};
  s.instances = {resistor("R1", {40, 30})};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[0][0] == n.wire_nets[0]);
}

TEST_CASE("connection geometry: exact touching rule", "[cap:schematic-netlisting][req:Connection geometry]") {
  CHECK(schem::touches({50, 0}, {100, 0}, {0, 0}));  // endpoints in either order
  CHECK(schem::touches({1, 1}, {0, 0}, {2, 2}));     // diagonal
  CHECK_FALSE(schem::touches({1, 1.0000000001}, {0, 0}, {2, 2}));
  CHECK_FALSE(schem::touches({3, 3}, {0, 0}, {2, 2}));
  CHECK(schem::touches({5, 5}, {5, 5}, {5, 5}));  // zero length touches its own point only
  CHECK_FALSE(schem::touches({5, 6}, {5, 5}, {5, 5}));
  CHECK_FALSE(schem::touches({std::nan(""), 0}, {0, 0}, {10, 0}));

  schem::SchematicNets s;
  s.wires = {wire(0, 0, 0, 100), wire(0, 50, 30, 80), wire(10, 10, 20, 20)};  // diagonal from a vertical interior
  s.instances = {label("l", {15, 15}, "D"), label("m", {30, 80}, std::nullopt)};
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets[0] == n.wire_nets[1]);
  CHECK(n.wire_nets[2] == "D");
  CHECK(n.pin_nets[1][0] == n.wire_nets[1]);
}

TEST_CASE("connection geometry: two pins at one point connect", "[cap:schematic-netlisting][req:Connection geometry]") {
  schem::SchematicNets s;
  s.instances = {resistor("R1", {0, 0}), resistor("R2", {0, 60}), label("l", {0, -30}, "X")};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[0][1] == n.pin_nets[1][0]);  // R1 M and R2 P at (0,30)
  CHECK(n.pin_nets[0][0] == "X");
}

TEST_CASE("connection geometry: ignored wires and instances carry no connectivity",
          "[cap:schematic-netlisting][req:Connection geometry]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0), wire(100, 0, 200, 0)};
  s.wires[1].ignored = true;
  s.instances = {label("a", {0, 0}, "A"), label("b", {200, 0}, "B"), label("c", {0, 0}, "C")};
  s.instances[2].ignored = true;
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets == Names{"A", ""});
  CHECK(n.pin_nets[1][0] == "B");
  CHECK(n.pin_nets[2].empty());
  CHECK_FALSE(n.failed());
}

TEST_CASE("First label wins", "[cap:schematic-netlisting][req:Net naming order]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0)};
  s.instances = {label("l1", {0, 0}, "A"), label("l2", {100, 0}, "B"), resistor("R1", {50, 30})};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[2][0] == "A");
  CHECK(n.wire_nets[0] == "A");
  REQUIRE(n.shorts.size() == 1);
  CHECK(n.shorts[0].existing == "A");
  CHECK(n.shorts[0].incoming == "B");
}

TEST_CASE("Top-level short always reported", "[cap:schematic-netlisting][req:Net naming order]") {
  // Q-SCHEM-16: the short is found by the connectivity of the schematic
  // itself, so every run that computes it (with or without the hierarchy,
  // first pass or not) reports it and fails; the names are unaffected.
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0)};
  s.instances = {label("l1", {0, 0}, "A"), label("l2", {100, 0}, "B")};
  for (int run = 0; run < 2; ++run) {
    const auto n = schem::name_nets(s);
    CHECK(n.failed());
    REQUIRE(n.diagnostics.size() == 1);
    CHECK(n.diagnostics[0].code == schem::net_codes::kShort);
    CHECK(n.diagnostics[0].severity == hefesto::diag::Severity::error);
    CHECK(n.diagnostics[0].text == "Net shorted: A - B");
    CHECK(n.wire_nets[0] == "A");
  }
}

TEST_CASE("net naming: every short is reported", "[cap:schematic-netlisting][req:Net naming order]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0), wire(0, 50, 100, 50)};
  s.instances = {label("a", {0, 0}, "A"),  label("b", {100, 0}, "B"), label("c", {50, 0}, "C"),
                 label("d", {0, 50}, "D"), label("e", {100, 50}, "D")};
  const auto n = schem::name_nets(s);
  REQUIRE(n.shorts.size() == 2);
  CHECK(n.shorts[0].incoming == "B");
  CHECK(n.shorts[1].incoming == "C");
  CHECK(n.wire_nets == Names{"A", "D"});  // equal names are no short
}

TEST_CASE("net naming: ports name nets like labels", "[cap:schematic-netlisting][req:Net naming order]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 100, 0)};
  s.instances = {label("p1", {0, 0}, "IN", "ipin"), label("p2", {100, 0}, std::nullopt, "opin"),
                 label("x", {500, 0}, "IGNORED", "show_label")};
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets[0] == "IN");
  CHECK(n.pin_nets[1][0] == "IN");
  CHECK(n.pin_nets[2][0] == "#net1");  // show_label does not name
}

TEST_CASE("SCHEM-NL-11 net names, shorts and stored lab values",
          "[cap:schematic-netlisting][req:Net naming order]") {
  const auto n = schem::name_nets(nl11());
  // The pins of tests/golden/SCHEM/cases/SCHEM-NL-11/expected/nl11.spice.
  const std::vector<std::pair<std::size_t, Names>> expected = {
      {2, {"A", "net6"}},     {4, {"T", "net7"}},      {5, {"net2", "net8"}},  {6, {"net3", "net9"}},
      {7, {"net10", "net11"}}, {9, {"net4", "net12"}}, {10, {"net4", "net13"}}};
  for (const auto& [inst, nets] : expected) {
    INFO(inst);
    CHECK(text(n, inst, 0) == nets[0]);
    CHECK(text(n, inst, 1) == nets[1]);
  }
  // Q-SCHEM-16: the A-B short is reported (stderr `Error: Net shorted: A - B`).
  REQUIRE(n.shorts.size() == 1);
  CHECK(n.diagnostics[0].text == "Net shorted: A - B");
  // Q-SCHEM-29 (SCHEM-NL-22 expected/nl11-saved.sch): each wire stores its
  // net, unnamed nets with their `#`.
  CHECK(n.wire_nets == Names{"A", "T", "T", "#net2", "#net3", "net1", "#net4", "#net5"});
  CHECK(n.label_labs.empty());  // every label has a lab
}

TEST_CASE("Two floating resistors", "[cap:schematic-netlisting][req:Unnamed nets]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 0, 50)};
  s.instances = {resistor("R1", {0, -30}), resistor("R2", {0, 80})};
  const auto n = schem::name_nets(s);
  CHECK(text(n, 0, 1) == "net1");
  CHECK(text(n, 1, 0) == "net1");
  CHECK(text(n, 0, 0) == "net2");
  CHECK(text(n, 1, 1) == "net3");
}

TEST_CASE("User net named net1", "[cap:schematic-netlisting][req:Unnamed nets]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 10, 0), wire(0, 100, 10, 100), wire(0, 200, 10, 200)};
  s.instances = {label("l", {0, 0}, "net1")};
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets == Names{"net1", "#net2", "#net3"});
}

TEST_CASE("unnamed nets: multiplicity and stored label values", "[cap:schematic-netlisting][req:Unnamed nets]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 0, 100)};
  auto r = resistor("R1[3:0]", {0, -30});
  r.vector_width = 4;
  auto b = resistor("R2", {0, 130});
  b.pins[0].width = 2;
  s.instances = {r, b, label("lab", {500, 500}, std::nullopt)};
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets[0] == "#net1");
  CHECK(n.multiplicity.at("#net1") == 2);  // min(1*4, 2*1)
  CHECK(text(n, 0, 1) == "net1_1,net1_0");
  CHECK(text(n, 0, 0) == "net2_3,net2_2,net2_1,net2_0");
  CHECK(schem::net_text(n.pin_nets[0][0], n, "<>") == "net2_3,net2_2,net2_1,net2_0");
  CHECK(text(n, 1, 1) == "net3");
  REQUIRE(n.label_labs.size() == 1);
  CHECK(n.label_labs[0] == std::pair<std::size_t, std::string>{2, "net4"});  // without `#`
}

TEST_CASE("Shorted resistor joins nets", "[cap:schematic-netlisting][req:Pass-through and shorted symbols]") {
  schem::SchematicNets s;
  auto r = resistor("R1", {0, 0});
  r.shorted = true;
  s.instances = {label("a", {0, -30}, "A"), r, label("x", {0, 30}, std::nullopt)};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[1] == Names{"A", "A"});
  CHECK(n.pin_nets[2][0] == "A");
}

TEST_CASE("Shorted ammeter", "[cap:schematic-netlisting][req:Ignored and shorted objects]") {
  schem::SchematicNets s;
  auto meter = resistor("Vmeas", {0, 0});
  meter.type = "ammeter";
  meter.shorted = true;
  s.instances = {resistor("R1", {0, -60}), meter, resistor("R2", {0, 60})};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[0][1] == n.pin_nets[2][0]);  // R1 M (on A) and R2 P (on B) are one net
}

TEST_CASE("Pass-through pin", "[cap:schematic-netlisting][req:Pass-through and shorted symbols]") {
  schem::SchematicNets s;
  schem::NetInstance through;
  through.name = "F1";
  through.type = "feedthrough";
  through.pins = {{"A", {-20, 0}, 1}, {"A", {20, 0}, 1}};
  s.wires = {wire(-50, 0, -20, 0)};
  s.instances = {label("n", {-50, 0}, "N1"), through, label("x", {20, 0}, std::nullopt)};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[1] == Names{"N1", "N1"});
  CHECK(n.pin_nets[2][0] == "N1");
}

TEST_CASE("Bit tap", "[cap:schematic-netlisting][req:Bus taps]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 200, 0)};
  s.instances = {label("b", {0, 0}, "DATA[15:0]"), tap("t", {100, 0}, "[3]")};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[1][0] == "DATA[3]");
  CHECK(n.pin_nets[1][1] == "DATA[15:0]");
}

TEST_CASE("bus taps: lists, digits, plain names and existing names", "[cap:schematic-netlisting][req:Bus taps]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 400, 0)};
  s.instances = {label("b", {0, 0}, "CLK, DATA[7:0]"), tap("t1", {100, 0}, "5"), tap("t2", {200, 0}, "EN"),
                 tap("t3", {300, 0}, "[1]"), label("x", {300, -20}, "MINE")};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[1][0] == "DATA5");
  CHECK(n.pin_nets[2][0] == "EN");
  CHECK(n.pin_nets[3][0] == "DATA[1]");  // the tap names it before the later label
  REQUIRE(n.shorts.size() == 1);
  CHECK(n.shorts[0].existing == "DATA[1]");
  CHECK(n.shorts[0].incoming == "MINE");
}

TEST_CASE("bus taps: a named tap side is kept and the conflict reported", "[cap:schematic-netlisting][req:Bus taps]") {
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 200, 0)};
  s.instances = {label("x", {100, -20}, "MINE"), label("b", {0, 0}, "D[3:0]"), tap("t", {100, 0}, "[2]")};
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[2][0] == "MINE");
  REQUIRE(n.shorts.size() == 1);
  CHECK(n.shorts[0].tag == "Bus tap");
}

TEST_CASE("bus taps: chains of taps and tap loops end", "[cap:schematic-netlisting][req:Bus taps]") {
  schem::SchematicNets s;
  // t1's tap side is t2's bus side; t2's tap side is t1's bus side.
  s.instances = {tap("t1", {0, 0}, "1"), tap("t2", {0, -20}, "2")};
  s.instances[1].pins = {{"tap", {0, 0}, 1}, {"bus", {0, -20}, 1}};
  s.instances.push_back(label("b", {0, 0}, "B"));
  const auto n = schem::name_nets(s);
  CHECK(n.pin_nets[0][1] == "B");
  CHECK(n.pin_nets[0][0] == "B1");
  CHECK(n.failed());  // B1 + "2" would rename B: kept, reported
}

TEST_CASE("SCHEM-NL-19 bus tap nets", "[cap:schematic-netlisting][req:Bus taps]") {
  // The inputs of tests/golden/SCHEM/cases/SCHEM-NL-19/input/nl19.sch.
  schem::SchematicNets s;
  s.wires = {wire(0, 0, 500, 0), wire(0, 300, 500, 300)};
  auto r4 = resistor("R4[1:0]", {100, 250});
  r4.vector_width = 2;
  s.instances = {label("b1", {0, 0}, "DATA[3:0]"), label("b2", {0, 300}, "ADDR[7:0]"),
                 tap("t1", {100, 0}, "[3]"),       resistor("R1", {100, -50}),
                 tap("t2", {200, 0}, "2"),         resistor("R2", {200, -50}),
                 tap("t3", {300, 0}, "SPARE"),     resistor("R3", {300, -50}),
                 tap("t4", {100, 300}, "[5:4]"),   r4,
                 tap("t5", {200, 300}, "[0]"),     resistor("R5", {200, 250}),
                 label("l1", {0, 500}, "0")};
  const auto n = schem::name_nets(s);
  // expected/nl19.spice: R1 net1 DATA[3], R2 net2 DATA2, R3 net3 SPARE,
  // R4[1] net4_1 ADDR[5], R4[0] net4_0 ADDR[4], R5 net5 ADDR[0].
  CHECK(text(n, 3, 0) == "net1");
  CHECK(text(n, 3, 1) == "DATA[3]");
  CHECK(text(n, 5, 0) == "net2");
  CHECK(text(n, 5, 1) == "DATA2");
  CHECK(text(n, 7, 0) == "net3");
  CHECK(text(n, 7, 1) == "SPARE");
  CHECK(text(n, 9, 0) == "net4_1,net4_0");
  CHECK(text(n, 9, 1) == "ADDR[5],ADDR[4]");
  CHECK(text(n, 11, 0) == "net5");
  CHECK(text(n, 11, 1) == "ADDR[0]");
  CHECK_FALSE(n.failed());
}

TEST_CASE("IHP ground", "[cap:schematic-netlisting][req:Global nets]") {
  schem::SchematicNets s;
  auto g1 = label("l1", {0, 0}, "GND");
  g1.global = true;
  auto g2 = label("l2", {100, 0}, "GND");
  g2.global = true;
  s.instances = {g1, g2};
  const auto n = schem::name_nets(s);
  CHECK(n.globals == Names{"GND"});
}

TEST_CASE("global nets: bits, ground 0 and first-seen order", "[cap:schematic-netlisting][req:Global nets]") {
  schem::SchematicNets s;
  auto bus = label("l1", {0, 0}, "VDD[1:0]");
  bus.global = true;
  auto zero = label("l2", {100, 0}, "0");
  zero.global = true;
  auto port = label("p", {200, 0}, "NOTGLOBAL", "ipin");
  port.global = true;  // only labels declare globals
  auto vss = label("l3", {300, 0}, "VSS");
  vss.global = true;
  s.instances = {bus, zero, port, vss};
  const auto n = schem::name_nets(s);
  CHECK(n.globals == Names{"VDD[1]", "VDD[0]", "VSS"});
}

// Regression (fuzz_schem_bus timeout, CI run 44871606049): a global label
// with a wide bus searched the global list once per bit, O(n^2); 92032 bits
// took over 10 s. First-seen order and de-duplication are unchanged.
TEST_CASE("global nets: a wide global bus is collected in linear time", "[cap:schematic-netlisting][req:Global nets]") {
  schem::SchematicNets s;
  auto wide = label("l1", {0, 0}, "G[199999:0]");
  wide.global = true;
  auto again = label("l2", {100, 0}, "G[5]");
  again.global = true;
  s.instances = {wide, again};
  const auto n = schem::name_nets(s);
  REQUIRE(n.globals.size() == 200000);
  CHECK(n.globals.front() == "G[199999]");
  CHECK(n.globals.back() == "G[0]");
}

TEST_CASE("connectivity: orientation transform of pin points", "[cap:schematic-netlisting][req:Connection geometry]") {
  CHECK(schem::transform_point({100, 200}, 1, 0, {-20, 0}) == Point{100, 180});
  CHECK(schem::transform_point({100, 200}, 0, 1, {-20, 0}) == Point{120, 200});
  CHECK(schem::transform_point({0, 0}, 2, 0, {3, 4}) == Point{-3, -4});
  CHECK(schem::transform_point({0, 0}, 3, 0, {3, 4}) == Point{4, -3});
  CHECK(schem::transform_point({0, 0}, 7, 1, {3, 4}) == Point{4, 3});  // rotation modulo 4
  schem::BoxRecord box;
  box.p1 = {-22.5, -2.5};
  box.p2 = {-17.5, 2.5};
  CHECK(schem::pin_point(box) == Point{-20, 0});
}

TEST_CASE("connectivity: many wires at one point and long chains stay bounded",
          "[cap:schematic-netlisting][req:Connection geometry]") {
  schem::SchematicNets s;
  for (int i = 0; i < 20000; ++i) {
    s.wires.push_back(wire(i, 0, i + 1, 0));  // one chain of 20000 segments
  }
  for (int i = 0; i < 1000; ++i) {
    s.wires.push_back(wire(0, 10, 0, 11 + i));  // overlapping wires from (0,10)
  }
  for (int i = 0; i < 200; ++i) {
    s.wires.push_back(wire(-100, -100 - i, -99, -99 + i));  // diagonal wires, tested against every probe
  }
  s.instances = {label("a", {0, 0}, "CHAIN"), label("s", {0, 10}, "STAR")};
  const auto n = schem::name_nets(s);
  CHECK(n.wire_nets[19999] == "CHAIN");
  CHECK(n.wire_nets[20999] == "STAR");
  CHECK(n.wire_nets.back() == "#net200");  // the diagonals only cross: 200 nets
}
