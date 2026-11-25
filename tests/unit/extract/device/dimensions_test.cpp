// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-devices: the dimension algorithms on hand-built boundary lists
// with pinned values (extComputeEffectiveLW, extSeparateBounds,
// extComputeCapLW and the three sqrt estimates of design D22), parameter
// ranges per mode (Q-EXTRACT-21), the regression tests of the fixed Magic
// bugs (Q-EXTRACT-8, -13, -15), `parameters` records and the `device` cell
// property (Q-EXTRACT-16). openspec/changes/m7-extraction tasks.md 7.2-7.5.
// Owner: EXTDEV.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "device_fixture.hpp"

using namespace hefesto::test::dev;  // NOLINT(google-build-using-namespace): test fixture helpers
using extract::BoundaryDir;
using extract::BoundaryLists;
using extract::BoundarySegment;

namespace {

BoundarySegment seg(db::Coord x0, db::Coord y0, db::Coord x1, db::Coord y1, BoundaryDir d) {
  return BoundarySegment{{x0, y0, x1, y1}, d};
}

// The boundary of the square ring gate of the spec scenario: inner
// diffusion (2,2)-(6,6) as terminal 0, outer (0,0)-(8,8) as terminal 1.
BoundaryLists annular_lists() {
  return {{seg(6, 2, 6, 6, BoundaryDir::left), seg(2, 2, 2, 6, BoundaryDir::right),
           seg(2, 6, 6, 6, BoundaryDir::bottom), seg(2, 2, 6, 2, BoundaryDir::top)},
          {seg(0, 0, 0, 8, BoundaryDir::left), seg(8, 0, 8, 8, BoundaryDir::right),
           seg(0, 0, 8, 0, BoundaryDir::bottom), seg(0, 8, 8, 8, BoundaryDir::top)}};
}

const char* const kTwoModels =
    "    device mosfet nshort nfet ndiff None GND! l<5\n"
    "    device mosfet nlong nfet ndiff None GND! l>=5\n";

}  // namespace

// --- Effective length and width of non-rectangular gates -------------------------

TEST_CASE("Rectangle: loc = 2, lw = 8, W = 8 >> 1 = 4, L = 2",
          "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  const BoundaryLists lists{{seg(0, 0, 0, 4, BoundaryDir::left)}, {seg(2, 0, 2, 4, BoundaryDir::right)}};
  bool multiple = true;
  const auto lw = extract::effective_lw(lists, 2, 1.0F, &multiple);
  REQUIRE(lw);
  CHECK(*lw == extract::Dimensions{2, 4});
  CHECK_FALSE(multiple);
}

TEST_CASE("Annular gate: L = 2 and W = (4 sides x 2 x 4 + 8 corner extensions x 2) >> 1 = 24",
          "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  const auto lw = extract::effective_lw(annular_lists(), 2, 1.0F);
  REQUIRE(lw);
  CHECK(*lw == extract::Dimensions{2, 24});
}

TEST_CASE("The corner chop is applied in single precision and rounded half up",
          "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  // Each inner side has cw = 4: (int)(0.5 + 4 * 0.6f) = 2, lw = 8 + 2 per side.
  const auto lw = extract::effective_lw(annular_lists(), 2, 0.6F);
  REQUIRE(lw);
  CHECK(*lw == extract::Dimensions{2, (4 * 10) >> 1});
}

TEST_CASE("Pinned diagonal width: (int)(0.5 + sqrt(dx^2 + dy^2)) per non-Manhattan segment (ExtBasic.c:1535)",
          "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  BoundaryLists lists{{seg(0, 0, 0, 4, BoundaryDir::left)},
                      {seg(2, 0, 2, 4, BoundaryDir::right), seg(2, 4, 5, 8, BoundaryDir::ne)}};
  auto lw = extract::effective_lw(lists, 2, 1.0F);
  REQUIRE(lw);
  CHECK(lw->width == (8 + 5) >> 1);  // sqrt(9 + 16) = 5
  lists[1][1] = seg(2, 4, 4, 7, BoundaryDir::sw);  // sqrt(13) = 3.606: 4
  lw = extract::effective_lw(lists, 2, 1.0F);
  REQUIRE(lw);
  CHECK(lw->width == (8 + 4) >> 1);
  lists[1][1] = seg(2, 4, 3, 5, BoundaryDir::se);  // sqrt(2) = 1.414: 1
  lw = extract::effective_lw(lists, 2, 1.0F);
  REQUIRE(lw);
  CHECK(lw->width == (8 + 1) >> 1);
}

