// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.res.ext` overlay text and its consumption (capability
// extresist-output "Overlay file grammar and order", "Naming of network
// nodes", "Consumption by netlisters"; tasks.md 14.3): the scale line, net
// blocks, node names, device records (Q-NETLIST-39 per mode), and EXTFLAT's
// overlay reader (Q-NETLIST-12 per mode) on the written text. The
// networks are hand-built from RESNET's data types. Owner: RESCMD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

#include "../command/resist_fixture.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/extflat/hierarchy.hpp"

using namespace resist_test;  // NOLINT(google-build-using-namespace): the fixture's aliases
namespace extflat = hefesto::extflat;

namespace {

// NetNode and NetResistor gained fields in M7 R4 (RESNET); these build the
// ones the output tests set.
[[maybe_unused]] extresist::NetNode net_node(db::Point at, std::string name, double cap) {
  extresist::NetNode n;
  n.location = at;
  n.named = !name.empty();
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

extresist::Network two_node_net(const std::string& a, const std::string& b) {
  extresist::Network n;
  n.nodes = {net_node(db::Point{0, 0}, a, 1000.0), net_node(db::Point{50, 0}, b, 500.0)};
  n.resistors = {net_resistor(0, 1, 2000.0)};
  return n;
}

const extract::ExtStyle& style_of(const Session& s) {
  return *s.editor.extract.current;
}

}  // namespace

TEST_CASE("the overlay scale line", "[cap:extresist-output][req:Overlay file grammar and order]") {
  Session s(true, true, tech_text(1000, 1));
  extract::ExtStyle style = style_of(s);
  style.units_per_lambda = 5.0F;
  CHECK(extresist::res_ext_scale(style) == "scale 1000 1 5\n");
}

extresist::InputNode input_node(const std::string& name, double node_cap = 0.0) {
  extresist::InputNode n;
  n.name = name;
  n.node_cap = node_cap;
  return n;
}

TEST_CASE("a simple overlay block", "[cap:extresist-output][req:Overlay file grammar and order]") {
  Session s;
  std::vector<std::string> messages;
  extresist::Network net = two_node_net("a.t0", "a.n0");
  CHECK(extresist::net_block(input_node("a"), net, false, style_of(s), messages) ==
        "killnode \"a\"\n"
        "rnode \"a.t0\" 0 1000 0 0 0\n"
        "rnode \"a.n0\" 0 500 50 0 0\n"
        "resist \"a.t0\" \"a.n0\" 2000\n");
  // The node that keeps the net's name: its node capacitance is subtracted,
  // and the net is not killed.
  extresist::Network kept = two_node_net("a", "a.n0");
  CHECK(extresist::net_block(input_node("a", 400.0), kept, false, style_of(s), messages) ==
        "rnode \"a\" 0 600 0 0 0\nrnode \"a.n0\" 0 500 50 0 0\nresist \"a\" \"a.n0\" 2000\n");
  CHECK(messages.empty());
}

TEST_CASE("a global net's nodes are named without the trailing bang",
          "[cap:extresist-output][req:Naming of network nodes]") {
  Session s;
  std::vector<std::string> messages;
  extresist::Network net = two_node_net("", "");
  const std::string block = extresist::net_block(input_node("vdd!"), net, false, style_of(s), messages);
  CHECK(block.starts_with("killnode \"vdd!\"\n"));
  CHECK(net.nodes[0].name == "vdd.n0");
  CHECK(net.nodes[1].name == "vdd.n1");
}

// ResPrintExtNode's orphan patch: a DONTKILL net no node keeps is attached
// to the first node with a zero resistor, and the first node has the net's
// own capacitance subtracted.
TEST_CASE("a net with a missing terminal is patched to its first node",
          "[cap:extresist-output][req:Naming of network nodes]") {
  Session s;
  std::vector<std::string> messages;
  extresist::Network net = two_node_net("GND.t0", "");
  CHECK(extresist::net_block(input_node("GND!", 400.0), net, true, style_of(s), messages) ==
        "rnode \"GND.t0\" 0 600 0 0 0\nrnode \"GND.n0\" 0 500 50 0 0\n"
        "resist \"GND!\" \"GND.t0\" 0.0\nresist \"GND.t0\" \"GND.n0\" 2000\n");
  REQUIRE(messages.size() == 1);
  CHECK(messages[0] == "Warning:  Orphaned node \"GND!\" arbitrarily attached to \"GND.t0\"");
}

namespace {

// Two nfets with their gates on "out", each matched to a layout device
// whose gate lands on a network node.
struct FixupCase {
  Session s;
  extresist::InputModel model;
  extresist::Network net;
  std::size_t out = 0;

