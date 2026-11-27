// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `extresist fasthenry`, `geometry` and `box` runs, and the FastHenry heights
// of contact nodes (capability extresist-output "FastHenry geometry file",
// "Geometry centerlines"; extresist-command "Command options"; tasks.md
// 14.4; Q-NETLIST-21, Q-NETLIST-27). Owner: RESCMD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "../command/resist_fixture.hpp"

using namespace resist_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

namespace {

const std::string kOneNet =
    "timestamp 0\nversion 8.3\ntech restest\nstyle restest\nscale 1 1 1\n"
    "node \"a\" 20 30000 0 0 poly\nport \"a\" 0 0 0 1 1 poly\n";

void add_port(db::CellDef& cell, const std::string& text, std::uint32_t index) {
  db::Label label;
  label.text = text;
  label.rect = db::Rect{0, 0, 1, 1};
  label.port_directions = 1;
  label.port_index = index;
  cell.labels().push_back(label);
}

extresist::NetResistor resistor_of(tech::TileType type) {
  extresist::NetResistor r;
  r.a = 0;
  r.b = 1;
  r.type = type;
  return r;
}

}  // namespace

// Q-NETLIST-21 (fix): with no frequency ever given the `.freq` line is
// 10 MHz. The file has the reference plane, the port order and the end.
TEST_CASE("extresist fasthenry writes the cell's .fh file", "[cap:extresist-output][req:FastHenry geometry file]") {
  Session s;
  REQUIRE(s.files().write_atomic("top.ext", kOneNet).ok());
  add_port(*s.top, "a", 1);
  REQUIRE(s.run("extresist fasthenry").ok());
  const std::vector<std::string> printed = s.take();
  CHECK(has(printed, "Writing FastHenry-format geometry file \"top.fh\""));
  const std::string fh = s.file("top.fh");
  CHECK(fh.starts_with("* FastHenry output for magic cell top\n"));
  CHECK(fh.find("\n.freq fmin=1e+07 fmax=1e+07\n") != std::string::npos);
  CHECK(fh.ends_with("\n* Order of arguments to SPICE subcircuit call:\n* 1 a\n\n.end\n"));
  CHECK_FALSE(s.files().exists(platform::Path{"top.res.ext"}));
  // The run's settings are restored.
  CHECK(s.editor.extresist.options.extout);
  CHECK(s.editor.extresist.options.simplify);
}

TEST_CASE("extresist fasthenry skips a cell with no ports", "[cap:extresist-output][req:FastHenry geometry file]") {
  Session s;
  REQUIRE(s.files().write_atomic("top.ext", kOneNet).ok());
  REQUIRE(s.run("extresist fasthenry 1e9").ok());
  CHECK_FALSE(s.files().exists(platform::Path{"top.fh"}));
}

// Q-NETLIST-27 (fix, both modes): a node that touches only contacts takes
// the height of the contact's upper residue (metal1, plane order 1, 0.1).
TEST_CASE("a contact-only FastHenry node takes the contact's upper layer height",
          "[cap:extresist-output][req:FastHenry geometry file]") {
  for (const bool compat : {true, false}) {
    Session s(compat);
    const extresist::GeometryContext ctx{s.editor.technology.get(), s.editor.extract.current.get(), nullptr, compat};
    extresist::Network net;
    net.nodes.resize(2);
    net.nodes[0].name = "a";
    net.nodes[0].named = true;
    net.nodes[0].resistors = {0};
    net.nodes[1].name = "b";
    net.nodes[1].named = true;
    net.nodes[1].resistors = {0};
    net.resistors = {resistor_of(s.type("pc"))};
    extresist::FastHenryIds ids;
    std::vector<std::int32_t> node_ids;
    const std::string text = extresist::fasthenry_nodes(net, ctx, ids, node_ids);
    CHECK(text.find("Na x=0.00 y=0.00 z=0.10\n") != std::string::npos);
  }
}

// Standing rule (proposed Q-NETLIST-73, compat): Magic's loop keeps the
// height of the node's last non-contact resistor (metal1, 0.1) although its
// comment says the first (poly, plane order 0); OFF takes the first.
TEST_CASE("a FastHenry node's height per mode", "[cap:extresist-output][req:FastHenry geometry file]") {
  for (const bool compat : {true, false}) {
    Session s(compat);
    const extresist::GeometryContext ctx{s.editor.technology.get(), s.editor.extract.current.get(), nullptr, compat};
    extresist::Network net;
    net.nodes.resize(3);
    for (std::size_t i = 0; i < 3; ++i) {
      net.nodes[i].name = std::string(1, static_cast<char>('a' + i));
      net.nodes[i].named = true;
    }
    net.nodes[0].resistors = {0, 1};
    net.resistors = {resistor_of(s.type("poly")), resistor_of(s.type("metal1"))};
    net.resistors[1].b = 2;
    extresist::FastHenryIds ids;
    std::vector<std::int32_t> node_ids;
    const std::string text = extresist::fasthenry_nodes(net, ctx, ids, node_ids);
    CHECK(text.find(compat ? "Na x=0.00 y=0.00 z=0.10\n" : "Na x=0.00 y=0.00 z=0.00\n") != std::string::npos);
  }
}