TEST_CASE("No opposite segment: no estimate", "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  const BoundaryLists lists{{seg(0, 0, 0, 4, BoundaryDir::left)}, {seg(0, 4, 2, 4, BoundaryDir::top)}};
  CHECK_FALSE(extract::effective_lw(lists, 2, 1.0F));
}

TEST_CASE("One list: the width counted twice is divided by four",
          "[cap:extract-devices][req:Effective length and width of non-rectangular gates]") {
  const BoundaryLists lists{{seg(0, 0, 0, 4, BoundaryDir::left), seg(2, 0, 2, 4, BoundaryDir::right)}};
  const auto lw = extract::effective_lw(lists, 1, 1.0F);
  REQUIRE(lw);
  CHECK(*lw == extract::Dimensions{2, 16 >> 2});
}

// --- Boundary lists for dimension computation ------------------------------------

TEST_CASE("MOS capacitor: an empty last list takes the first chain of list 0",
          "[cap:extract-devices][req:Boundary lists for dimension computation]") {
  BoundaryLists lists{{seg(2, 0, 2, 4, BoundaryDir::right), seg(0, 0, 0, 4, BoundaryDir::left)}, {}};
  extract::separate_bounds(lists, 2);
  CHECK(lists[0] == std::vector<BoundarySegment>{seg(0, 0, 0, 4, BoundaryDir::left)});
  CHECK(lists[1] == std::vector<BoundarySegment>{seg(2, 0, 2, 4, BoundaryDir::right)});
}

TEST_CASE("Separation follows the chain of segments sharing end points",
          "[cap:extract-devices][req:Boundary lists for dimension computation]") {
  BoundaryLists lists{{seg(0, 0, 0, 4, BoundaryDir::left), seg(0, 4, 3, 4, BoundaryDir::top),
                       seg(5, 0, 5, 4, BoundaryDir::right)},
                      {}};
  extract::separate_bounds(lists, 2);
  CHECK(lists[1] ==
        std::vector<BoundarySegment>{seg(0, 0, 0, 4, BoundaryDir::left), seg(0, 4, 3, 4, BoundaryDir::top)});
  CHECK(lists[0] == std::vector<BoundarySegment>{seg(5, 0, 5, 4, BoundaryDir::right)});
}

TEST_CASE("Separation leaves a filled last list alone", "[cap:extract-devices][req:Boundary lists for dimension computation]") {
  const BoundaryLists before{{seg(0, 0, 0, 4, BoundaryDir::left)}, {seg(2, 0, 2, 4, BoundaryDir::right)}};
  BoundaryLists lists = before;
  extract::separate_bounds(lists, 2);
  CHECK(lists == before);
  extract::separate_bounds(lists, 0);  // a badly defined device: no change, no crash
  CHECK(lists == before);
}

// --- Capacitor dimensions and value ------------------------------------------------

TEST_CASE("Capacitor L and W from the bounding box of list 0", "[cap:extract-devices][req:Capacitor dimensions and value]") {
  const BoundaryLists lists{{seg(0, 0, 0, 4, BoundaryDir::left), seg(0, 4, 10, 4, BoundaryDir::top)}};
  const auto lw = extract::cap_lw(lists);
  REQUIRE(lw);
  CHECK(*lw == extract::Dimensions{4, 10});
  CHECK_FALSE(extract::cap_lw(BoundaryLists{{}}));
}

