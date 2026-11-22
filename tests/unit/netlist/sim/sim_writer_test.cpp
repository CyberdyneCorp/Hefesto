// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tests of the `.sim` writer (netlist/sim/writer.hpp; capability
// netlist-sim; openspec/changes/m7-extraction/design.md D15). The visit data
// is built by hand: extflat's flattening fills it through from_netlist().

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/netlist/sim/writer.hpp"

namespace sim = hefesto::netlist::sim;
namespace extflat = hefesto::extflat;
namespace extract = hefesto::extract;
using hefesto::compat::CompatSettings;

namespace {

// A circuit whose nodes are named by single components.
struct Circuit {
  sim::SimInput input;

  Circuit() {
    input.scale = 1.0F;
    input.tech = "scmos";
  }

  std::size_t node(const std::string& name, double cap = 0.0, std::int64_t resist = 0) {
    sim::SimNode n;
    n.names.push_back(sim::NodeName{{name}, !name.empty() && name.back() == '!'});
    n.cap = cap;
    n.resist = resist;
    input.nodes.push_back(n);
    return input.nodes.size() - 1;
  }

  sim::SimDevice& device(sim::DeviceClass cls, const std::string& type, const std::vector<std::size_t>& terms) {
    sim::SimDevice d;
    d.device_class = cls;
    d.type = type;
    for (const std::size_t t : terms) {
      d.terminals.push_back(sim::SimTerminal{t, {}, {}, {}});
    }
    input.devices.push_back(d);
    return input.devices.back();
  }
};

sim::SimOutput write(const sim::SimInput& input, const sim::WriteOptions& options = {}, bool compat = true) {
  extflat::NameFlags flags = extflat::NameFlags{false, false, false, false, false};
  return sim::write_sim(input, options, flags, CompatSettings{compat});
}

// The lines after the header.
std::string body(const std::string& sim) {
  return sim.substr(sim.find('\n') + 1);
}

}  // namespace

TEST_CASE("sim header names the units, technology and format",
          "[cap:netlist-sim][req:File structure and order]") {
  Circuit c;
  c.input.scale = 5.0F;
  CHECK(write(c.input).sim == "| units: 5 tech: scmos format: MIT\n");
  sim::WriteOptions su;
  su.settings.format = sim::SimFormat::su;
  CHECK(write(c.input, su).sim == "| units: 5 tech: scmos format: SU\n");
}

TEST_CASE("sim records come in device, coupling, resistor, node order",
          "[cap:netlist-sim][req:File structure and order]") {
  Circuit c;
  const auto a = c.node("a", 5000);
  const auto b = c.node("b");
  c.device(sim::DeviceClass::mosfet, "nfet", {a, b, b}).length = 2;
  c.input.caps.push_back(sim::SimCap{{"a"}, {"b"}, 5000});
  c.input.resists.push_back(sim::SimResist{{"a"}, {"b"}, 20000});
  CHECK(body(write(c.input).sim) == "n a b b 2 0 0 0\nC a b 5.0\nr a b 20\nC a0 5.0\n");
  sim::WriteOptions no_caps;
  no_caps.flat_caps = false;  // EF_FLATCAPS off: no coupling lines
  CHECK(body(write(c.input, no_caps).sim) == "n a b b 2 0 0 0\nr a b 20\nC a0 5.0\n");
}

// Q-NETLIST-17: merging ignores class and type with magic-compat ON.
TEST_CASE("merge across device types with magic-compat ON", "[cap:netlist-sim][req:File structure and order]") {
  Circuit c;
  const auto g = c.node("g");
  const auto s = c.node("s");
  const auto d = c.node("d");
  for (const char* type : {"nfet", "nhv"}) {
    auto& dev = c.device(sim::DeviceClass::mosfet, type, {g, s, d});
    dev.length = 2;
    dev.width = 4;
  }
  sim::WriteOptions options;
  options.settings.merge = sim::SimMerge::aggressive;
  const auto out = write(c.input, options, true);
  CHECK(body(out.sim) == "n g s d 2 8 0 0\n");
  REQUIRE(out.console.size() == 1);
  CHECK(out.console.front().text == "Devices merged: 1");
}

