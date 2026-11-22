// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device lines of the flat SPICE netlist (capability netlist-spice-devices).
// The inputs mirror the NL-SPICE cases' `.ext` files in Magic's visit order
// and the outputs are the cases' committed netlists; the Q-row tests show
// each mode where the decided behaviour differs.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "spice_fixture.hpp"

using namespace spice_test;
using spice::DeviceClass;
using spice::Dialect;

namespace {

spice::InputDevice devcap(const std::string& top, float ff, int x) {
  spice::InputDevice d = device(DeviceClass::cap, "None", {term(top), term("gnd!")});
  d.capacitance = ff;
  d.rect = {x, 0, x + 1, 1};
  return d;
}

spice::InputDevice nfet_w(std::int64_t w) {
  return mos("nfet", 2, w, "GND!", {term("g", 2 * w), term("s", w), term("d", w)});
}

// NL-SPICE-03 top.ext, in Magic's visit order.
spice::SpiceInput si_cases() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("g"), node("s"), node("d"), node("gnd!"), node("GND!")};
  for (int i = 0; i < 10; ++i) {
    in.nodes.push_back(node("c" + std::to_string(i)));
  }
  in.devices = {devcap("c1", 0.0001F, 10), devcap("c7", 9.999e17F, 70), devcap("c4", 100010.0F, 40),
                nfet_w(2),
                nfet_w(1),
                devcap("c9", 1e24F, 90),
                devcap("c6", 1e13F, 60),
                nfet_w(200),
                nfet_w(20000),
                nfet_w(20),
                devcap("c8", 1e18F, 80),
                nfet_w(2000),
                devcap("c3", 100.0F, 30),
                devcap("c0", 0.0F, 0),
                devcap("c5", 1e8F, 50),
                devcap("c2", 99.99F, 20)};
  return in;
}

// NL-SPICE-09 top.ext.
spice::SpiceInput shared_diffusion() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("x", 0, {{30, 16}}), node("a", 0, {{40, 26}}), node("b", 0, {{40, 26}}), node("g")};
  in.devices = {mos("nfet", 2, 8, "GND!", {term("g", 16), term("a", 8), term("x", 8)}),
                mos("nfet", 2, 8, "GND!", {term("g", 16), term("a", 8, "12,14"), term("b", 8)}),
                mos("nfet", 2, 8, "GND!", {term("g", 16), term("b", 8), term("x", 8)}),
                mos("nfet", 2, 8, "GND!", {term("g", 16, "ext:aps"), term("a", 8), term("b", 8)})};
  in.type_names = {"nfet"};
  return in;
}

// NL-SPICE-10 top.ext.
spice::SpiceInput resistors() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("rb", 4.0F), node("ra", 1.0F), node("rc", 1.0F), node("sub1", 1.0F)};
  spice::InputDevice rppd = device(DeviceClass::res, "rppd", {term("rb"), term("ra", 2), term("rc", 2)}, "None", 10, 2);
  spice::InputDevice x0 = device(DeviceClass::rsubckt, "rres", {term("rb"), term("ra"), term("rc")}, "sub1");
  spice::InputDevice none = device(DeviceClass::res, "None", {term("rb"), term("ra"), term("rc")});
  none.resistance = 25.5F;
  spice::InputDevice x1 = device(DeviceClass::rsubckt, "rres", {term("rb"), term("ra"), term("rc")});
  in.devices = {rppd, x0, none, x1};
  return in;
}

// NL-SPICE-11 top.ext: `parameters rppd r= w=w l=l s=sub` and
// `parameters mimcapm c= s=sub`, stored in reverse (Q-NETLIST-9).
spice::SpiceInput cdl_values() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("ra", 1), node("rb", 1), node("rc", 1), node("ct", 1), node("cb", 1), node("GND!")};
  in.params["rppd"] = {{"s", "sub"}, {"l", "l"}, {"w", "w"}, {"r", ""}};
  in.params["mimcapm"] = {{"s", "sub"}, {"c", ""}};
  spice::InputDevice cap = device(DeviceClass::cap, "mimcapm", {term("ct"), term("cb")}, "GND!", 10, 10);
  spice::InputDevice res = device(DeviceClass::res, "rppd", {term("rb"), term("ra", 2), term("rc", 2)}, "GND!", 10, 2);
  in.devices = {cap, res};
  return in;
}

