// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-devices on the captured flat EX cases (openspec/changes/
// m7-extraction tasks.md 7.1-7.5): each test extracts the case's cell at
// the device level and compares the record with the `device` line of
// tests/golden/EXTRACT/cases/EX-*/expected/collected/input/*.ext, the nodes
// named by their label, or `@x_y` (the reference point Magic's generated
// name encodes) without one. EX replay waits for EXTCMD's driver.
// Owner: EXTDEV.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "device_fixture.hpp"

using namespace hefesto::test::dev;  // NOLINT(google-build-using-namespace): test fixture helpers

namespace {

std::vector<std::string> one(const std::string& line) {
  return {line};
}

std::size_t feedback_count(const Case& c) {
  return c.result.feedback.size();
}

}  // namespace

// --- Device regions ---------------------------------------------------------

TEST_CASE("EX-037: device regions in region-list order, area on the lowest plane",
          "[cap:extract-devices][req:Device regions]") {
  const auto c = extract_case("EX-037");
  REQUIRE(c->regions.size() == 2);
  // Discovered left to right, written in reverse (Magic prepends).
  CHECK(c->regions[0].region.reference == db::Point{20, 0});
  CHECK(c->regions[0].region.area == 32);
  CHECK(c->regions[1].region.reference == db::Point{0, 0});
  CHECK(c->regions[1].region.area == 8);
}

TEST_CASE("A mimcap device region: area 80, reference (0,0)", "[cap:extract-devices][req:Device regions]") {
  const auto c = extract_texts(tech_with_extract("    device capacitor None mimcap metal2 10 2\n"),
                               "<< mimcap >>\nrect 0 0 10 8\n<< metal2 >>\nrect -1 -1 11 11\n");
  REQUIRE(c->regions.size() == 1);
  CHECK(c->regions[0].region.area == 80);
  CHECK(c->regions[0].region.reference == db::Point{0, 0});
}

TEST_CASE("An L-shaped device region keeps its lowest-leftmost tile as reference",
          "[cap:extract-devices][req:Device regions]") {
  const auto c = extract_case("EX-029");
  REQUIRE(c->regions.size() == 1);
  CHECK(c->regions[0].region.reference == db::Point{0, 0});
  CHECK(c->regions[0].region.area == 2 * 8 + 6 * 3);
  CHECK(c->regions[0].tiles.size() == 2);
}

// --- Terminal discovery on the device plane ----------------------------------

TEST_CASE("EX-026: rectangular MOSFET terminals, gate length and perimeter",
          "[cap:extract-devices][req:Terminal discovery on the device plane]") {
  const auto c = extract_case("EX-026");
  CHECK(c->lines() == one("device mosfet nfet 0 0 1 1 2 4 \"GND!\" \"g\" 4 0 \"s\" 4 12,14 \"d\" 4 12,14"));
  CHECK(c->result.messages.empty());
}

TEST_CASE("EX-044: the legacy fet record carries the device perimeter 12",
          "[cap:extract-devices][req:Terminal discovery on the device plane]") {
  const auto c = extract_case("EX-044");
  CHECK(c->lines() == one("fet nfet 0 0 1 1 8 12 \"GND!\" \"g\" 4 0 \"s\" 4 12,14 \"d\" 4 12,14"));
}

// --- Terminal area, perimeter and sharing -----------------------------------

TEST_CASE("EX-040: shared diffusion between two fingers is divided by the sharing count",
          "[cap:extract-devices][req:Terminal area, perimeter and sharing]") {
  const auto c = extract_case("EX-040");
  CHECK(c->lines() ==
        std::vector<std::string>{
            "device mosfet nfet 8 0 9 1 2 4 \"GND!\" \"g2\" 0 0 \"mid\" 4 12,10 \"s2\" 4 12,14",
            "device mosfet nfet 0 0 1 1 2 4 \"GND!\" \"g1\" 0 0 \"s1\" 4 12,14 \"mid\" 4 12,10"});
}

// --- Terminals off the device plane -----------------------------------------