  FixupCase() {
    auto m = read_model(s,
                        "node \"in\" 0 0 0 0 poly\nnode \"out\" 0 0 0 0 ndiff\nnode \"x\" 0 0 0 0 ndiff\n"
                        "device mosfet nfet 10 20 11 21 2 8 \"GND\" \"out\" 2 0 \"in\" 8 0 \"x\" 8 0\n"
                        "device mosfet nfet 30 20 31 21 2 8 \"GND\" \"out\" 2 0 \"x\" 8 0 \"in\" 8 0\n");
    REQUIRE(m.ok());
    model = std::move(m).value();
    out = *model.find("out");
    net.nodes = {net_node(db::Point{0, 0}, "", 0), net_node(db::Point{10, 0}, "", 0),
                 net_node(db::Point{30, 0}, "", 0)};
    net.devices.resize(2);
    for (std::size_t i = 0; i < 2; ++i) {
      net.devices[i].terminals = {i + 1, extresist::kNoNode};
      net.devices[i].inside = db::Rect{10 + 20 * static_cast<db::Coord>(i), 20, 12 + 20 * static_cast<db::Coord>(i), 25};
      net.devices[i].length = 1;
      net.devices[i].width = 5;
    }
    // The net's device list (newest first): the second device, then the first.
    net.input_devices = {1, 0};
  }
};

}  // namespace

// Regression (tasks.md 14.3, NL-RES-01): the terminal nodes are named
// `<net'>.t<k>` by ResFixUpConnections in the net's device order, the
// device records follow them, and the remaining nodes are `.n<k>`.
TEST_CASE("device terminals are renamed onto their network nodes",
          "[cap:extresist-output][req:Naming of network nodes]") {
  FixupCase c;
  std::vector<std::string> messages;
  CHECK_FALSE(extresist::fix_up_connections(c.model, c.out, c.net, hefesto::compat::CompatSettings{true}, messages));
  CHECK(messages.empty());
  CHECK(c.net.nodes[2].name == "out.t0");  // the second device comes first in the net's list
  CHECK(c.net.nodes[1].name == "out.t1");
  CHECK_FALSE(c.net.nodes[0].named);
  const extresist::InputDevice& first = c.model.devices[0];
  CHECK(c.model.nodes[*first.gate].name == "out.t1");
  CHECK(first.touched);
  REQUIRE(first.layout.has_value());
  CHECK(first.layout->location == db::Point{10, 20});
  std::vector<std::string> block_messages;
  (void)extresist::net_block(c.model.nodes[c.out], c.net, false, style_of(c.s), block_messages);
  CHECK(c.net.nodes[0].name == "out.n0");
  // The overlay's device record prints the layout device's corner and L W.
  CHECK(extresist::format_device(c.model, first, "GND", hefesto::compat::CompatSettings{true}) ==
        "device mosfet nfet 10 20 11 21  1 5 \"GND\" \"out.t1\" 2 0 \"in\" 5 0 \"x\" 5 0\n");
}

// A node that already has a name keeps it, and the counter steps back.
TEST_CASE("a named terminal node keeps its name and the counter steps back",
          "[cap:extresist-output][req:Naming of network nodes]") {
  FixupCase c;
  c.net.nodes[2].name = "out";
  c.net.nodes[2].named = true;
  std::vector<std::string> messages;
  CHECK_FALSE(extresist::fix_up_connections(c.model, c.out, c.net, hefesto::compat::CompatSettings{true}, messages));
  CHECK(c.net.nodes[2].name == "out");
  CHECK(c.net.nodes[1].name == "out.t0");
  CHECK(c.model.nodes[*c.model.devices[1].gate].name == "out");
}

TEST_CASE("a terminal with no layout node is reported and keeps the net",
          "[cap:extresist-output][req:Naming of network nodes]") {
  FixupCase c;
  c.net.devices[0].terminals[0] = extresist::kNoNode;
  std::vector<std::string> messages;
  CHECK(extresist::fix_up_connections(c.model, c.out, c.net, hefesto::compat::CompatSettings{true}, messages));
  REQUIRE(messages.size() == 1);
  CHECK(messages[0] == "Missing gate connection of device at (10 20) on net out");
}

TEST_CASE("a MOSFET device record", "[cap:extresist-output][req:Overlay file grammar and order]") {
  Session s;
  auto m = read_model(s,
                      "node \"in\" 0 0 0 0 poly\nnode \"out\" 0 0 0 0 ndiff\nnode \"x\" 0 0 0 0 ndiff\n"
                      "device mosfet nfet 10 20 11 21 2 8 \"GND\" \"in\" 2 0 \"out\" 8 0 \"x\" 8 0\n");
  REQUIRE(m.ok());
  const extresist::InputDevice& d = m.value().devices[0];
  CHECK(extresist::format_device(m.value(), d, "GND", hefesto::compat::CompatSettings{true}) ==
        "device mosfet nfet 10 20 11 21  2 8 \"GND\" \"in\" 4 0 \"out\" 8 0 \"x\" 8 0\n");
}

namespace {

extresist::InputModel resistor_model() {
  extresist::InputModel m;
  m.nodes.resize(2);
  m.nodes[0].name = "a";
  m.nodes[1].name = "b";
  extresist::InputDevice r;
  r.device_class = "resistor";
  r.model = "rpoly";
  r.location = db::Point{3, 4};
  r.leading = {"10", "2"};
  r.gate = 0;
  r.source = 1;
  r.length = 10;
  r.width = 2;
  r.style_class = extract::DeviceClass::devres;
  r.sd_count = 1;
  m.devices.push_back(r);
  return m;
}

}  // namespace

// Q-NETLIST-39 (compat): ON, as Magic, no value or L W fields.
TEST_CASE("a resistor device record with magic-compat ON", "[cap:extresist-output][req:Overlay file grammar and order]") {
  const extresist::InputModel m = resistor_model();
  CHECK(extresist::format_device(m, m.devices[0], "", hefesto::compat::CompatSettings{true}) ==
        "device devres rpoly 3 4 4 5  \"None\" \"a\" 20 0 \"b\" 2 0\n");
}

TEST_CASE("a resistor device record with magic-compat OFF", "[cap:extresist-output][req:Overlay file grammar and order]") {
  const extresist::InputModel m = resistor_model();
  CHECK(extresist::format_device(m, m.devices[0], "", hefesto::compat::CompatSettings{false}) ==
        "device devres rpoly 3 4 4 5  10 2 \"None\" \"a\" 20 0 \"b\" 2 0\n");
}

TEST_CASE("the overlay: scale line, net blocks, then the touched devices",
          "[cap:extresist-output][req:Overlay file grammar and order]") {
  FixupCase c;
  std::vector<std::string> messages;
  (void)extresist::fix_up_connections(c.model, c.out, c.net, hefesto::compat::CompatSettings{true}, messages);
  std::vector<extresist::OutputNet> nets(1);
  nets[0].net = c.out;
  nets[0].block = extresist::net_block(c.model.nodes[c.out], c.net, false, style_of(c.s), messages);
  nets[0].network = c.net;
  const auto text = extresist::write_res_ext(c.model, nets, style_of(c.s), hefesto::compat::CompatSettings{true});
  REQUIRE(text.ok());
  // Reverse read order (ResRDevList): the second device first.
  CHECK(text.value() == extresist::res_ext_scale(style_of(c.s)) + nets[0].block +
                            "device mosfet nfet 30 20 31 21  1 5 \"GND\" \"out.t0\" 2 0 \"x\" 5 0 \"in\" 5 0\n"
                            "device mosfet nfet 10 20 11 21  1 5 \"GND\" \"out.t1\" 2 0 \"in\" 5 0 \"x\" 5 0\n");
  CHECK(extresist::write_res_lump(c.model, nets) == "R out 0\n");
}

namespace {

// EXTFLAT's read of top.ext with its overlay; the flat resistors.
std::vector<extflat::FlatResist> overlay_resistors(bool magic_compat) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic(platform::Path{"/w/top.ext"},
                          "timestamp 0\nversion 8.3\ntech restest\nscale 1 1 1\nnode \"a\" 0 0 0 0 poly\n")
              .ok());
  extresist::Network net = two_node_net("", "");
  hefesto::extract::ExtStyle style;
  extresist::InputNode a;
  a.name = "a";
  std::vector<std::string> messages;
  REQUIRE(fs.write_atomic(platform::Path{"/w/top.res.ext"},
                          "scale 1 1 1\n" + extresist::net_block(a, net, false, style, messages))
              .ok());
  diag::CollectingSink sink;
  hefesto::cell::SearchPaths paths;
  hefesto::cell::PathVariables vars;
  hefesto::cell::LookupContext ctx{fs, paths, vars, sink};
  extflat::ReadOptions options;
  options.search_path = {"/w"};
  options.resist_overlay = true;
  options.magic_compat = magic_compat;
  auto hierarchy = extflat::read(ctx, "top", options);
  REQUIRE(hierarchy.ok());
  auto flat = extflat::flatten(hierarchy.value(), extflat::FlatFlags::flat_nodes | extflat::FlatFlags::flat_resists,
                               extflat::FlattenOptions{}, sink);
  REQUIRE(flat.ok());
  return flat.value().resists;
}

}  // namespace

// Q-NETLIST-12 (compat): ON, Magic's +0.5 mohm on every overlay resistor.
TEST_CASE("overlay resistors as netlisters read them with magic-compat ON",
          "[cap:extresist-output][req:Consumption by netlisters]") {
  const auto resists = overlay_resistors(true);
  REQUIRE(resists.size() == 1);
  CHECK(resists[0].resistance == 2000.5);
}

TEST_CASE("overlay resistors as netlisters read them with magic-compat OFF",
          "[cap:extresist-output][req:Consumption by netlisters]") {
  const auto resists = overlay_resistors(false);
  REQUIRE(resists.size() == 1);
  CHECK(resists[0].resistance == 2000.0);
}