// NL-SPICE-12 top.ext, in Magic's visit order.
spice::SpiceInput parallel() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("g"), node("s", 0, {{40, 26}}), node("d", 0, {{30, 16}}), node("ct"), node("cb")};
  const auto cap = [](float ff) {
    spice::InputDevice d = device(DeviceClass::cap, "None", {term("ct"), term("cb")});
    d.capacitance = ff;
    return d;
  };
  in.devices = {mos("nfet", 2, 8, "GND!", {term("g", 16), term("s", 8), term("d", 8)}),
                mos("nfet", 2, 8, "GND!", {term("g", 16), term("d", 8), term("s", 8)}),
                cap(10),
                mos("nfet", 2, 8, "GND!", {term("g", 16), term("s", 8), term("d", 8)}),
                cap(5),
                mos("nfet", 2, 4, "GND!", {term("g", 8), term("s", 4), term("d", 4)}),
                cap(10)};
  return in;
}

// NL-SPICE-13: leaf.ext inside use u1 of top.ext, flattened.
spice::SpiceInput instance_devices() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("x", 1), node("u1/b", 1), node("u1/g", 1)};
  in.nodes[0].aliases.push_back("u1/a");
  const auto leaf = [](std::vector<spice::InputTerminal> terms) {
    spice::InputDevice d = mos("nfet", 2, 4, "GND!", std::move(terms));
    d.prefix = {"u1"};
    return d;
  };
  in.devices = {leaf({term("u1/g", 8, "foo"), term("u1/a", 4), term("u1/b", 4)}),
                leaf({term("u1/g", 8), term("u1/a", 4)}),
                leaf({term("u1/g", 8), term("u1/a", 4, "D"), term("u1/b", 4)})};
  return in;
}

spice::SpiceSettings scale_off() {
  spice::SpiceSettings s;
  s.scale = false;
  return s;
}

}  // namespace

TEST_CASE("Device names count per class from 0 and MOSFETs from 1000",
          "[cap:netlist-spice-devices][req:Device instance names]") {
  CHECK(write(si_cases(), scale_off()).text == golden("NL-SPICE-03/expected/collected/top.spice"));
}

TEST_CASE("Flat gate attribute names join the path without a separator with magic-compat ON",
          "[cap:netlist-spice-devices][req:Device instance names]") {
  CHECK(write(instance_devices()).text == golden("NL-SPICE-13/expected/collected/top.flat.spice"));
}

TEST_CASE("Flat gate attribute names join the path with a slash with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Device instance names]") {
  const std::string text = write(instance_devices(), {}, false).text;
  CHECK(text.find("\nMu1/foo u1/b u1/g x GND nfet w=4 l=2\n") != std::string::npos);
  // A top-level device has no path, so both modes agree.
  CHECK(write(shared_diffusion(), {}, false).text.find("\nMext:aps b g a GND") != std::string::npos);
}

TEST_CASE("Tied, swapped and one-terminal devices are annotated or skipped",
          "[cap:netlist-spice-devices][req:Device eligibility and annotations]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("g"), node("x")};
  in.devices = {mos("nfet", 2, 4, "GND!", {term("g"), term("x")}), mos("nfet", 2, 4, "GND!", {term("g")}),
                device(DeviceClass::diode, "dn", {term("g")}), device(DeviceClass::subckt, "blk", {term("g")})};
  const auto out = write(in, with_format(Dialect::ngspice));
  CHECK(out.text.find("; ** SOURCE/DRAIN TIED\nM1000 x g x GND nfet w=4 l=2\n") != std::string::npos);
  CHECK(out.text.find("M1001") == std::string::npos);
  CHECK(out.text.find("\nX0 g blk\n") != std::string::npos);
  REQUIRE(out.messages.size() == 2);
  CHECK(out.messages[0].text == "Device other than subcircuit has only one terminal");
  CHECK(out.messages[1].text == "Diode has only one terminal");
}