TEST_CASE("extresist geometry runs without CORE-NOTIMPL-008", "[cap:extresist-output][req:Geometry centerlines]") {
  Session s;
  REQUIRE(s.files().write_atomic("top.ext", kOneNet).ok());
  const cmd::CommandResult r = s.run("extresist geometry");
  REQUIRE(r.ok());
  CHECK(has_prefix(s.take(), "Total Nets: 1\n"));
  CHECK_FALSE(s.files().exists(platform::Path{"top.res.ext"}));
}

namespace {

std::string resist_tech() {
  std::string t = tech_text();
  const std::string device = " device mosfet nfet nfet ndiff None GND\n";
  t.replace(t.find(device), device.size(), device + " resist metal1 100\n");
  return t;
}

}  // namespace

// `extresist box <type>` prints the network of the net under the box's
// lower-left corner, unreduced.
TEST_CASE("extresist box prints the resistors of the net under the box", "[cap:extresist-command][req:Command options]") {
  Session s(true, true, resist_tech());
  REQUIRE(db::paint(*s.top, *s.editor.technology, db::Rect{0, 0, 100, 4}, s.type("metal1")).ok());
  db::Label sink;
  sink.text = "x";
  sink.type = s.type("metal1");
  sink.rect = db::Rect{96, 0, 100, 4};
  s.top->labels().push_back(sink);
  REQUIRE(s.run("extresist box metal1").ok());
  const std::vector<std::string> printed = s.take();
  for (const std::string& line : printed) {
    UNSCOPED_INFO(line);
  }
  bool resistor = false;
  for (const std::string& line : printed) {
    resistor = resistor || (line.starts_with("r (") && line.find(") r=") != std::string::npos);
  }
  CHECK(resistor);
}

// Standing rule (proposed Q-NETLIST-75, compat): RES_BOX returns on a
// failed net without restoring ResOptionsFlags, so every on/off setting
// reads off afterwards; with magic-compat OFF the settings are kept.
TEST_CASE("a failed extresist box clears the settings with magic-compat ON",
          "[cap:extresist-command][req:Command options]") {
  Session s;
  REQUIRE(s.run("extresist box metal1").ok());
  CHECK_FALSE(s.editor.extresist.options.simplify);
  CHECK_FALSE(s.editor.extresist.options.extout);
}

TEST_CASE("a failed extresist box keeps the settings with magic-compat OFF",
          "[cap:extresist-command][req:Command options]") {
  Session s(false);
  REQUIRE(s.run("extresist box metal1").ok());
  CHECK(s.editor.extresist.options.simplify);
  CHECK(s.editor.extresist.options.extout);
}

namespace {

// A metal1 wire under net "a", driven from its port, with simplification
// off, a 1 ps delay cutoff, no resistance cutoff and every net processed:
// ResDoSimplify computes no Tdi, and the session's value is preset to 1 zs.
std::vector<std::string> stale_tdi_run(bool magic_compat) {
  Session s(magic_compat, true, resist_tech());
  REQUIRE(db::paint(*s.top, *s.editor.technology, db::Rect{0, 0, 100, 4}, s.type("metal1")).ok());
  REQUIRE(s.files()
              .write_atomic("top.ext",
                            "timestamp 0\nversion 8.3\ntech restest\nstyle restest\nscale 1 1 1\n"
                            "node \"a\" 20 30000 0 0 metal1\nport \"a\" 0 0 0 1 4 metal1\n")
              .ok());
  s.editor.extresist.last_tdi = 1.0;
  REQUIRE(s.run("extresist simplify off").ok());
  REQUIRE(s.run("extresist threshold 0").ok());
  REQUIRE(s.run("extresist mindelay 1").ok());
  REQUIRE(s.run("extresist all").ok());
  std::vector<std::string> printed = s.take();
  for (const std::string& line : printed) {
    UNSCOPED_INFO(line);
  }
  return printed;
}

}  // namespace

// Standing rule (proposed Q-NETLIST-72, compat): without simplification
// or a lump file ResDoSimplify leaves rg_Tdi as the last net that computed
// it set it, and the second cutoff reads that value; with magic-compat OFF
// no delay cutoff applies to such a net.
TEST_CASE("the second cutoff reads the session's last delay with magic-compat ON",
          "[cap:extresist-command][req:Second cutoff after extraction]") {
  CHECK(has_prefix(stale_tdi_run(true), "Total Nets: 1\nNets extracted: 1 (1.000000)\nNets output: 0 "));
}

TEST_CASE("the second cutoff applies no delay test without a delay with magic-compat OFF",
          "[cap:extresist-command][req:Second cutoff after extraction]") {
  CHECK(has_prefix(stale_tdi_run(false), "Total Nets: 1\nNets extracted: 1 (1.000000)\nNets output: 1 "));
}
