// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The flat SPICE netlist over hand-built inputs (capability
// netlist-spice-options "File header and scale card", "Node names per
// dialect", "Flat netlist structure", "Subcircuit header line", "Parasitic
// and node lines"). Each input mirrors an NL-SPICE case's `.ext` file in
// the visit order Magic used, and the output is compared with the case's
// committed netlist; the full replay through EXTFLAT follows integration.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "spice_fixture.hpp"

using namespace spice_test;
using spice::DeviceClass;
using spice::Dialect;

namespace {

// NL-SPICE-02/05 inv.ext.
spice::SpiceInput inverter() {
  spice::SpiceInput in = nltest_input("inv");
  in.nodes = {node("in", 0.4F, {{0, 0}, {24, 20}, {0, 0}}), node("out", 5.0F, {{40, 26}, {0, 0}, {40, 28}}),
              node("vdd!", 1.2F, {{40, 26}, {0, 0}, {80, 48}}), node("gnd!", 1.2F, {{20, 18}, {0, 0}, {80, 48}})};
  in.nodes[0].port = in.nodes[1].port = true;
  in.ports = {{"in", 0}, {"out", 1}};
  in.devices = {mos("nfet", 2, 4, "gnd!", {term("in", 8), term("gnd!", 4), term("out", 4)}),
                mos("pfet", 2, 8, "vdd!", {term("in", 16), term("vdd!", 8), term("out", 8)})};
  return in;
}

}  // namespace

TEST_CASE("Inverter netlists in every dialect match NL-SPICE-02",
          "[cap:netlist-spice-options][req:File header and scale card]") {
  const spice::SpiceInput in = inverter();
  CHECK(write(in, with_format(Dialect::spice3)).text == golden("NL-SPICE-02/expected/collected/inv.spice3.spice"));
  CHECK(write(in, with_format(Dialect::ngspice)).text == golden("NL-SPICE-02/expected/collected/inv.ngspice.spice"));
  CHECK(write(in, with_format(Dialect::hspice)).text == golden("NL-SPICE-02/expected/collected/inv.hspice.spice"));
  CHECK(write(in, with_format(Dialect::cdl)).text == golden("NL-SPICE-02/expected/collected/inv.cdl"));
  CHECK(write(in, with_format(Dialect::spice3)).text == golden("NL-SPICE-02/expected/collected/custom.spice"));
}

TEST_CASE("SPICE2 numbers nodes from 10 in reference order", "[cap:netlist-spice-options][req:Node names per dialect]") {
  CHECK(write(inverter(), with_format(Dialect::spice2)).text ==
        golden("NL-SPICE-02/expected/collected/inv.spice2.spice"));
}

TEST_CASE("The default header has the scale card", "[cap:netlist-spice-options][req:File header and scale card]") {
  spice::SpiceInput in = nltest_input("inv");
  in.tech = "scmos";
  in.scale = 5.0F;
  CHECK(write(in).text == "* SPICE3 file created from inv.ext - technology: scmos\n\n.option scale=50n\n\n");
  spice::SpiceSettings off;
  off.scale = false;
  CHECK(write(in, off).text == "* SPICE3 file created from inv.ext - technology: scmos\n\n");
}

TEST_CASE("A style that does not match the file warns", "[cap:netlist-spice-options][req:File header and scale card]") {
  spice::SpiceInput in = inverter();
  in.style_matches = false;
  const auto out = write(in);
  REQUIRE(out.messages.size() >= 2);
  CHECK(out.messages[0].text == "Warning:  Current extraction style does not match .ext file!");
  CHECK(out.messages[1].text == "Area/Perimeter values and parasitic values will be zero.");
  CHECK(out.text.find("ad=0 pd=0 as=0 ps=0") != std::string::npos);
}