TEST_CASE("MOSFET lines share diffusion areas once (NL-SPICE-09)", "[cap:netlist-spice-devices][req:MOSFET lines]") {
  const auto on = write(shared_diffusion());
  CHECK(on.text == golden("NL-SPICE-09/expected/collected/top.scaleon.spice"));
  REQUIRE_FALSE(on.messages.empty());
  CHECK(on.messages.back().text == "error: subap for devtype 0 unspecified");
  CHECK(write(shared_diffusion(), scale_off()).text == golden("NL-SPICE-09/expected/collected/top.scaleoff.spice"));
}

TEST_CASE("Scale off writes SI geometry", "[cap:netlist-spice-devices][req:MOSFET lines]") {
  spice::SpiceInput in = nltest_input("top");
  in.scale = 5.0F;  // E = 0.05 um
  in.nodes = {node("out", 0, {{40, 26}}), node("in"), node("GND")};
  in.devices = {mos("nfet", 2, 8, "GND", {term("in"), term("GND"), term("out")})};
  const std::string text = write(in, scale_off()).text;
  CHECK(text.find("M1000 out in GND GND nfet w=0.4u l=100n\n+  ad=0.1p pd=1.3u as=") != std::string::npos);
}

TEST_CASE("Terminal areas are counted once per node and class", "[cap:netlist-spice-devices][req:Terminal area and perimeter]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("x", 0, {{30, 16}}), node("g"), node("s1"), node("s2")};
  in.devices = {mos("nfet", 2, 4, "GND!", {term("g"), term("s1"), term("x")}),
                mos("nfet", 2, 4, "GND!", {term("g"), term("s2"), term("x", 0, "ext:apf")})};
  const std::string text = write(in).text;
  CHECK(text.find("+  ad=30p pd=16u as=0 ps=0\n") != std::string::npos);
  CHECK(text.find("+  ad=0 pd=0 as=0 ps=0\n") != std::string::npos);
}

TEST_CASE("Merged areas outside scale mode are not divided with magic-compat ON",
          "[cap:netlist-spice-devices][req:Terminal area and perimeter]") {
  spice::SpiceInput in = parallel();
  in.devices.resize(2);
  spice::SpiceSettings s = scale_off();
  s.merge = spice::MergeMode::conservative;
  in.scale = 100.0F;  // E = 1
  CHECK(write(in, s).text.find("M=2\n+  ad=30p pd=16u as=40p ps=26u\n") != std::string::npos);
}

TEST_CASE("Merged areas outside scale mode are divided with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Terminal area and perimeter]") {
  spice::SpiceInput in = parallel();
  in.devices.resize(2);
  spice::SpiceSettings s = scale_off();
  s.merge = spice::MergeMode::conservative;
  in.scale = 100.0F;
  CHECK(write(in, s, false).text.find("M=2\n+  ad=15p pd=8u as=20p ps=13u\n") != std::string::npos);
}

TEST_CASE("Declared parameters are written in stored order", "[cap:netlist-spice-devices][req:Device parameter output]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("g"), node("s"), node("d", 0, {{12, 14}})};
  in.params["nfet"] = {{"w", "w"}, {"l", "l"}, {"p2", "pd"}, {"a2", "ad"}, {"x", "xp"}, {"r", "rv"}};
  spice::InputDevice d = mos("nfet", 2, 8, "GND!", {term("g"), term("s"), term("d")});
  d.rect = {7, 0, 9, 8};
  d.resistance = 1500.0F;
  d.params = {"mult=1"};
  in.devices = {d};
  // p2 followed by a2 is one pair, area first.
  CHECK(write(in).text.find("nfet w=8 l=2 w=8 l=2 ad=12p pd=14u xp=7 rv=1.5k mult=1\n") != std::string::npos);
}

