// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SPICE/CDL reader (capability lvs-netlist-input; tasks.md 6.2). Written
// from the spec text only (netgen clean room, Q-LVS-30); no NG-READ golden
// comparisons yet (G2-LVS has not passed).
//
// Owner: LVSREAD.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/lvs/net/codes.hpp"
#include "hefesto/lvs/read/codes.hpp"
#include "read_fixture.hpp"

using lvs_test::connections;
using lvs_test::instance;
using lvs_test::port_names;
using lvs_test::property;
using lvs_test::Reading;
namespace lvs = hefesto::lvs;
namespace codes = hefesto::lvs::read_codes;
using Names = std::vector<std::string>;

// ---------------------------------------------------------------- lexical

TEST_CASE("continuation with interleaved comment", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  REQUIRE(r.read("M1 d g s b nfet\n* note\n+ W=1u L=0.15u\n").ok());
  const auto& m1 = instance(r.cell("in.spice"), "nfet:1");
  CHECK(property(m1, "W") == "1u");
  CHECK(property(m1, "L") == "0.15u");
}

TEST_CASE("CDL global comment", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading a;
  REQUIRE(a.read("*.GLOBAL VDD VSS\n").ok());
  Reading b;
  REQUIRE(b.read(".GLOBAL VDD VSS\n").ok());
  CHECK(lvs_test::dump(a.db) == lvs_test::dump(b.db));
  const auto& top = a.cell("in.spice");
  REQUIRE(top.find_net("vdd") != lvs::kNoNet);
  CHECK(top.net(top.find_net("VDD")).global);
  CHECK(top.net(top.find_net("VSS")).global);
}

TEST_CASE("backslash joins physical lines", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  REQUIRE(r.read("R1 a \\\nb 1k\r\n").ok());
  const auto& top = r.cell("in.spice");
  CHECK(connections(top, instance(top, "r:1")) == Names{"a", "b"});
}

TEST_CASE("quotes make one token", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  REQUIRE(r.read("R1 a b 'w * 2' \"x y\"\nX1 n1 n2 sub p=2'x 'y z'\n").ok());
  const auto& top = r.cell("in.spice");
  const auto& r1 = instance(top, "\"xy\":1");  // the quoted token is the model
  CHECK(property(r1, "value") == "'w*2'");
  CHECK(r1.cell == *r.db.find_cell("\"xy\"", 0));
  // A single quote after a digit opens no quote: `2'x` stays one token and
  // `'y z'` is the next one.
  CHECK(property(instance(top, "sub:1"), "p") == "2'x");
  CHECK(r.printed("Token \"'yz'\" is not a parameter!"));
}

TEST_CASE("card names and element letters ignore case", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  REQUIRE(r.read(".SuBcKt cell a b\nr1 a b 1k\nm2 a a b b nfet\n.EnDs\n").ok());
  const auto& cell = r.cell("cell");
  CHECK(cell.instances.size() == 2);
  CHECK(r.sink.snapshot().empty());
}

TEST_CASE("control blocks are ignored", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  REQUIRE(r.read("R1 a b 1k\n.control\nR9 x y 1k\nrun\n.endc\nR2 a b 2k\n.END\n").ok());
  const auto& top = r.cell("in.spice");
  CHECK(top.instances.size() == 2);
  CHECK(r.sink.snapshot().empty());
}

TEST_CASE("unknown cards are skipped with a warning", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  REQUIRE(r.read(".option scale=1\nZ1 a b\n").ok());
  CHECK(r.texts(codes::kIgnoredCard) ==
        Names{"Ignoring line starting with token: .option", "Ignoring line starting with token: Z1"});
  CHECK(r.texts(codes::kSummary) == Names{"File in.spice read with 2 warnings."});
}

TEST_CASE("warnings stop after one hundred", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  std::string text;
  for (int i = 0; i < 150; ++i) {
    text += "Z" + std::to_string(i) + " a b\n";
  }
  REQUIRE(r.read(text).ok());
  CHECK(r.texts(codes::kIgnoredCard).size() == 100);
  CHECK(r.texts(codes::kTooManyWarnings) == Names{"Too many warnings. . . will not report any more."});
  CHECK(r.texts(codes::kSummary) == Names{"File in.spice read with 150 warnings."});
}

TEST_CASE("a single warning is singular", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  REQUIRE(r.read("Z1 a b\n").ok());
  CHECK(r.texts(codes::kSummary) == Names{"File in.spice read with 1 warning."});
}