TEST_CASE("EX-033: a capacitor bottom plate on another plane: length 0, area 144, perimeter 48",
          "[cap:extract-devices][req:Terminals off the device plane]") {
  const auto c = extract_case("EX-033");
  REQUIRE(c->result.devices.size() == 1);
  const auto& t = c->result.devices[0].terminals.at(0);
  CHECK(c->name(t.node) == "b");
  CHECK(t.length == 0);
  CHECK(t.area == 144);
  CHECK(t.perimeter == 48);
}

TEST_CASE("EX-035: a diode terminal is the well under it, found by the area search",
          "[cap:extract-devices][req:Terminals off the device plane]") {
  const auto c = extract_case("EX-035");
  REQUIRE(c->result.devices.size() == 1);
  const auto& t = c->result.devices[0].terminals.at(0);
  CHECK(c->name(t.node) == "w");
  CHECK(t.area == 225);
  CHECK(t.perimeter == 60);
}

// --- Device record selection --------------------------------------------------

TEST_CASE("EX-038: records chosen by substrate layer and identifier layer",
          "[cap:extract-devices][req:Device record selection]") {
  const auto c = extract_case("EX-038");
  CHECK(c->lines() ==
        std::vector<std::string>{
            "device mosfet nfet_nw 60 0 61 1 2 4 \"VSUB\" \"g60\" 0 0 \"@57_0\" 4 12,14 \"@62_0\" 4 12,14",
            "device mosfet nfet_nw 40 0 41 1 2 4 \"@34_-4\" \"g40\" 0 0 \"@37_0\" 4 12,14 \"@42_0\" 4 12,14",
            "device mosfet nfet_hv 20 0 21 1 2 4 \"@14_-4\" \"g20\" 0 0 \"@17_0\" 4 12,14 \"@22_0\" 4 12,14",
            "device mosfet nfet 0 0 1 1 2 4 \"@-6_-4\" \"g0\" 0 0 \"@-3_0\" 4 12,14 \"@2_0\" 4 12,14"});
  CHECK(c->result.messages.empty());  // the dnwell gate keeps its record's substrate name, silently
}

TEST_CASE("A substrate layer switches to the record that lists it",
          "[cap:extract-devices][req:Device record selection]") {
  const auto c = extract_texts(tech_with_extract("    device mosfet nfet nfet ndiff pwell VSUB\n"
                                                 "    device mosfet nfet nfet ndiff nwell VSUB\n"
                                                 "    device mosfet nfet nfet ndiff dnwell VSUB\n"),
                               "<< nfet >>\nrect 0 0 2 4\n<< ndiff >>\nrect -3 0 0 4\nrect 2 0 5 4\n"
                               "<< nwell >>\nrect -6 -4 8 8\n");
  CHECK(c->lines() == one("device mosfet nfet 0 0 1 1 2 4 \"@-6_-4\" \"@0_0\" 0 0 \"@-3_0\" 4 12,14 \"@2_0\" 4 12,14"));
}

// --- Missing terminals and rejected devices -----------------------------------

TEST_CASE("EX-039: the missing terminal copies the last one; feedback only with warn fets",
          "[cap:extract-devices][req:Missing terminals and rejected devices]") {
  Options warn;
  const auto on = extract_case("EX-039", warn);
  CHECK(on->lines() == one("device mosfet nfet 0 0 1 1 2 4 \"GND!\" \"g\" 0 0 \"s\" 0 0 \"s\" 4 12,14"));
  CHECK(feedback_count(*on) == 1);
  CHECK(on->result.feedback.at(0).text.starts_with("device missing 1 terminal;\n connecting remainder to node "));
  Options quiet;
  quiet.warn_fets = false;
  const auto off = extract_case("EX-039", quiet);
  CHECK(off->lines() == on->lines());
  CHECK(feedback_count(*off) == 0);
}

TEST_CASE("A device with no terminal at all is not written",
          "[cap:extract-devices][req:Missing terminals and rejected devices]") {
  const auto c = extract_texts(tech_with_extract("    device mosfet nfet nfet ndiff None GND!\n"),
                               "<< nfet >>\nrect 0 0 2 4\n<< poly >>\nrect 0 4 2 6\n");
  CHECK(c->result.devices.empty());
  REQUIRE(c->result.feedback.size() == 1);
  CHECK(c->result.feedback[0].text == "device missing 2 terminals");
}