TEST_CASE("A consumed l<n> parameter leaves a space with magic-compat ON",
          "[cap:netlist-spice-devices][req:Device parameter output]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("a"), node("b"), node("m")};
  in.params["rpoly"] = {{"l1", "len"}};
  spice::InputDevice d = device(DeviceClass::subckt, "rpoly", {term("m"), term("a"), term("b")});
  d.params = {"l1=4", "foo=1"};
  in.devices = {d};
  CHECK(write(in).text.find("X0 m a b rpoly len=4  foo=1\n") != std::string::npos);
}

TEST_CASE("A consumed l<n> parameter is skipped with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Device parameter output]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("a"), node("b"), node("m")};
  in.params["rpoly"] = {{"l1", "len"}};
  spice::InputDevice d = device(DeviceClass::subckt, "rpoly", {term("m"), term("a"), term("b")});
  d.params = {"l1=4", "foo=1"};
  in.devices = {d};
  CHECK(write(in, {}, false).text.find("X0 m a b rpoly len=4 foo=1\n") != std::string::npos);
}

namespace {

spice::SpiceInput perimeter_param() {
  spice::SpiceInput in = nltest_input("top");
  in.scale = 100.0F;
  in.nodes = {node("g"), node("s"), node("d")};
  in.params["nfet"] = {{"p", "pd"}};
  spice::InputDevice d = mos("nfet", 2, 8, "GND!", {term("g"), term("s"), term("d")});
  d.perim = 16;
  in.devices = {d};
  return in;
}

spice::SpiceInput offset_param() {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("g"), node("s"), node("d")};
  in.params["nfet"] = {{"l", "lo", 1.0, 3}};
  in.devices = {mos("nfet", 2, 8, "GND!", {term("g"), term("s"), term("d")})};
  return in;
}

}  // namespace

TEST_CASE("The flat SI perimeter uses 1e-12 with magic-compat ON", "[cap:netlist-spice-devices][req:Device parameter output]") {
  CHECK(write(perimeter_param(), scale_off()).text.find(" pd=16p\n") != std::string::npos);
}

TEST_CASE("The flat SI perimeter uses 1e-6 with magic-compat OFF", "[cap:netlist-spice-devices][req:Device parameter output]") {
  CHECK(write(perimeter_param(), scale_off(), false).text.find(" pd=16u\n") != std::string::npos);
}

TEST_CASE("Parameter offsets apply only to SI values with magic-compat ON",
          "[cap:netlist-spice-devices][req:Device parameter output]") {
  CHECK(write(offset_param()).text.find("w=8 l=2 lo=2\n") != std::string::npos);
  CHECK(write(offset_param(), scale_off()).text.find("w=80n l=20n lo=50n\n") != std::string::npos);
}

TEST_CASE("Parameter offsets apply in every column with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Device parameter output]") {
  CHECK(write(offset_param(), {}, false).text.find("w=8 l=2 lo=5\n") != std::string::npos);
}

TEST_CASE("Resistors move body capacitance and split with a tee (NL-SPICE-10)",
          "[cap:netlist-spice-devices][req:Resistor lines]") {
  spice::SpiceSettings off;
  off.cthresh = 0.0;
  CHECK(write(resistors(), off).text == golden("NL-SPICE-10/expected/collected/top.teeoff.spice"));
  spice::SpiceSettings on = off;
  on.resistor_tee = true;
  CHECK(write(resistors(), on).text == golden("NL-SPICE-10/expected/collected/top.teeon.spice"));
}

TEST_CASE("Unmodeled resistors print their value", "[cap:netlist-spice-devices][req:Resistor lines]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("m"), node("a"), node("b")};
  spice::InputDevice r = device(DeviceClass::res, "None", {term("m"), term("a"), term("b")});
  r.resistance = 25.5F;
  in.devices = {r};
  CHECK(write(in).text.find("R0 a b 25.500000\n") != std::string::npos);
}

TEST_CASE("Flat tee rsubckt writes one half with magic-compat ON", "[cap:netlist-spice-devices][req:Subcircuit-class device lines]") {
  spice::SpiceSettings tee;
  tee.resistor_tee = true;
  spice::SpiceInput in = resistors();
  in.devices = {in.devices[1]};
  const std::string text = write(in, tee).text;
  CHECK(text.find("X0A ra rb sub1 rres\nC0") != std::string::npos);
  CHECK(text.find("X0B") == std::string::npos);
}