TEST_CASE("binary input stops the read", "[cap:lvs-netlist-input][req:SPICE lexical structure]") {
  Reading r;
  const auto read = r.read(std::string{"R1 a b 1k\n\x89PNG\x01\x02\xff\xfe\xfd x\n"});
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kBinary);
  CHECK(read.error().text == "Input file \"in.spice\" appears to be binary. . . bailing out");
  // A token is binary when it is shorter than four times its count of
  // non-ASCII bytes; a longer UTF-8 name is not.
  Reading utf8;
  CHECK(utf8.read("R1 n\xC3\xA9t_a_b b 1k\n").ok());
  Reading short_utf8;
  CHECK_FALSE(short_utf8.read("R1 n\xC3\xA9t b 1k\n").ok());
}

// ---------------------------------------------------------------- .subckt

TEST_CASE("parameters on the header", "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  REQUIRE(r.read(".subckt res2 a b PARAMS: w=1 l=2\n.ends\n").ok());
  const auto& res2 = r.cell("res2");
  CHECK(port_names(res2) == Names{"a", "b"});
  REQUIRE(res2.properties.size() == 2);
  CHECK(res2.properties[0] == lvs::PropertyDef{"w", lvs::PropertyType::string, 0.0, "1"});
  CHECK(res2.properties[1] == lvs::PropertyDef{"l", lvs::PropertyType::string, 0.0, "2"});
  CHECK(res2.device_class == lvs::DeviceClass::subcircuit);
}

TEST_CASE("a definition without ports gets a proxy port", "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  REQUIRE(r.read(".subckt filler\nR1 a b 1k\n.ends\n").ok());
  const auto& filler = r.cell("filler");
  REQUIRE(filler.ports.size() == 1);
  CHECK(filler.ports[0].proxy);
  CHECK(filler.ports[0].net != lvs::kNoNet);  // a fresh net at .ENDS
}

TEST_CASE("duplicate port names share a net", "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  REQUIRE(r.read(".subckt feed a a b\n.ends\n").ok());
  const auto& feed = r.cell("feed");
  CHECK(feed.ports[0].net == feed.ports[1].net);
  CHECK(feed.ports[0].net != feed.ports[2].net);
}

TEST_CASE("nested definitions", "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  REQUIRE(r.read(".subckt outer a\n.subckt inner b\nR1 b x 1k\n.ends\nXi a inner\n.ends\n.ends\n").ok());
  CHECK(r.texts(codes::kMissingEnds) == Names{"Missing .ENDS statement on subcircuit."});
  CHECK(r.texts(codes::kStrayEnds) == Names{".ENDS occurred outside of a subcircuit!"});
  const auto& outer = r.cell("outer");
  REQUIRE(outer.instances.size() == 1);
  CHECK(r.db.cells.at(outer.instances[0].cell).name == "inner");
  CHECK(r.cell("inner").instances.size() == 1);
}

TEST_CASE("end of file inside a definition closes it", "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  const auto top = r.read(".subckt open a b\nR1 a b 1k\n");
  REQUIRE(top.ok());
  CHECK(r.texts(codes::kMissingEnds) == Names{"Missing .ENDS statement on subcircuit."});
  CHECK(r.db.cells.at(top.value()).name == "open");
  CHECK(r.cell("open").instances.size() == 1);
}

TEST_CASE("globals are copied in and unconnected ones removed",
          "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  REQUIRE(r.read(".global vdd gnd\n.subckt a x\n.ends\n.subckt b x\nR1 x vdd 1k\n.ends\n").ok());
  CHECK(r.cell("a").find_net("vdd") == lvs::kNoNet);
  const auto& b = r.cell("b");
  REQUIRE(b.find_net("vdd") != lvs::kNoNet);
  CHECK(b.net(b.find_net("vdd")).global);
  CHECK(b.find_net("gnd") == lvs::kNoNet);
  CHECK(connections(b, instance(b, "r:1")) == Names{"x", "vdd"});
}

TEST_CASE("redefinition", "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  REQUIRE(r.read(".subckt buf a y\nR1 a y 1k\n.ends\nX1 p q buf\n.subckt buf a y\n.ends\n.subckt buf a y\n.ends\n").ok());
  const auto& first = r.cell("buf[[0]]");
  CHECK(first.flags.duplicate);
  CHECK(first.instances.size() == 1);
  CHECK(r.cell("buf[[1]]").flags.duplicate);
  const auto& current = r.cell("buf");
  CHECK_FALSE(current.flags.duplicate);
  // The earlier instance follows its renamed class.
  const auto& top = r.cell("in.spice");
  CHECK(r.db.cells.at(instance(top, "buf:1").cell).name == "buf[[0]]");
  CHECK(r.texts(codes::kDuplicateCell) == Names{"Duplicate cell buf in file", "Renaming original cell to buf[[0]]",
                                                "Duplicate cell buf in file", "Renaming original cell to buf[[1]]"});
}

