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
#include <vector>

#include "../command/resist_fixture.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/extflat/hierarchy.hpp"

using namespace resist_test;  // NOLINT(google-build-using-namespace): the fixture's aliases
namespace extflat = hefesto::extflat;

namespace {

extresist::Network two_node_net(const std::string& a, const std::string& b) {
  extresist::Network n;
  n.nodes = {extresist::NetNode{db::Point{0, 0}, a, 1000.0}, extresist::NetNode{db::Point{50, 0}, b, 500.0}};
  n.resistors = {extresist::NetResistor{0, 1, 2000.0}};
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

TEST_CASE("a simple overlay block", "[cap:extresist-output][req:Overlay file grammar and order]") {
  Session s;
  const extresist::Network net = two_node_net("a.t0", "a.n0");
  CHECK(extresist::net_block("a", net, true, 0.0, style_of(s)) ==
        "killnode \"a\"\n"
        "rnode \"a.t0\" 0 1000 0 0 0\n"
        "rnode \"a.n0\" 0 500 50 0 0\n"
        "resist \"a.t0\" \"a.n0\" 2000\n");
  // The node that keeps the net's name: its node capacitance is subtracted,
  // and the net is not killed.
  const extresist::Network kept = two_node_net("a", "a.n0");
  CHECK(extresist::net_block("a", kept, false, 400.0, style_of(s)) ==
        "rnode \"a\" 0 600 0 0 0\nrnode \"a.n0\" 0 500 50 0 0\nresist \"a\" \"a.n0\" 2000\n");
}

TEST_CASE("a global net's nodes are named without the trailing bang",
          "[cap:extresist-output][req:Naming of network nodes]") {
  extresist::Network net = two_node_net("", "");
  CHECK(extresist::name_nodes("vdd!", net, {}));
  CHECK(net.nodes[0].name == "vdd.n0");
  CHECK(net.nodes[1].name == "vdd.n1");
}

TEST_CASE("terminal nodes are named first and a node with the net's name keeps it",
          "[cap:extresist-output][req:Naming of network nodes]") {
  extresist::Network net;
  net.nodes = {extresist::NetNode{{0, 0}, "", 0}, extresist::NetNode{{1, 0}, "", 0},
               extresist::NetNode{{2, 0}, "out", 0}};
  const std::vector<extresist::TerminalNode> terminals{{0, extresist::Terminal::source, 1},
                                                       {1, extresist::Terminal::drain, 2}};
  CHECK_FALSE(extresist::name_nodes("out", net, terminals));  // "out" stays: not killed
  CHECK(net.nodes[1].name == "out.t0");
  CHECK(net.nodes[2].name == "out");  // named already: the counter does not advance
  CHECK(net.nodes[0].name == "out.n0");
}

TEST_CASE("a MOSFET device record", "[cap:extresist-output][req:Overlay file grammar and order]") {
  Session s;
  const auto m = read_model(s,
                            "node \"in\" 0 0 0 0 poly\nnode \"out\" 0 0 0 0 ndiff\nnode \"x\" 0 0 0 0 ndiff\n"
                            "device mosfet nfet 10 20 11 21 2 8 \"GND\" \"in\" 2 0 \"out\" 8 0 \"x\" 8 0\n");
  REQUIRE(m.ok());
  const extresist::InputDevice& d = m.value().devices[0];
  CHECK(extresist::format_device(m.value(), d, {"", "out.t0", "", ""}, "GND", hefesto::compat::CompatSettings{true}) ==
        "device mosfet nfet 10 20 11 21  2 8 \"GND\" \"in\" 4 0 \"out.t0\" 8 0 \"x\" 8 0\n");
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
  CHECK(extresist::format_device(m, m.devices[0], {"a.t0", "", "", ""}, "", hefesto::compat::CompatSettings{true}) ==
        "device devres rpoly 3 4 4 5  \"None\" \"a.t0\" 20 0 \"b\" 2 0\n");
}

TEST_CASE("a resistor device record with magic-compat OFF", "[cap:extresist-output][req:Overlay file grammar and order]") {
  const extresist::InputModel m = resistor_model();
  CHECK(extresist::format_device(m, m.devices[0], {"a.t0", "", "", ""}, "", hefesto::compat::CompatSettings{false}) ==
        "device devres rpoly 3 4 4 5  10 2 \"None\" \"a.t0\" 20 0 \"b\" 2 0\n");
}

TEST_CASE("the overlay: scale line, net blocks, then the renamed devices",
          "[cap:extresist-output][req:Overlay file grammar and order]") {
  Session s;
  const auto m = read_model(s,
                            "node \"in\" 0 0 0 0 poly\nnode \"out\" 0 0 0 0 ndiff\nnode \"x\" 0 0 0 0 ndiff\n"
                            "device mosfet nfet 10 20 11 21 2 8 \"GND\" \"in\" 2 0 \"out\" 8 0 \"x\" 8 0\n"
                            "device mosfet nfet 30 20 31 21 2 8 \"GND\" \"in\" 2 0 \"x\" 8 0 \"x\" 8 0\n");
  REQUIRE(m.ok());
  std::vector<extresist::OutputNet> nets(1);
  nets[0].net = *m.value().find("out");
  nets[0].network = two_node_net("", "");
  nets[0].terminals = {extresist::TerminalNode{0, extresist::Terminal::source, 0}};
  const auto text = extresist::write_res_ext(m.value(), nets, style_of(s), hefesto::compat::CompatSettings{true});
  REQUIRE(text.ok());
  CHECK(text.value() == extresist::res_ext_scale(style_of(s)) +
                            "killnode \"out\"\nrnode \"out.t0\" 0 1000 0 0 0\nrnode \"out.n0\" 0 500 50 0 0\n"
                            "resist \"out.t0\" \"out.n0\" 2000\n"
                            "device mosfet nfet 10 20 11 21  2 8 \"GND\" \"in\" 4 0 \"out.t0\" 8 0 \"x\" 8 0\n");
  CHECK(extresist::write_res_lump(m.value(), nets) == "R out 0\n");
}

namespace {

// EXTFLAT's read of top.ext with its overlay; the flat resistors.
std::vector<extflat::FlatResist> overlay_resistors(bool magic_compat) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic(platform::Path{"/w/top.ext"},
                          "timestamp 0\nversion 8.3\ntech restest\nscale 1 1 1\nnode \"a\" 0 0 0 0 poly\n")
              .ok());
  extresist::Network net = two_node_net("", "");
  (void)extresist::name_nodes("a", net, {});
  hefesto::extract::ExtStyle style;
  REQUIRE(fs.write_atomic(platform::Path{"/w/top.res.ext"},
                          "scale 1 1 1\n" + extresist::net_block("a", net, true, 0.0, style))
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