TEST_CASE("Flat tee rsubckt writes both halves with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Subcircuit-class device lines]") {
  spice::SpiceSettings tee;
  tee.resistor_tee = true;
  tee.cthresh = 0.0;
  spice::SpiceInput in = resistors();
  in.devices = {in.devices[1]};
  const std::string text = write(in, tee, false).text;
  CHECK(text.find("X0A ra rb sub1 rres\nX0B rb rc sub1 rres\n") != std::string::npos);
  // The substrate pin zeroes the body node's capacitance.
  CHECK(text.find(" rb ") == text.find(" rb sub1"));
  CHECK(text.find("C0 rb") == std::string::npos);
}

TEST_CASE("CDL writes unnamed values and the substrate parameter (NL-SPICE-11)",
          "[cap:netlist-spice-devices][req:Capacitor, diode, BJT and voltage source lines]") {
  CHECK(write(cdl_values(), with_format(Dialect::cdl)).text ==
        golden("NL-SPICE-11/expected/collected/top.cdl"));
}

TEST_CASE("A flat unnamed c parameter prints the resistance with magic-compat ON",
          "[cap:netlist-spice-devices][req:Resistor lines]") {
  spice::SpiceInput in = cdl_values();
  in.devices[0].capacitance = 7.0F;
  in.devices[0].resistance = 3.0F;
  CHECK(write(in, with_format(Dialect::cdl)).text.find("C0 ct cb 3 sub=GND!") != std::string::npos);
}

TEST_CASE("A flat unnamed c parameter prints the capacitance with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Resistor lines]") {
  spice::SpiceInput in = cdl_values();
  in.devices[0].capacitance = 7.0F;
  in.devices[0].resistance = 3.0F;
  CHECK(write(in, with_format(Dialect::cdl), false).text.find("C0 ct cb 7 sub=GND!") !=
        std::string::npos);
}

TEST_CASE("Capacitor, diode, BJT and voltage source lines", "[cap:netlist-spice-devices][req:Capacitor, diode, BJT and voltage source lines]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("top"), node("bot"), node("a"), node("b"), node("sub")};
  spice::InputDevice c = device(DeviceClass::cap, "None", {term("top"), term("bot")});
  c.capacitance = 12.5F;
  spice::InputDevice crev = c;
  crev.device_class = DeviceClass::caprev;
  in.devices = {c,
                crev,
                device(DeviceClass::diode, "dn", {term("a"), term("b")}),
                device(DeviceClass::ndiode, "dn", {term("a")}, "sub"),
                device(DeviceClass::bjt, "npn", {term("a"), term("a"), term("b")}, "sub"),
                device(DeviceClass::volt, "None", {term("a"), term("a"), term("b")})};
  const std::string text = write(in).text;
  CHECK(text.find("C0 top bot 12.5f\nC1 bot top 12.5f\nD0 a b dn\nD1 sub a dn\nQ1000 sub a b npn\nV0 a b 0.0\n") !=
        std::string::npos);
}

TEST_CASE("Subcircuit devices order their terminals by class",
          "[cap:netlist-spice-devices][req:Subcircuit-class device lines]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("c1"), node("c2"), node("t0"), node("t1"), node("t2"), node("t3"), node("t4")};
  in.devices = {device(DeviceClass::subckt, "sky130_fd_pr__cap_mim", {term("c1"), term("c2")}),
                device(DeviceClass::msubckt, "mm", {term("t0"), term("t1"), term("t2")}),
                device(DeviceClass::dsubckt, "dd", {term("t0"), term("t1")}),
                device(DeviceClass::veriloga, "va", {term("t0"), term("t1"), term("t2"), term("t3"), term("t4")})};
  const std::string text = write(in, with_format(Dialect::cdl)).text;
  CHECK(text.find("X0 c1 c2 / sky130_fd_pr__cap_mim\nX1 t2 t0 t1 / mm\nX2 t1 t0 / dd\nN3 t0 t1 t2 t3 / va\n") !=
        std::string::npos);
}