TEST_CASE("a definition replaces a placeholder", "[cap:lvs-netlist-input][req:SPICE subcircuit definitions]") {
  Reading r;
  REQUIRE(r.read("X1 n1 n2 later\n.subckt later p q\nR1 p q 1k\n.ends\n").ok());
  const auto& later = r.cell("later");
  CHECK_FALSE(later.flags.placeholder);
  CHECK(later.device_class == lvs::DeviceClass::subcircuit);
  CHECK(port_names(later) == Names{"p", "q"});
  const auto& top = r.cell("in.spice");
  const auto& x1 = instance(top, "later:1");
  CHECK(x1.cell == *r.db.find_cell("later", 0));
  CHECK(connections(top, x1) == Names{"n1", "n2"});
  CHECK(r.texts(codes::kDuplicateCell).empty());
}

// ---------------------------------------------------------------- primitives

TEST_CASE("MOSFET model cell", "[cap:lvs-netlist-input][req:SPICE primitive device cards]") {
  Reading r;
  REQUIRE(r.read("M1 y a vss vss sky130_fd_pr__nfet_01v8 W=1 L=0.15\n").ok());
  const auto& model = r.cell("sky130_fd_pr__nfet_01v8");
  CHECK(model.device_class == lvs::DeviceClass::fet);
  CHECK(port_names(model) == Names{"drain", "gate", "source", "bulk"});
  REQUIRE(model.properties.size() == 3);
  CHECK(model.properties[0] == lvs::PropertyDef{"L", lvs::PropertyType::real, 0.01, "0"});
  CHECK(model.properties[1] == lvs::PropertyDef{"W", lvs::PropertyType::real, 0.01, "0"});
  CHECK(model.properties[2] == lvs::PropertyDef{"M", lvs::PropertyType::integer, 0.0, "1"});
  const auto& top = r.cell("in.spice");
  const auto& m1 = instance(top, "sky130_fd_pr__nfet_01v8:1");
  CHECK(connections(top, m1) == Names{"y", "a", "vss", "vss"});
  CHECK(property(m1, "W") == "1");
  CHECK(property(m1, "L") == "0.15");
}

TEST_CASE("plain resistor", "[cap:lvs-netlist-input][req:SPICE primitive device cards]") {
  Reading r;
  REQUIRE(r.read("R5 a b 1k\n").ok());
  const auto& resistor = r.cell("r");
  CHECK(resistor.device_class == lvs::DeviceClass::resistor);
  CHECK(port_names(resistor) == Names{"end_a", "end_b"});
  REQUIRE(resistor.properties.size() == 1);
  CHECK(resistor.properties[0] == lvs::PropertyDef{"value", lvs::PropertyType::real, 0.01, "0"});
  CHECK(property(instance(r.cell("in.spice"), "r:5"), "value") == "1k");
}

TEST_CASE("value and model tokens of C and R", "[cap:lvs-netlist-input][req:SPICE primitive device cards]") {
  Reading r;
  REQUIRE(r.read("C1 a b 1p cmim\nC2 a b\nR2 a b 2k $[rpoly] m=2\nR3 a b rhigh\n").ok());
  const auto& cmim = r.cell("cmim");
  CHECK(cmim.device_class == lvs::DeviceClass::capacitor);
  CHECK(port_names(cmim) == Names{"top", "bottom"});
  CHECK(cmim.properties[0] == lvs::PropertyDef{"value", lvs::PropertyType::value, 0.01, "0"});
  CHECK(r.cell("c").device_class == lvs::DeviceClass::capacitor);
  const auto& top = r.cell("in.spice");
  CHECK(property(instance(top, "cmim:1"), "value") == "1p");
  CHECK(instance(top, "c:2").properties.front().empty());
  const auto& r2 = instance(top, "rpoly:2");
  CHECK(property(r2, "value") == "2k");
  CHECK(property(r2, "m") == "2");
  CHECK(r.cell("rpoly").properties[0].type == lvs::PropertyType::value);
  CHECK(property(instance(top, "rhigh:3"), "value") == "<none>");
}