TEST_CASE("merge across device types with magic-compat OFF", "[cap:netlist-sim][req:File structure and order]") {
  Circuit c;
  const auto g = c.node("g");
  const auto s = c.node("s");
  const auto d = c.node("d");
  for (const char* type : {"nfet", "nhv", "nfet"}) {
    auto& dev = c.device(sim::DeviceClass::mosfet, type, {g, d, s});  // the third swaps source and drain
    dev.length = 2;
    dev.width = 4;
  }
  c.input.devices[2].terminals = {sim::SimTerminal{g, {}, {}, {}}, sim::SimTerminal{s, {}, {}, {}},
                                  sim::SimTerminal{d, {}, {}, {}}};
  sim::WriteOptions options;
  options.settings.merge = sim::SimMerge::conservative;
  const auto out = write(c.input, options, false);
  CHECK(body(out.sim) == "n g d s 2 8 0 0\nn g d s 2 4 0 0\n");
  CHECK(out.console.front().text == "Devices merged: 1");
}

TEST_CASE("sim MOSFET line", "[cap:netlist-sim][req:Device lines]") {
  Circuit c;
  const auto in = c.node("in");
  const auto gnd = c.node("GND");
  const auto out = c.node("out");
  auto& d = c.device(sim::DeviceClass::mosfet, "nfet", {in, gnd, out});
  d.length = 2;
  d.width = 8;
  d.lower_left = {10, 20};
  CHECK(body(write(c.input).sim) == "n in GND out 2 8 10 20\n");
  d.scale = 0.5F;
  CHECK(body(write(c.input).sim) == "n in GND out 1 4 5 10\n");
}

TEST_CASE("sim resistor, capacitor and unresolved terminals", "[cap:netlist-sim][req:Device lines]") {
  Circuit c;
  const auto body_node = c.node("w");
  const auto a = c.node("a");
  const auto b = c.node("b");
  c.device(sim::DeviceClass::res, "None", {body_node, a, b}).resistance = 25.5;
  c.device(sim::DeviceClass::cap, "None", {a, b}).capacitance = 3.0;
  auto& open = c.device(sim::DeviceClass::mosfet, "pfet", {a, b, b});
  open.terminals[1].node.reset();
  c.device(sim::DeviceClass::mosfet, "nfet", {a});  // fewer than 2 terminals: skipped
  CHECK(body(write(c.input).sim) == "r a b 25.500000\nc a b 3.000000\np a GND b 0 0 0 0\n");
}

TEST_CASE("sim class letter of a FET whose type starts with neither n nor p", "[cap:netlist-sim][req:Device lines]") {
  Circuit c;
  const auto a = c.node("a");
  c.device(sim::DeviceClass::mosfet, "hvfet", {a, a, a});
  c.device(sim::DeviceClass::mosfet, "xfet", {a, a, a});
  sim::WriteOptions options;
  options.device_info = {extract::DeviceInfo{"hvfet", 9, -1, -1, -1, ""}};
  options.type_long_names = {"pdevice"};  // the tile type's long name gives the letter
  const auto out = write(c.input, options);
  CHECK(body(out.sim) == "p a a a 0 0 0 0\nn a a a 0 0 0 0\n");
  REQUIRE(out.console.size() == 1);
  CHECK(out.console.front().error);
  CHECK(out.console.front().text ==
        "Error: MOSFET device type \"xfet\" does not start with \"n\" or \"p\" as required for the .sim format");
}

TEST_CASE("sim subcircuit and diode devices end with their type", "[cap:netlist-sim][req:Device lines]") {
  Circuit c;
  const auto a = c.node("a");
  const auto b = c.node("b");
  auto& d = c.device(sim::DeviceClass::diode, "pdio", {a, b});
  d.terminals[1].attributes = "x";
  d.length = 3;
  d.width = 4;
  CHECK(body(write(c.input).sim) == "x a b s=x l=3 w=4 x=0 y=0 pdio\n");
}

TEST_CASE("sim SU attribute fields", "[cap:netlist-sim][req:Transistor attributes]") {
  Circuit c;
  const auto in = c.node("in");
  const auto src = c.node("s");
  const auto drn = c.node("d");
  const auto gnd = c.node("GND");
  c.input.nodes[src].area_perim = {sim::AreaPerim{40, 26}};
  c.input.nodes[drn].area_perim = {sim::AreaPerim{30, 16}};
  auto& d = c.device(sim::DeviceClass::mosfet, "nfet", {in, src, drn});
  d.substrate = sim::SimDevice::Substrate{"GND", gnd, "GND", {}};
  sim::WriteOptions options;
  options.settings.format = sim::SimFormat::su;
  options.device_info = {extract::DeviceInfo{"nfet", 7, 0, 0, 0, "GND"}};
  options.type_long_names = {"ntransistor"};
  CHECK(body(write(c.input, options).sim) == "n in s d 0 0 0 0 g=S_GND s=A_40,P_26 d=A_30,P_16\n");
  // A node/class pair already written yields A_0,P_0; -B drops the attributes.
  c.input.devices.push_back(c.input.devices.front());
  CHECK(body(write(c.input, options).sim) ==
        "n in s d 0 0 0 0 g=S_GND s=A_40,P_26 d=A_30,P_16\nn in s d 0 0 0 0 g=S_GND s=A_0,P_0 d=A_0,P_0\n");
  options.settings.no_attrs = true;
  CHECK(body(write(c.input, options).sim) == "n in s d 0 0 0 0\nn in s d 0 0 0 0\n");
}