TEST_CASE("Pinned equivalent-square fallback: (int)(sqrt(area) + 0.5) (ExtBasic.c:2948, 3206)",
          "[cap:extract-devices][req:Capacitor dimensions and value]") {
  CHECK(extract::equivalent_square(144) == 12);
  CHECK(extract::equivalent_square(156) == 12);  // 12.49
  CHECK(extract::equivalent_square(157) == 13);  // 12.53
  CHECK(extract::equivalent_square(2) == 1);     // 1.414
  CHECK(extract::equivalent_square(0) == 0);
}

TEST_CASE("Q-EXTRACT-8 regression: a zero-size modelled capacitor writes 0 0 once",
          "[cap:extract-devices][req:Capacitor dimensions and value]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Options o;
    o.magic_compat = on;
    // The terminal is the (absent) substrate: no node, so no terminal area.
    const auto c = extract_texts(tech_with_extract("    device capacitor capm mimcap metal2,space 10 2\n"),
                                 "<< mimcap >>\nrect 0 0 10 10\n", o);
    REQUIRE(c->result.devices.size() == 1);
    CHECK(c->result.devices[0].fields == std::vector<std::string>{"0", "0"});
    CHECK(c->lines() == one_line("device devcap capm 0 0 1 1 0 0 \"@0_0\" 0 0 \"(none)\" 0 0"));
    REQUIRE(c->result.feedback.size() == 1);
    CHECK(c->result.feedback[0].text == "Capacitor has zero size");
  }
}

// --- Diode records ---------------------------------------------------------------------

TEST_CASE("Q-EXTRACT-13 regression: diode L and W are computed before the range matching",
          "[cap:extract-devices][req:Diode records]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Options o;
    o.magic_compat = on;
    // dsmall (tried first) accepts l <= 3; the 4x5 diode has L = 5, so dbig.
    const auto c = extract_texts(tech_with_extract("    device diode dbig ndiff pwell l=l w=w\n"
                                                   "    device diode dsmall ndiff pwell l<=3 l=l w=w\n"),
                                 read_text(case_dir("EX-035") + "ex.mag"), o);
    CHECK(c->lines() == one_line("device diode dbig 0 0 1 1 l=5 w=4 \"w\" \"k\" 0 0 \"w\" 0 225,60"));
  }
}

TEST_CASE("A diode without l or w parameters computes no dimensions", "[cap:extract-devices][req:Diode records]") {
  const auto c = extract_case("EX-035");
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].length == 0);
  CHECK(c->result.devices[0].width == 0);
}

// --- Device parameter values -------------------------------------------------------------

TEST_CASE("Pinned w<n> terminal width: (int)(0.25 * (sqrt(p^2 - 16a) + p)) (ExtBasic.c:1916-1920)",
          "[cap:extract-devices][req:Device parameter values]") {
  CHECK(extract::terminal_width(12, 14) == 4);  // a 3x4 terminal
  CHECK(extract::terminal_width(24, 20) == 6);  // 4x6
  CHECK(extract::terminal_width(10, 14) == 5);  // sqrt(36) = 6: (6 + 14) / 4
  CHECK(extract::terminal_width(11, 14) == 4);  // sqrt(20) = 4.47: 4.62
  CHECK(extract::terminal_width(100, 10) == 0);  // negative discriminant: NaN as 0 (Q-EXTRACT-30)
}

TEST_CASE("w1 and l1 parameters from terminal 1's area and perimeter", "[cap:extract-devices][req:Device parameter values]") {
  const auto c = extract_texts(tech_with_extract("    device msubcircuit nmod nfet ndiff ndiff None VSUB w1=wa l1=la\n"),
                               read_text(case_dir("EX-026") + "ex.mag"));
  REQUIRE(c->result.devices.size() == 1);
  // Terminal 1 (index 0, the source): area 12, perimeter 14: w1 = 4, l1 = 12 / 4.
  CHECK(c->result.devices[0].params == std::vector<std::string>{"w1=4", "l1=3"});
}