TEST_CASE("every primitive card makes its class", "[cap:lvs-netlist-input][req:SPICE primitive device cards]") {
  Reading r;
  REQUIRE(r.read("Q1 c b e npn1\nD1 a k dio\nL1 a b 1n\nT1 a b c d tl\nV1 p n 1.8\nI1 p n 1m isrc\n"
                 "E1 p n cp cn 2 amp\n")
              .ok());
  CHECK(r.cell("npn1").device_class == lvs::DeviceClass::bjt);
  CHECK(port_names(r.cell("npn1")) == Names{"collector", "base", "emitter"});
  CHECK(r.cell("dio").device_class == lvs::DeviceClass::diode);
  CHECK(port_names(r.cell("dio")) == Names{"anode", "cathode"});
  CHECK(r.cell("l").device_class == lvs::DeviceClass::inductor);
  CHECK(r.cell("tl").device_class == lvs::DeviceClass::xline);
  CHECK(port_names(r.cell("tl")) == Names{"node1", "node2", "node3", "node4"});
  CHECK(r.cell("v").device_class == lvs::DeviceClass::vsource);
  CHECK(r.cell("isrc").device_class == lvs::DeviceClass::isource);
  CHECK(r.cell("amp").device_class == lvs::DeviceClass::module);
  CHECK(port_names(r.cell("amp")) == Names{"pos", "neg", "ctrlp", "ctrln"});
  const auto& top = r.cell("in.spice");
  CHECK(property(instance(top, "l:1"), "value") == "1n");
  CHECK(property(instance(top, "v:1"), "value") == "1.8");
  CHECK(property(instance(top, "amp:1"), "value") == "2");
}

TEST_CASE("a model with the wrong port count is rejected", "[cap:lvs-netlist-input][req:SPICE primitive device cards]") {
  Reading r;
  REQUIRE(r.read(".subckt nfet a b\n.ends\nM1 d g s b nfet\nM2 d g s\n").ok());
  CHECK(r.texts(codes::kBadlyFormed) == Names{"Device \"nfet\" has wrong number of ports for a fet.",
                                              "Badly formed line in input.", "Badly formed line in input."});
  CHECK(r.cell("in.spice").instances.empty());
}

TEST_CASE(".MODEL reclassifies a model cell", "[cap:lvs-netlist-input][req:SPICE primitive device cards]") {
  Reading r;
  REQUIRE(r.read("M1 d g s b nch\nM2 d g s b pch\nQ1 c b e qp\n.model nch NMOS level=1\n.model pch pmos(l=1)\n"
                 ".model qp pnp\n.model other nmos\n")
              .ok());
  CHECK(r.cell("nch").device_class == lvs::DeviceClass::nmos);
  CHECK(r.cell("pch").device_class == lvs::DeviceClass::pmos);
  CHECK(r.cell("qp").device_class == lvs::DeviceClass::pnp);
  CHECK_FALSE(r.has_cell("other"));
}

TEST_CASE("ignoring R and C", "[cap:lvs-netlist-input][req:SPICE primitive device cards]") {
  Reading r;
  r.options.ignore_rc = true;
  REQUIRE(r.read("R1 a b 1k\nC1 a b 1p\nM1 a b c d nfet\n").ok());
  CHECK(r.cell("in.spice").instances.size() == 1);
  CHECK_FALSE(r.has_cell("r"));
}

// ---------------------------------------------------------------- calls

TEST_CASE("placeholder for a PDK device", "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  REQUIRE(r.read("XM1 d g s b sky130_fd_pr__nfet_01v8 w=1 l=0.15\n").ok());
  const auto& placeholder = r.cell("sky130_fd_pr__nfet_01v8");
  CHECK(placeholder.flags.placeholder);
  CHECK(placeholder.device_class == lvs::DeviceClass::module);
  CHECK(port_names(placeholder) == Names{"1", "2", "3", "4"});
  CHECK(placeholder.properties == std::vector<lvs::PropertyDef>{{"M", lvs::PropertyType::integer, 0.0, "1"}});
  const auto& xm1 = instance(r.cell("in.spice"), "sky130_fd_pr__nfet_01v8:M1");
  CHECK(property(xm1, "w") == "1");
  CHECK(r.texts(codes::kUndefinedCell) ==
        Names{"Call to undefined subcircuit sky130_fd_pr__nfet_01v8", "Creating placeholder cell definition."});
}

TEST_CASE("instance name already prefixed", "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  REQUIRE(r.read(".subckt inv a y\n.ends\nXinv_1 a y inv\nXu2 a y INV\n").ok());
  const auto& top = r.cell("in.spice");
  CHECK(connections(top, instance(top, "/inv_1")) == Names{"a", "y"});
  CHECK(instance(top, "INV:u2").cell == *r.db.find_cell("inv", 0));
}

TEST_CASE("CDL slash syntax", "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  REQUIRE(r.read(".subckt inv a y\n.ends\nX1 a y / inv\nX2 a y /inv PARAMS: m=2\n").ok());
  const auto& top = r.cell("in.spice");
  CHECK(connections(top, instance(top, "inv:1")) == Names{"a", "y"});
  CHECK(property(instance(top, "inv:2"), "m") == "2");
  CHECK(r.texts(codes::kUndefinedCell).empty());
}