TEST_CASE("HSPICE names map prefixes and write the dictionary",
          "[cap:netlist-spice-options][req:Node names per dialect]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("top/u1/a", 3.0F), node("top/u1/b", 3.0F), node("averyveryverylongname", 3.0F)};
  const std::string text = write(in, with_format(Dialect::hspice)).text;
  CHECK(text.find("C0 x0/a 0 3f **FLOATING\nC1 x0/b 0 3f **FLOATING\n") != std::string::npos);
  CHECK(text.find("** averyveryverylongname == z@10\nC2 z@10 0 3f **FLOATING\n") != std::string::npos);
  CHECK(text.ends_with("\n** hspice subcircuit dictionary\n* x0\ttop/u1\n"));
}

TEST_CASE("Unresolved terminals are written as errGnd!", "[cap:netlist-spice-options][req:Node names per dialect]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("g")};
  in.devices = {mos("nfet", 2, 4, "GND!", {term("g"), term("nowhere"), term("g")})};
  CHECK(write(in).text.find("M1000 g g errGnd! GND nfet") != std::string::npos);
}

TEST_CASE("The flat inverter writes devices before node lines without a wrapper",
          "[cap:netlist-spice-options][req:Flat netlist structure]") {
  spice::SpiceSettings auto_top;
  CHECK(write(inverter(), auto_top).text == golden("NL-SPICE-05/expected/collected/inv.topauto.spice"));
  spice::SpiceSettings off;
  off.subckt_top = spice::TopSubckt::off;
  CHECK(write(inverter(), off).text == golden("NL-SPICE-05/expected/collected/inv.topoff.spice"));
  spice::SpiceSettings on;
  on.subckt_top = spice::TopSubckt::on;
  CHECK(write(inverter(), on).text == golden("NL-SPICE-05/expected/collected/inv.topon.spice"));
}

TEST_CASE("Flat top auto with ports and magic-compat ON writes no wrapper",
          "[cap:netlist-spice-options][req:Flat netlist structure]") {
  const std::string text = write(inverter(), {}, true).text;
  CHECK(text.find(".subckt") == std::string::npos);
  CHECK(text.find(".ends") == std::string::npos);
}

TEST_CASE("Flat top auto with ports and magic-compat OFF wraps the root",
          "[cap:netlist-spice-options][req:Flat netlist structure]") {
  // Q-NETLIST-37 OFF: top auto wraps a root with port records.
  const std::string text = write(inverter(), {}, false).text;
  CHECK(text.find(".option scale=10n\n\n.subckt inv in out\nM1000 out in gnd gnd nfet") != std::string::npos);
  CHECK(text.ends_with("C0 out 0 5f\n.ends\n"));
}

TEST_CASE("The subcircuit header renames a numeric cell and wraps at 80 columns",
          "[cap:netlist-spice-options][req:Subcircuit header line]") {
  spice::SpiceInput in = nltest_input("7seg");
  for (int i = 0; i < 30; ++i) {
    const std::string name = std::string{"long_port_name_number_"} + (i < 10 ? "0" : "") + std::to_string(i);
    in.nodes.push_back(node(name));
    in.nodes.back().port = true;
    in.ports.push_back({name, i});
  }
  // `parameters :7seg p=1` is looked up under the renamed cell, so nothing
  // is added, as in Magic.
  in.params[":7seg"] = {{"p", "p=1"}};
  in.devices = {mos("nfet", 2, 4, "GND!",
                    {term("long_port_name_number_00", 8), term("long_port_name_number_01", 4),
                     term("long_port_name_number_02", 4)})};
  spice::SpiceSettings on;
  on.subckt_top = spice::TopSubckt::on;
  CHECK(write(in, on).text == golden("NL-SPICE-06/expected/collected/7seg.spice"));
  in.params[":x7seg"] = {{"p", "p=1"}};
  CHECK(write(in, on).text.find("long_port_name_number_29 p=1\n") != std::string::npos);
}