TEST_CASE("Q-EXTRACT-15 regression: r is 0 for a zero width instead of dividing by zero",
          "[cap:extract-devices][req:Device parameter values]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Options o;
    o.magic_compat = on;
    // A single-tile resistor whose terminal is on another plane: no terminal
    // length, so W = 0 and L = perimeter / 2.
    const auto c = extract_texts(tech_with_extract("    resist rpoly 2000\n    device resistor rmod rpoly metal2 r=rr\n"),
                                 "<< rpoly >>\nrect 0 0 10 2\n<< metal2 >>\nrect -1 -1 11 3\n", o);
    REQUIRE(c->result.devices.size() == 1);
    CHECK(c->result.devices[0].fields == std::vector<std::string>{"12", "0"});
    CHECK(c->result.devices[0].params == std::vector<std::string>{"r=0"});
  }
}

TEST_CASE("Unknown parameter keys write an empty value; s, x, y write nothing",
          "[cap:extract-devices][req:Device parameter values]") {
  const auto c = extract_texts(tech_with_extract("    device msubcircuit nmod nfet ndiff ndiff None VSUB m=mult s=sa x=xx\n"),
                               read_text(case_dir("EX-026") + "ex.mag"));
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].params == std::vector<std::string>{"m="});
}

// --- Model selection by parameter ranges (Q-EXTRACT-21) ----------------------------------

TEST_CASE("EX-037 with magic-compat ON: a lower limit alone is never checked, both gates are nlong",
          "[cap:extract-devices][req:Model selection by parameter ranges]") {
  Options o;
  o.magic_compat = true;
  const auto c = extract_case("EX-037", o);
  CHECK(c->lines() ==
        std::vector<std::string>{
            "device mosfet nlong 20 0 21 1 8 4 \"GND!\" \"g2\" 0 0 \"@17_0\" 4 12,14 \"@28_0\" 4 12,14",
            "device mosfet nlong 0 0 1 1 2 4 \"GND!\" \"g1\" 4 0 \"s1\" 4 12,14 \"d1\" 4 12,14"});
  CHECK(c->result.messages.empty());
}

TEST_CASE("EX-037 with magic-compat OFF: the lower limit applies, the short gate is nshort",
          "[cap:extract-devices][req:Model selection by parameter ranges]") {
  Options o;
  o.magic_compat = false;
  const auto c = extract_case("EX-037", o);
  REQUIRE(c->result.devices.size() == 2);
  CHECK(c->style.devices.at(c->result.devices[0].pattern).model == "nlong");
  CHECK(c->style.devices.at(c->result.devices[1].pattern).model == "nshort");
}

TEST_CASE("Range matching: the next compatible record, else a mismatch message",
          "[cap:extract-devices][req:Model selection by parameter ranges]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Options o;
    o.magic_compat = on;
    const auto c = extract_texts(tech_with_extract(std::string(kTwoModels) +
                                                   "    device mosfet nmid nfet ndiff None GND! l>=1 l<=1\n"),
                                 read_text(case_dir("EX-026") + "ex.mag"), o);
    REQUIRE(c->result.devices.size() == 1);
    // nmid (tried first) rejects L = 2; nlong is next. With magic-compat ON
    // its lower-only range is unchecked; OFF it rejects L = 2 too and nshort
    // accepts it.
    CHECK(c->style.devices.at(c->result.devices[0].pattern).model == (on ? "nlong" : "nshort"));
  }
}

TEST_CASE("Range matching returns the last record tried (ON) or the first (OFF) when none passes",
          "[cap:extract-devices][req:Model selection by parameter ranges]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Options o;
    o.magic_compat = on;
    const auto c = extract_texts(tech_with_extract("    device mosfet na nfet ndiff None GND! l<=1\n"
                                                   "    device mosfet nb nfet ndiff None GND! l<=1\n"),
                                 read_text(case_dir("EX-026") + "ex.mag"), o);
    REQUIRE(c->result.devices.size() == 1);
    CHECK(c->style.devices.at(c->result.devices[0].pattern).model == (on ? "na" : "nb"));
    CHECK(c->codes() == std::vector<std::string>{"EXTRACT-DEV-007"});
  }
}

// --- Parameters records ---------------------------------------------------------------------