TEST_CASE("sim terminal attributes in MIT format", "[cap:netlist-sim][req:Transistor attributes]") {
  Circuit c;
  const auto a = c.node("a");
  auto& d = c.device(sim::DeviceClass::mosfet, "nfet", {a, a, a});
  d.terminals[0].attributes = "g1";
  d.terminals[2].attributes = "ext:aph";
  CHECK(body(write(c.input).sim) == "n a a a 0 0 0 0 g=g1 d=ext:aph\n");
}

// Q-NETLIST-15: `C <name>0 <cap>` with magic-compat ON, `C <name> GND <cap>` OFF.
TEST_CASE("sim node capacitance line with magic-compat ON", "[cap:netlist-sim][req:Parasitic and node records]") {
  Circuit c;
  c.node("out", 5000);
  c.node("small", 2000);  // 2.0 is not above cthresh 2
  CHECK(body(write(c.input, {}, true).sim) == "C out0 5.0\n");
}

TEST_CASE("sim node capacitance line with magic-compat OFF", "[cap:netlist-sim][req:Parasitic and node records]") {
  Circuit c;
  c.node("out", 5000);
  CHECK(body(write(c.input, {}, false).sim) == "C out GND 5.0\n");
}

TEST_CASE("sim lumped resistance, attributes and aliases", "[cap:netlist-sim][req:Parasitic and node records]") {
  Circuit c;
  c.node("w", 0, 25600);
  c.node("v", 0, 10400);  // (10400 + 500) / 1000 = 10: not above rthresh 10
  const auto a = c.node("a");
  c.input.nodes[a].names.push_back(sim::NodeName{{"u1", "x"}, false});
  c.input.nodes[a].attributes = {"k1", "k2"};
  const auto vdd = c.node("vdd!");
  c.input.nodes[vdd].names.push_back(sim::NodeName{{"u1", "vdd!"}, true});  // both global: skipped
  c.input.nodes[vdd].names.push_back(sim::NodeName{{"u1", "p"}, false});
  CHECK(body(write(c.input).sim) == "R w 26\nA a k1,k2\n= a u1/x\n= vdd! u1/p\n");
  sim::WriteOptions no_attrs;
  no_attrs.settings.no_attrs = true;
  CHECK(body(write(c.input, no_attrs).sim) == "R w 26\n= a u1/x\n= vdd! u1/p\n");
}

TEST_CASE("sim coupling threshold and the -F node filter", "[cap:netlist-sim][req:Parasitic and node records]") {
  Circuit c;
  const auto a = c.node("a", 3000);
  c.node("b", 3000);
  c.input.caps.push_back(sim::SimCap{{"a"}, {"b"}, 2000});  // 2.0 is not above 2
  c.input.caps.push_back(sim::SimCap{{"a"}, {"b"}, 2100});
  c.device(sim::DeviceClass::mosfet, "nfet", {a, a});
  sim::WriteOptions options;
  options.settings.dev_nodes_only = true;
  CHECK(body(write(c.input, options).sim) == "n a a 0 0 0 0\nC a b 2.1\nC a0 3.0\n");
}

// Q-NETLIST-52: the alias and label texts exist only when their option is on.
TEST_CASE("sim alias and label files only when on", "[cap:netlist-sim][req:Command options and defaults]") {
  Circuit c;
  const auto a = c.node("a");
  c.input.nodes[a].names.push_back(sim::NodeName{{"u1", "x"}, false});
  c.input.nodes[a].location = {3, 4};
  c.input.nodes[a].layer = "metal1";
  sim::WriteOptions options;
  options.settings.alias = true;
  options.settings.labels = true;
  auto out = write(c.input, options);
  CHECK(body(out.sim).empty());
  REQUIRE(out.alias.has_value());
  CHECK(*out.alias == "= a u1/x\n");
  REQUIRE(out.labels.has_value());
  CHECK(*out.labels == "a 3 4 metal1\n");
  out = write(c.input);
  CHECK(body(out.sim) == "= a u1/x\n");
  CHECK_FALSE(out.alias.has_value());
  CHECK_FALSE(out.labels.has_value());
}