TEST_CASE("A record whose model is Ignore produces no output",
          "[cap:extract-devices][req:Missing terminals and rejected devices]") {
  const auto c = extract_texts(tech_with_extract("    device mosfet Ignore nfet ndiff None GND!\n"),
                               read_text(case_dir("EX-026") + "ex.mag"));
  CHECK(c->result.devices.empty());
  CHECK(c->result.messages.empty());
}

// --- Device record header and substrate field ---------------------------------

TEST_CASE("EX-027: the substrate field names the node found under the device",
          "[cap:extract-devices][req:Device record header and substrate field]") {
  const auto c = extract_case("EX-027");
  CHECK(c->lines() ==
        one("device asymmetric nfet 0 0 1 1 2 4 \"@-6_-4\" \"g\" 4 0 \"s\" 4 12,14 \"d\" 4 12,14"));
}

TEST_CASE("EX-031: a resistor without model writes no substrate field",
          "[cap:extract-devices][req:Device record header and substrate field]") {
  const auto c = extract_case("EX-031");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].substrate_field == extract::SubstrateField::none);
  CHECK(c->result.devices[0].area == db::Rect{0, 0, 1, 1});
}

// --- MOSFET-family length and width ---------------------------------------------

TEST_CASE("EX-026: L = gate length / 2, W = terminal length",
          "[cap:extract-devices][req:MOSFET-family length and width]") {
  const auto c = extract_case("EX-026");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].length == 2);
  CHECK(c->result.devices[0].width == 4);
}

TEST_CASE("EX-030: MOS capacitor with tied source and drain",
          "[cap:extract-devices][req:MOSFET-family length and width]") {
  const auto c = extract_case("EX-030");
  CHECK(c->lines() == one("device mosfet nfet 0 0 1 1 2 4 \"GND!\" \"g\" 0 0 \"sd\" 0 0 \"sd\" 8 12,14"));
}

// --- Boundary lists and effective L, W ------------------------------------------

TEST_CASE("EX-028: annular gate, L = 2, W = 24", "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  const auto c = extract_case("EX-028");
  CHECK(c->lines() ==
        one("device mosfet nfet 0 0 1 1 2 24 \"GND!\" \"g\" 0 0 \"out\" 32 132,88 \"in\" 16 16,16"));
}

TEST_CASE("EX-029: bent gate with two lengths: widths scaled to the shorter, with the message",
          "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  const auto c = extract_case("EX-029");
  CHECK(c->lines() == one("device mosfet nfet 0 0 1 1 2 10 \"GND!\" \"g\" 0 0 \"s\" 16 57,44 \"d\" 11 30,22"));
  REQUIRE(c->result.messages.size() == 1);
  CHECK(c->result.messages[0].code == "EXTRACT-DEV-008");
  CHECK(c->result.messages[0].text == "Device has multiple lengths:  scaling all widths to length 2");
}

TEST_CASE("EX-030: the tied terminals' list is split into the two sides",
          "[cap:extract-devices][req:Boundary lists for dimension computation]") {
  const auto c = extract_case("EX-030");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].length == 2);
  CHECK(c->result.devices[0].width == 4);
}

TEST_CASE("EX-032: serpentine resistor from the side lists", "[cap:extract-devices][req:Boundary lists for dimension computation]") {
  const auto c = extract_case("EX-032");
  CHECK(c->lines() == one("device devres rppd 0 0 1 1 24 2 \"@0_0\" 0 0 \"b\" 2 4,8 \"a\" 2 4,8"));
}

// --- Resistors ------------------------------------------------------------------

TEST_CASE("EX-031: single-tile resistor value R = sheet * L / W in ohms",
          "[cap:extract-devices][req:Resistor dimensions and value]") {
  const auto c = extract_case("EX-031");
  CHECK(c->lines() == one("device devres None 0 0 1 1 10 \"@0_0\" 0 0 \"a\" 2 4,8 \"b\" 2 4,8"));
  CHECK(c->result.feedback.empty());
}