TEST_CASE("EX-036: parameters record with a scale", "[cap:extract-devices][req:Parameters records]") {
  const auto c = extract_case("EX-036");
  CHECK(extract::parameter_records(c->style, c->regions, *c->cell) ==
        std::vector<std::string>{"parameters nmod l=l w=w*1e+06 c=cg r=rr"});
}

TEST_CASE("EX-035: parameters record of a diode; none for records without named parameters",
          "[cap:extract-devices][req:Parameters records]") {
  const auto c = extract_case("EX-035");
  CHECK(extract::parameter_records(c->style, c->regions, *c->cell) ==
        std::vector<std::string>{"parameters dn a=area p=pj"});
  const auto plain = extract_case("EX-026");
  CHECK(extract::parameter_records(plain->style, plain->regions, *plain->cell).empty());
}

TEST_CASE("Parameters record offsets are written with a sign", "[cap:extract-devices][req:Parameters records]") {
  const auto c = extract_texts(tech_with_extract("    device msubcircuit nmod nfet ndiff ndiff None VSUB l=l+0.5 w=w\n"),
                               read_text(case_dir("EX-026") + "ex.mag"));
  const std::vector<std::string> records = extract::parameter_records(c->style, c->regions, *c->cell);
  REQUIRE(records.size() == 1);
  CHECK(records[0].starts_with("parameters nmod l=l"));
  CHECK(records[0].ends_with(" w=w"));
}

// --- Device cell property (Q-EXTRACT-16) -------------------------------------------------------

TEST_CASE("EX-042: a primitive cell's parameters line", "[cap:extract-devices][req:Device cell property]") {
  const auto c = extract_case("EX-042", Options{true, true, true, "cap1", {}});
  const extract::DeviceProperty p = extract::device_property(*c->cell, c->style);
  CHECK(p.found);
  CHECK(p.primitive);
  CHECK(p.parameters_line == "parameters :cap1 m=1");
}

TEST_CASE("EX-042: a device property replaces the device output", "[cap:extract-devices][req:Device cell property]") {
  const auto c = extract_case("EX-042", Options{true, true, true, "dev2", {}});
  const extract::DeviceProperty p = extract::device_property(*c->cell, c->style);
  CHECK(p.found);
  CHECK_FALSE(p.primitive);
  CHECK(p.device_line == "device mosfet nmod 0 0 2 4 4 2 extra");
  CHECK(p.messages.empty());
}

TEST_CASE("Property dimensions kept in token order and scaled by units-per-lambda (Q-EXTRACT-16)",
          "[cap:extract-devices][req:Device cell property]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);  // replicate: the same in both modes
    extract::ExtStyle style;
    style.units_per_lambda = 1.5F;
    db::CellDef cell("dev", 8);
    cell.properties().put("device", std::string("mosfet nfet 0 0 2 2 3 5 VSS"));
    CHECK(extract::device_property(cell, style).device_line == "device mosfet nfet 0 0 3 3 4 7 VSS");
    cell.properties().put("device", std::string("fet nfet 0 0 0 0 2 3"));
    CHECK(extract::device_property(cell, style).device_line == "fet nfet 0 0 1 1 4 4");  // area upl^2, perimeter upl
  }
}

TEST_CASE("Device property errors fall back to the normal output", "[cap:extract-devices][req:Device cell property]") {
  extract::ExtStyle style;
  db::CellDef cell("dev", 8);
  cell.properties().put("device", std::string("transistor nfet 0 0 2 2"));
  extract::DeviceProperty p = extract::device_property(cell, style);
  CHECK(p.device_line.empty());
  REQUIRE(p.messages.size() == 1);
  CHECK(p.messages[0].text == "Extract error:  \"device\" property has unknown device type: transistor");
  cell.properties().put("device", std::string("mos nfet 0 0 x 2"));  // a unique prefix, then a bad coordinate
  p = extract::device_property(cell, style);
  REQUIRE(p.messages.size() == 1);
  CHECK(p.messages[0].text == "Extract error:  \"device mosfet\" property syntax error");
}