TEST_CASE("Subcircuit devices keep every terminal with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Subcircuit-class device lines]") {
  // Magic's loop over terminals 3.. starts one late and drops the last one.
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("t0"), node("t1"), node("t2"), node("t3"), node("t4")};
  in.devices = {device(DeviceClass::veriloga, "va", {term("t0"), term("t1"), term("t2"), term("t3"), term("t4")})};
  CHECK(write(in, {}, false).text.find("N0 t0 t1 t2 t3 t4 va\n") != std::string::npos);
}

TEST_CASE("Substrate text uses the default name or the node (NL-SPICE-14)",
          "[cap:netlist-spice-devices][req:Substrate terminal text]") {
  spice::SpiceInput in = nltest_input("top");
  in.nodes = {node("g", 1), node("s", 1), node("well1", 1), node("VSUBS!")};
  in.devices = {mos("nfet", 2, 4, "VSUBS!", {term("g", 8), term("s", 4), term("g", 4)}),
                mos("nfet", 2, 4, "well1", {term("g", 8), term("s", 4), term("g", 4)})};
  CHECK(write(in).text == golden("NL-SPICE-14/expected/collected/top.spice"));
  // A default name is trimmed by esFormatSubs; an unknown node is errGnd!.
  spice::SpiceInput vsubs = nltest_input("top");
  vsubs.types["nfet"].default_substrate = "VSUBS!";
  vsubs.nodes = {node("g")};
  vsubs.devices = {mos("nfet", 2, 4, "VSUBS!", {term("g"), term("g"), term("g")}),
                   mos("nfet", 2, 4, "nowhere", {term("g"), term("g"), term("g")})};
  const std::string text = write(vsubs).text;
  CHECK(text.find("M1000 g g g VSUBS nfet") != std::string::npos);
  CHECK(text.find("M1001 g g g errGnd! nfet") != std::string::npos);
}

TEST_CASE("Parallel devices merge as none, conservative and aggressive (NL-SPICE-12)",
          "[cap:netlist-spice-devices][req:Parallel device merging]") {
  CHECK(write(parallel()).text == golden("NL-SPICE-12/expected/collected/top.none.spice"));
  spice::SpiceSettings cons;
  cons.merge = spice::MergeMode::conservative;
  const auto c = write(parallel(), cons);
  CHECK(c.text == golden("NL-SPICE-12/expected/collected/top.conservative.spice"));
  REQUIRE_FALSE(c.messages.empty());
  CHECK(c.messages.front().text == "Devs merged: 4");
  spice::SpiceSettings aggr;
  aggr.merge = spice::MergeMode::aggressive;
  const auto a = write(parallel(), aggr);
  CHECK(a.text == golden("NL-SPICE-12/expected/collected/top.aggressive.spice"));
  CHECK(a.messages.front().text == "Devs merged: 5");
}

TEST_CASE("A one-terminal device keeps M = 1 and later devices keep their multipliers",
          "[cap:netlist-spice-devices][req:Parallel device merging]") {
  // Regression for Q-NETLIST-6 (fix, both modes): Magic's flat merge pass
  // reserves no slot for a one-terminal device, so the devices after it
  // read the wrong multiplier (dropping a device or printing a wrong M).
  spice::SpiceInput in = parallel();
  in.devices.resize(2);  // two parallel nfets: the second merges into the first
  spice::InputDevice one = device(DeviceClass::subckt, "blk", {term("g")});
  in.devices.insert(in.devices.begin(), one);
  in.devices.push_back(mos("nfet", 2, 4, "GND!", {term("g"), term("s"), term("ct")}));
  for (const bool compat : {true, false}) {
    spice::SpiceSettings cons;
    cons.merge = spice::MergeMode::conservative;
    const auto out = write(in, cons, compat);
    INFO(out.text);
    CHECK(out.text.find("X0 g blk\nM1000 d g s GND nfet w=8 l=2 M=2\n") != std::string::npos);
    CHECK(out.text.find("M1001 ct g s GND nfet w=4 l=2\n") != std::string::npos);
    CHECK(out.messages.front().text == "outPremature");
  }
}