TEST_CASE("a cell named like a parameter is a cell", "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  REQUIRE(r.read(".subckt a=b x\n.ends\nX1 n a=b m=3\n").ok());
  const auto& top = r.cell("in.spice");
  const auto& x1 = instance(top, "a=b:1");
  CHECK(connections(top, x1) == Names{"n"});
  CHECK(property(x1, "m") == "3");
}

TEST_CASE("a cell instanced inside itself stops the read",
          "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  const auto read = r.read(".subckt loop a\nX1 a loop\n.ends\n");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kSelfInstance);
  CHECK(read.error().text == "Fatal: Class \"loop\" is instanced inside of itself!");
}

TEST_CASE("ignored and shorted classes", "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  r.options.ignore = {{"fill", lvs::IgnoredClass::Kind::class_}, {"short", lvs::IgnoredClass::Kind::shorted}};
  REQUIRE(r.read("X1 a b fill\nX2 a A short\nX3 a b short\n").ok());
  const auto& top = r.cell("in.spice");
  REQUIRE(top.instances.size() == 1);
  CHECK(top.instances[0].name == "short:3");
  CHECK(r.texts(codes::kIgnoredCall) == Names{"Class 'fill' instanced in input but is being ignored.",
                                              "Instance of 'short' is shorted, ignoring."});
}

TEST_CASE("connection count mismatch", "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  REQUIRE(r.read(".subckt inv a y\n.ends\nX1 a y z inv\nX2 a inv\n").ok());
  CHECK(r.texts(codes::kParameterMismatch) == Names{"Parameter list mismatch in inv:1: Too many parameters in call!",
                                                    "Parameter list mismatch in inv:2: Not enough parameters in call!"});
  const auto& top = r.cell("in.spice");
  CHECK(connections(top, instance(top, "inv:1")) == Names{"a", "y"});
  CHECK(connections(top, instance(top, "inv:2")) == Names{"a", "-"});
}

TEST_CASE("a placeholder without connections has a proxy port",
          "[cap:lvs-netlist-input][req:SPICE subcircuit calls and placeholders]") {
  Reading r;
  REQUIRE(r.read("X1 logo\nX2 logo\n").ok());
  const auto& logo = r.cell("logo");
  REQUIRE(logo.ports.size() == 1);
  CHECK(logo.ports[0].proxy);
  CHECK(r.texts(codes::kParameterMismatch).empty());
}

// ---------------------------------------------------------------- globals, includes, params

TEST_CASE("relative include", "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  Reading r;
  r.add_file("/work/lvs/top.spice", ".include \"cells/inv.spice\"\nX1 a y inv\n");
  r.add_file("/work/lvs/cells/inv.spice", ".subckt inv a y\nR1 a y 1k\n.ends\n");
  const auto top = r.read_file("/work/lvs/top.spice");
  REQUIRE(top.ok());
  CHECK(r.cell("inv", 0).file == 0);
  CHECK(r.db.cells.at(top.value()).name == "/work/lvs/top.spice");
  CHECK(r.texts(codes::kUndefinedCell).empty());
}

TEST_CASE("include quotes are stripped", "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  Reading r;
  r.add_file("/w/a.sp", "R1 a b 1k\n");
  r.add_file("/w/b.sp", "R1 a b 1k\n");
  r.add_file("/w/c.sp", "R1 a b 1k\n");
  REQUIRE(r.read(".include '/w/a.sp'\n.include `/w/b.sp`\n.INCLUDE /w/c.sp\n").ok());
  CHECK(r.cell("in.spice").instances.size() == 3);
}

TEST_CASE("home directory in includes (Q-LVS-14)", "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  Reading desktop;
  desktop.add_file("/home/me/lib/r.sp", "R1 a b 1k\n");
  desktop.add_file("/home/ann/r.sp", "R2 a b 1k\n");
  desktop.options.home_directory = [](std::string_view user) -> std::optional<std::string> {
    return user.empty() ? "/home/me" : "/home/" + std::string{user};
  };
  REQUIRE(desktop.read(".include ~/lib/r.sp\n.include ~ann/r.sp\n").ok());
  CHECK(desktop.cell("in.spice").instances.size() == 2);

  // iPadOS: `~` is the documents root and `~user` is rejected.
  Reading ipad;
  ipad.add_file("/Documents/r.sp", "R1 a b 1k\n");
  ipad.options.home_directory = [](std::string_view user) -> std::optional<std::string> {
    if (!user.empty()) {
      return std::nullopt;
    }
    return std::string{"/Documents"};
  };
  REQUIRE(ipad.read(".include ~/r.sp\n").ok());
  const auto rejected = ipad.read(".include ~ann/r.sp\n", "other.spice");
  REQUIRE_FALSE(rejected.ok());
  CHECK(rejected.error().code == codes::kPathUnreadable);
  CHECK(rejected.error().text == "cannot expand '~ann' in include path '~ann/r.sp' on this platform");

  Reading none;
  none.options.home_directory = nullptr;
  CHECK_FALSE(none.read(".include ~/r.sp\n").ok());
}