TEST_CASE("EX-032: modelled resistor writes L W and no value", "[cap:extract-devices][req:Resistor dimensions and value]") {
  const auto c = extract_case("EX-032");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].fields == std::vector<std::string>{"24", "2"});
}

// --- Capacitors -----------------------------------------------------------------

TEST_CASE("EX-033: capacitor without model writes C in fF", "[cap:extract-devices][req:Capacitor dimensions and value]") {
  const auto c = extract_case("EX-033");
  CHECK(c->lines() == one("device devcap None 0 0 1 1 0.6 \"t\" 0 0 \"b\" 0 144,48"));
}

TEST_CASE("EX-034: L-shaped modelled capacitor writes W and area/W, with feedback",
          "[cap:extract-devices][req:Capacitor dimensions and value]") {
  const auto c = extract_case("EX-034");
  CHECK(c->lines() == one("device devcap mimcapm 0 0 1 1 10 6 \"t\" 0 0 \"b\" 0 144,48"));
  REQUIRE(c->result.feedback.size() == 1);
  CHECK(c->result.feedback[0].text == "L,W estimated for non-rectangular capacitor.");
}

// --- Diodes ---------------------------------------------------------------------

TEST_CASE("EX-035: diode area and perimeter parameters", "[cap:extract-devices][req:Diode records]") {
  const auto c = extract_case("EX-035");
  CHECK(c->lines() == one("device diode dn 0 0 1 1 a=20 p=18 \"w\" \"k\" 0 0 \"w\" 0 225,60"));
}

// --- Device parameter values ------------------------------------------------------

TEST_CASE("EX-036: l, w, c and r parameters (r with devresist area and perimeter)",
          "[cap:extract-devices][req:Device parameter values]") {
  const auto c = extract_case("EX-036");
  CHECK(c->lines() ==
        one("device msubckt nmod 0 0 1 1 l=2 w=4 c=0 r=3 \"VSUB\" \"g\" 4 0 \"s\" 4 12,14 \"d\" 4 12,14"));
}

TEST_CASE("EX-041: a gate attribute with '=' is a parameter, not a gate attribute",
          "[cap:extract-devices][req:Device parameter values]") {
  const auto c = extract_case("EX-041");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].params == std::vector<std::string>{"mult=2"});
  CHECK(c->result.devices[0].gate.attributes.empty());
}

// --- Terminal output and ordering ---------------------------------------------------

TEST_CASE("EX-041: a terminal attribute on the source boundary", "[cap:extract-devices][req:Terminal output and ordering]") {
  const auto c = extract_case("EX-041");
  CHECK(c->lines() ==
        one("device mosfet nfet 0 0 1 1 2 4 mult=2 \"GND!\" \"g\" 4 0 \"s\" 4 \"S\",12,14 \"d\" 4 12,14"));
}

TEST_CASE("EX-027: asymmetric terminals keep the declaration order",
          "[cap:extract-devices][req:Terminal output and ordering]") {
  const auto c = extract_case("EX-027");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->name(c->result.devices[0].terminals.at(0).node) == "s");
  CHECK(c->name(c->result.devices[0].terminals.at(1).node) == "d");
}

TEST_CASE("Symmetric terminals are sorted by position", "[cap:extract-devices][req:Terminal output and ordering]") {
  // The drain is found first (the top-right walk) but sorts after the source.
  const auto c = extract_texts(tech_with_extract("    device mosfet nfet nfet ndiff None GND!\n"),
                               "<< nfet >>\nrect 0 0 2 4\n<< ndiff >>\nrect -3 0 0 4\nrect 2 0 5 4\n"
                               "<< labels >>\nrlabel ndiff 3 2 3 2 0 d\nrlabel ndiff -2 2 -2 2 0 s\n");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->name(c->result.devices[0].terminals.at(0).node) == "s");
  CHECK(c->name(c->result.devices[0].terminals.at(1).node) == "d");
}