TEST_CASE("Implicit and duplicate ports are numbered as Magic does",
          "[cap:netlist-spice-options][req:Subcircuit header line]") {
  spice::SpiceInput in = nltest_input("cell");
  in.nodes = {node("a"), node("b"), node("c"), node("d"), node("x")};
  in.ports = {{"a", -1}, {"b", 0}, {"c", -1}, {"d", 3}};
  // A node `x` with the alias `y`: both table entries name it, once.
  in.nodes.back().aliases.push_back("y");
  in.ports.push_back(spice::RootPort{"x", 1, {{"y", -1}}, std::nullopt});
  in.ports.push_back(spice::RootPort{"y", -1, {}, extflat::PortName{"x", 1}});
  spice::SpiceSettings on;
  on.subckt_top = spice::TopSubckt::on;
  // Implicit ports take 4 and 5 in reverse table order (c then a); `x` is
  // written once although two entries name it.
  CHECK(write(in, on).text.find(".subckt cell b x d c a\n") != std::string::npos);
}

TEST_CASE("Subcircuit calls use the instance path or a number", "[cap:netlist-spice-options][req:Subcircuit instance lines]") {
  int counter = 0;
  extflat::NameFlags names;
  const spice::InstanceCall call{{"u1"}, {"n1", "n2"}, "inv", {}};
  CHECK(spice::instance_line(call, Dialect::spice3, false, counter, names, kMagicNames) == "Xu1 n1 n2 inv\n");
  CHECK(spice::instance_line(call, Dialect::cdl, false, counter, names, kMagicNames) == "Xu1 n1 n2 / inv\n");
  CHECK(counter == 0);
  CHECK(spice::instance_line(call, Dialect::spice3, true, counter, names, kMagicNames) == "X0 n1 n2 inv\n");
  CHECK(spice::instance_line(call, Dialect::spice2, false, counter, names, kMagicNames) == "X1 n1 n2 inv\n");
  // Only commas are converted in the instance name; `!` is kept.
  const spice::InstanceCall odd{{"top", "u,1!"}, {"n1"}, "7seg", {"w=2"}};
  CHECK(spice::instance_line(odd, Dialect::spice3, false, counter, names, kMagicNames) ==
        "Xtop/u|1! n1 x7seg w=2\n");
  std::vector<std::string> many(20, std::string(9, 'n'));
  const spice::InstanceCall wide{{"u2"}, many, "cell", {}};
  const std::string line = spice::instance_line(wide, Dialect::spice3, false, counter, names, kMagicNames);
  CHECK(line.find("\n+ nnnnnnnnn") != std::string::npos);
}

TEST_CASE("The global card lists the style's substrate names", "[cap:netlist-spice-options][req:Global card in hierarchical mode]") {
  extflat::NameFlags names;
  names.trim_global = true;
  const std::vector<hefesto::extract::DeviceInfo> two{{"a", 0, -1, -1, -1, "VSUBS!"}, {"b", 0, -1, -1, -1, "VDD!"}};
  CHECK(spice::global_card(two, Dialect::spice3, {}, names) == ".global VDD VSUBS \n\n");
  // NL-SPICE-08: an `err` name is excluded; CDL keeps `!` and adds the
  // bang-terminated power names before the rest.
  const std::vector<hefesto::extract::DeviceInfo> three{
      {"a", 0, -1, -1, -1, "VSUBS!"}, {"b", 0, -1, -1, -1, "VDD!"}, {"c", 0, -1, -1, -1, "errsub"}};
  CHECK(spice::global_card(three, Dialect::spice3, {}, names) == ".global VDD VSUBS \n\n");
  extflat::NameFlags cdl;
  const spice::PowerNames power{"VSUBS", "vdd!", "gnd!"};
  CHECK(spice::global_card(three, Dialect::cdl, power, cdl) == ".global gnd! vdd! VDD! VSUBS! \n\n");
  const std::vector<hefesto::extract::DeviceInfo> one{{"a", 0, -1, -1, -1, "VSUBS"}};
  CHECK(spice::global_card(one, Dialect::cdl, power, cdl) == ".global gnd! vdd! VSUBS \n\n");
  const std::vector<hefesto::extract::DeviceInfo> none{{"a", 0, -1, -1, -1, "None"}, {"b", 0, -1, -1, -1, ""}};
  CHECK(spice::global_card(none, Dialect::spice3, {}, names).empty());
}