TEST_CASE("an unreadable include is skipped", "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  Reading r;
  REQUIRE(r.read(".include missing.sp\nR1 a b 1k\n").ok());
  CHECK(r.texts(codes::kIncludeUnreadable) == Names{"Cannot open include file 'missing.sp'."});
  CHECK(r.cell("in.spice").instances.size() == 1);
}

TEST_CASE("global nets", "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  Reading r;
  REQUIRE(r.read(".subckt c a\nR1 a vdd 1k\n.global vdd\n.ends\n.global gnd\n").ok());
  const auto& c = r.cell("c");
  CHECK(c.net(c.find_net("vdd")).global);  // the existing net is converted
  const auto& file = r.cell("in.spice");
  CHECK(file.flags.top);
  CHECK(file.net(file.find_net("gnd")).global);
}

TEST_CASE(".PARAM values are recorded per file", "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  Reading r;
  REQUIRE(r.read(".param wmin=0.5u k='2*3'\nR1 a b 1k\n").ok());
  const auto& table = r.db.parameters.at(0);
  CHECK(table.at("WMIN") == "0.5u");
  CHECK(table.at("K") == "'2*3'");
}

// ---------------------------------------------------------------- library

TEST_CASE("library reads define black boxes", "[cap:lvs-netlist-input][req:Library and black-box reads]") {
  Reading r;
  r.options.library = true;
  const auto top = r.read(".subckt ram a b w=1\nR1 a b 1k\n.subckt inner x\n.ends\n.ends\nR9 a b 1k\nX1 a b ram\n",
                          "lib.spice");
  REQUIRE(top.ok());
  const auto& ram = r.cell("ram");
  CHECK(ram.device_class == lvs::DeviceClass::module);
  CHECK(port_names(ram) == Names{"a", "b"});
  CHECK(ram.properties.size() == 1);
  CHECK(ram.instances.empty());
  CHECK_FALSE(r.has_cell("inner"));
  CHECK_FALSE(r.has_cell("r"));
  const auto& file = r.cell("lib.spice");
  CHECK(file.flags.top);
  CHECK(file.instances.empty());
}

TEST_CASE("a library-only file still registers its file cell",
          "[cap:lvs-netlist-input][req:Library and black-box reads]") {
  Reading r;
  const auto top = r.read(".subckt inv a y\n.ends\n", "cells.spice");
  REQUIRE(top.ok());
  CHECK(r.db.cells.at(top.value()).name == "inv");
  CHECK(r.cell("cells.spice").flags.top);
}

TEST_CASE("empty subcircuit as black box", "[cap:lvs-netlist-input][req:Library and black-box reads]") {
  Reading on;
  on.options.auto_blackbox = true;
  REQUIRE(on.read(".subckt ip a b\n.ends\n.subckt full a\nR1 a b 1k\n.ends\n").ok());
  CHECK(on.cell("ip").device_class == lvs::DeviceClass::module);
  CHECK(on.cell("full").device_class == lvs::DeviceClass::subcircuit);
  Reading off;
  REQUIRE(off.read(".subckt ip a b\n.ends\n").ok());
  CHECK(off.cell("ip").device_class == lvs::DeviceClass::subcircuit);
}

// ---------------------------------------------------------------- in memory

TEST_CASE("an in-memory source reads as the same file", "[cap:lvs-netlist-input][req:In-memory netlist sources]") {
  const std::string text =
      "* inv\n.include cells.sp\n.subckt top a y\nXinv a y inv\nZ9 bad card\n.ends\n.subckt top a y\n.ends\n";
  const std::string cells = ".subckt inv a y\nM1 y a vss vss nfet W=1\n.ends\n";
  Reading from_file;
  from_file.add_file("/doc/inv.spice", text);
  from_file.add_file("/doc/cells.sp", cells);
  REQUIRE(from_file.read_file("/doc/inv.spice").ok());
  Reading in_memory;
  in_memory.add_file("/doc/cells.sp", cells);
  REQUIRE(in_memory.read(text, "/doc/inv.spice").ok());
  CHECK(lvs_test::dump(from_file.db) == lvs_test::dump(in_memory.db));
  CHECK(from_file.sink.snapshot() == in_memory.sink.snapshot());
  CHECK_FALSE(in_memory.sink.snapshot().empty());
}

TEST_CASE("a path source needs the file service", "[cap:lvs-netlist-input][req:In-memory netlist sources]") {
  Reading r;
  r.options.files = nullptr;
  const auto read = r.read_file("/doc/a.spice");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kPathUnreadable);
  CHECK(r.read("R1 a b 1k\n", "/doc/b.spice").ok());  // in memory: no file service needed
}