namespace {

// NL-SPICE-07 top.ext.
spice::SpiceInput floating() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("fl", 3.0F), node("out", 5.0F), node("g", 0.5F)};
  in.nodes[1].attributes = {"cap=10f"};
  in.devices = {mos("nfet", 2, 4, "GND!", {term("g", 8), term("out", 4), term("g", 4)})};
  return in;
}

}  // namespace

TEST_CASE("Node lines mark floating nodes and attributes as Magic does (NL-SPICE-07)",
          "[cap:netlist-spice-options][req:Parasitic and node lines]") {
  CHECK(write(floating()).text == golden("NL-SPICE-07/expected/collected/top.spice3.spice"));
  CHECK(write(floating(), with_format(Dialect::ngspice)).text ==
        golden("NL-SPICE-07/expected/collected/top.ngspice.spice"));
  spice::RunFlags dash_f;
  dash_f.device_nodes_only = true;
  CHECK(write(floating(), {}, true, dash_f).text == golden("NL-SPICE-07/expected/collected/top.dashF.spice"));
  spice::SpiceSettings dash_b;
  dash_b.no_attributes = true;
  CHECK(write(floating(), dash_b).text == golden("NL-SPICE-07/expected/collected/top.dashB.spice"));
}

TEST_CASE("Floating marks follow the documented rule with magic-compat OFF",
          "[cap:netlist-spice-options][req:Parasitic and node lines]") {
  // Magic's flat test marks every device node FLOATING (its used bit is
  // set for device nodes); with magic-compat OFF only nodes that touch no
  // device or resistor are floating, and -F keeps device nodes.
  const std::string text = write(floating(), {}, false).text;
  CHECK(text.find("C0 fl 0 3f **FLOATING\nC1 out 0 5f\n") != std::string::npos);
  spice::RunFlags dash_f;
  dash_f.device_nodes_only = true;
  const std::string only = write(floating(), {}, false, dash_f).text;
  CHECK(only.find("C0 out 0 5f\n**nodeattr out : cap=10f\n") != std::string::npos);
  CHECK(only.find("fl") == std::string::npos);
}

TEST_CASE("Node capacitance threshold is strict and uses the substrate node",
          "[cap:netlist-spice-options][req:Parasitic and node lines]") {
  spice::SpiceInput in = nltest_input("top");
  in.compat_substrate = false;
  in.nodes = {node("n2", 2.0F), node("n7", 3.0F), node("sub", 9.0F)};
  in.nodes[2].substrate = true;
  const std::string text = write(in, with_format(Dialect::ngspice)).text;
  CHECK(text.find("n2") == std::string::npos);  // 2 is not greater than 2
  CHECK(text.find("C0 n7 sub 3f ; **FLOATING\n") != std::string::npos);
  CHECK(text.find("C1 sub") == std::string::npos);  // the substrate's own cap is zero
}

TEST_CASE("Coupling capacitors and resistors follow the device lines",
          "[cap:netlist-spice-options][req:Parasitic and node lines]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("a"), node("b"), node("c", 4.0F)};
  in.caps = {{"a", "b", 2500.0}, {"b", "c", 1500.0}, {"a", "c", 2000.0}};
  in.resists = {{"a", "c", 25500.0}};
  const std::string text = write(in).text;
  // `c` touches only a resistor: Magic's flat test marks it FLOATING.
  CHECK(text.find("C0 a b 2.5f\nR0 a c 25.5\nC1 c 0 4f **FLOATING\n") != std::string::npos);
  spice::SpiceSettings no_caps;
  no_caps.cthresh = std::numeric_limits<double>::infinity();
  CHECK(write(in, no_caps).text.find("C0") == std::string::npos);
}

TEST_CASE("SPICE2 names coupling and resistor nodes right to left",
          "[cap:netlist-spice-options][req:Node names per dialect]") {
  // Magic's GCC build evaluates the two nodeSpiceName() arguments of the
  // line from the right, so the second node is numbered first.
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("a"), node("b")};
  in.resists = {{"a", "b", 1000.0}};
  CHECK(write(in, with_format(Dialect::spice2)).text.find("R0 11 10 1\n") != std::string::npos);
}