// ---------------------------------------------------------------- limits

TEST_CASE("include cycle", "[cap:lvs-netlist-input][req:Input limits]") {
  // Regression: an include cycle recursed until the stack overflowed.
  Reading r;
  r.add_file("/w/a.spice", ".include b.spice\n");
  r.add_file("/w/b.spice", "* b\n.include a.spice\n");
  const auto read = r.read_file("/w/a.spice");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kIncludeDepth);
  CHECK(read.error().text == "include depth limit of 64 exceeded in /w/b.spice line 2");
  CHECK(read.error().location == hefesto::diag::Location{hefesto::diag::SourceLocation{"/w/b.spice", 2}});
}

TEST_CASE("a lowered include depth", "[cap:lvs-netlist-input][req:Input limits]") {
  Reading r;
  r.options.limits.include_depth = 2;
  r.add_file("/w/a.sp", ".include b.sp\n");
  r.add_file("/w/b.sp", ".include c.sp\n");
  r.add_file("/w/c.sp", "R1 a b 1k\n");
  const auto read = r.read_file("/w/a.sp");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().text == "include depth limit of 2 exceeded in /w/b.sp line 1");
  r.options.limits.include_depth = 3;
  CHECK(r.read_file("/w/a.sp").ok());
}

TEST_CASE("a multiplying include tree stops", "[cap:lvs-netlist-input][req:Input limits]") {
  // Regression: each file including the next one twice read 2^depth files
  // below the depth limit, which took hours.
  Reading r;
  for (int i = 0; i < 20; ++i) {
    const std::string next = "f" + std::to_string(i + 1) + ".sp";
    r.add_file("/w/f" + std::to_string(i) + ".sp", ".include " + next + "\n.include " + next + "\n");
  }
  r.add_file("/w/f20.sp", "* leaf\n");
  const auto read = r.read_file("/w/f0.sp");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kIncludeCount);
}

TEST_CASE("a failed read leaves nothing that a later read returns", "[cap:lvs-netlist-input][req:Format selection]") {
  // Regression (M9 R1 review): the cells of a failed read stayed in the
  // database, so reading the file again returned its partial top cell.
  Reading r;
  r.options.limits.include_depth = 1;
  r.add_file("/w/top.sp", "R1 a b 1k\n.include leaf.sp\n");
  r.add_file("/w/leaf.sp", "R2 b c 2k\n");
  REQUIRE_FALSE(r.read_file("/w/top.sp").ok());
  CHECK(r.db.cells.empty());
  CHECK(r.db.next_file == 0);
  CHECK(r.db.parameters.empty());
  r.options.limits.include_depth = 2;
  const auto again = r.read_file("/w/top.sp");
  REQUIRE(again.ok());
  CHECK(r.db.cells.at(again.value()).instances.size() == 2);
  CHECK(r.db.cells.at(again.value()).file == 0);
}

TEST_CASE("a large file that includes itself twice stops on the include bytes",
          "[cap:lvs-netlist-input][req:Input limits]") {
  // Regression (M9 R1 review): the include count alone let a large file
  // included many times below it lex terabytes. With a 16 KiB logical-line
  // limit the include bytes stop at 64 x 16 KiB = 1 MiB.
  Reading r;
  r.options.limits.logical_line = std::size_t{16} << 10;
  std::string text = ".include self.sp\n.include self.sp\n";
  while (text.size() < (std::size_t{256} << 10)) {
    text += "* a comment line that only adds bytes to the file\n";
  }
  r.add_file("/w/self.sp", text);
  const auto read = r.read_file("/w/self.sp");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kIncludeCount);
  CHECK(read.error().text.starts_with("more than 1048576 bytes of include files in one netlist at /w/self.sp"));
}

TEST_CASE("logical line limit", "[cap:lvs-netlist-input][req:Input limits]") {
  // Regression: unbounded continuation lines grew one line without limit.
  Reading r;
  r.options.limits.logical_line = 64;
  std::string text = "R1 a b 1k\nX1";
  for (int i = 0; i < 40; ++i) {
    text += "\n+ n" + std::to_string(i);
  }
  const auto read = r.read(text + " sub\n");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kLineLength);
  CHECK(read.error().text == "logical line exceeds the line limit of 64 bytes in in.spice line 2");

  Reading huge;
  const auto big = huge.read("R1 a b " + std::string((std::size_t{16} << 20) + 1, '1') + "\n");
  REQUIRE_FALSE(big.ok());
  CHECK(big.error().code == codes::kLineLength);
}

TEST_CASE("name length limit", "[cap:lvs-netlist-input][req:Input limits]") {
  Reading r;
  const auto read = r.read("R1 a " + std::string(4097, 'n') + " 1k\n");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kNameLength);
  CHECK(read.error().text == "name of 4097 bytes exceeds the name-length limit of 4096 bytes in in.spice line 1");
  Reading ok;
  CHECK(ok.read("R1 a " + std::string(4096, 'n') + " 1k\n").ok());
}

TEST_CASE("devices per run", "[cap:lvs-netlist-input][req:Input limits]") {
  Reading r;
  r.options.limits.devices = 2;
  REQUIRE(r.read("R1 a b 1k\nR2 a b 1k\n", "one.spice").ok());
  const auto read = r.read("X3 a b c\n", "two.spice");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == hefesto::lvs::net_codes::kDeviceLimit);
  CHECK(read.error().text == "the device limit of 2 per netlist database is exceeded in two.spice line 1");
}

TEST_CASE("limits can be lowered but not raised past twice the default", "[cap:lvs-netlist-input][req:Input limits]") {
  lvs::ReadLimits limits;
  CHECK(lvs::validate_limits(limits).ok());
  limits.include_depth = 128;
  limits.logical_line = std::size_t{32} << 20;
  limits.name_length = 8192;
  limits.devices = 2 * lvs::kMaxDevicesPerRun;
  CHECK(lvs::validate_limits(limits).ok());
  for (int which = 0; which < 5; ++which) {
    lvs::ReadLimits bad;
    bad.include_depth = which == 0 ? 129 : bad.include_depth;
    bad.logical_line = which == 1 ? (std::size_t{32} << 20) + 1 : bad.logical_line;
    bad.name_length = which == 2 ? 0 : bad.name_length;
    bad.devices = which == 3 ? 2 * lvs::kMaxDevicesPerRun + 1 : bad.devices;
    bad.include_depth = which == 4 ? 0 : bad.include_depth;
    const auto status = lvs::validate_limits(bad);
    REQUIRE_FALSE(status.ok());
    CHECK(status.error().code == codes::kBadLimit);
  }
  Reading r;
  r.options.limits.include_depth = 1000;
  const auto read = r.read("R1 a b 1k\n");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kBadLimit);
}

TEST_CASE("hostile inputs end without crash or hang", "[cap:lvs-netlist-input][req:Input limits]") {
  // Regressions: an unterminated continuation or quote, deep nesting and
  // long runs of comment and continuation lines.
  Reading a;
  CHECK(a.read("R1 a b 1k \\").ok());
  Reading b;
  CHECK(b.read("R1 a b '1k\n+ \"open").ok());
  Reading c;
  std::string nested;
  for (int i = 0; i < 20000; ++i) {
    nested += ".subckt s" + std::to_string(i) + " a\n";
  }
  CHECK(c.read(nested).ok());
  CHECK(c.texts(codes::kMissingEnds).size() == 100);  // the rest are counted, not reported
  Reading d;
  std::string comments = "R1 a b 1k\n";
  for (int i = 0; i < 100000; ++i) {
    comments += "* c\n";
  }
  CHECK(d.read(comments + "+ m=2\n").ok());
  CHECK(property(instance(d.cell("in.spice"), "r:1"), "m") == "2");
  Reading e;
  CHECK(e.read(std::string("\0\0\0", 3)).ok());
  Reading f;
  CHECK(f.read("+\n+\n*\n\\\n.subckt\n.ends\nX\nM\n.include\n.model\n.global\n").ok());
}

TEST_CASE("case-sensitive reads keep SPICE case-sensitive",
          "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  Reading insensitive;
  REQUIRE(insensitive.read("R1 Vdd b 1k\nR2 VDD b 1k\n").ok());
  const auto& top = insensitive.cell("in.spice");
  CHECK(top.flags.case_insensitive);
  CHECK(top.instances[0].connections == top.instances[1].connections);

  Reading sensitive;
  sensitive.db.case_sensitive_read = true;  // e.g. a Verilog file was read before
  REQUIRE(sensitive.read("R1 Vdd b 1k\nR2 VDD b 1k\n").ok());
  const auto& cell = sensitive.cell("in.spice");
  CHECK_FALSE(cell.flags.case_insensitive);
  CHECK(cell.instances[0].connections != cell.instances[1].connections);
  CHECK(sensitive.texts(codes::kCaseSensitive) ==
        Names{"Warning:  A case-sensitive file has been read and so the SPICE netlist must be treated "
              "case-sensitive to match."});
}

TEST_CASE("bus delimiters name one net in a SPICE cell",
          "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  Reading r;
  REQUIRE(r.read("R1 data[3] b 1k\nR2 DATA<3> b 1k\n").ok());
  const auto& top = r.cell("in.spice");
  CHECK(top.instances[0].connections == top.instances[1].connections);
}
