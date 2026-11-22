// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-write: the -tech LAYER blocks and the magic-type to LEF-name map
// (tasks.md 6.3; Q-LEFDEF-31, 32).
//
// Owner: LEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

std::string tech_output(Session& s) {
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{0, 0, 10, 10});
  LefWriteOptions o;
  o.tech = true;
  return s.macro(inv, o);
}

LayerRecord& record(Session& s, std::string_view name) {
  LayerRecord* r = s.editor.lefdef.layers.find_mutable(name);
  REQUIRE(r != nullptr);
  return *r;
}

TEST_CASE("a route record writes its stored width and its direction", "[cap:lef-write][req:Technology layers]") {
  Session s;
  LayerRecord& met1 = record(s, "met1");
  met1.width = 14;
  met1.spacing = -1;
  met1.pitch = 0;
  met1.direction = RouteDirection::horizontal;
  CHECK(block(tech_output(s), "LAYER met1", "END met1") ==
        "LAYER met1\n  TYPE ROUTING ;\n  WIDTH 0.140000 ;\n  DIRECTION HORIZONTAL ;\nEND met1\n");
}

TEST_CASE("an unset route on a 1:1 grid writes no WIDTH only with magic-compat ON",
          "[cap:lef-write][req:Technology layers]") {
  for (const bool on : {true, false}) {
    Session s(on);
    const std::string expected = on ? "LAYER met1\n  TYPE ROUTING ;\nEND met1\n"
                                    : "LAYER met1\n  TYPE ROUTING ;\n  WIDTH 0.030000 ;\n  SPACING 0.030000 ;\n"
                                      "  DIRECTION HORIZONTAL ;\nEND met1\n";
    CHECK(block(tech_output(s), "LAYER met1", "END met1") == expected);
  }
}

TEST_CASE("a cut writes CUT AREA with magic-compat ON and WIDTH with it OFF",
          "[cap:lef-write][req:Technology layers]") {
  for (const bool on : {true, false}) {
    Session s(on);
    record(s, "licon").contact_area = db::Rect{-2, -2, 2, 2};  // a 2 x 2 cut in half units
    const std::string text = tech_output(s);
    CHECK(block(text, "LAYER licon", "END licon") ==
          (on ? "LAYER licon\n  TYPE CUT ;\n  CUT AREA 0.001600 ;\nEND licon\n"
              : "LAYER licon\n  TYPE CUT ;\n  WIDTH 0.020000 ;\nEND licon\n"));
  }
}

TEST_CASE("masterslice, obstruction-only and bound records", "[cap:lef-write][req:Technology layers]") {
  Session s(true, "    obstruction obsm2 met2obs\n");
  const std::string text = tech_output(s);
  CHECK(block(text, "LAYER nwell", "END nwell") == "LAYER nwell\n  TYPE MASTERSLICE ;\nEND nwell\n");
  CHECK(text.find("LAYER met2obs") == std::string::npos);
  CHECK(text.find("LAYER prb") == std::string::npos);
  // Each record once, under its canonical name, each block followed by an
  // empty line.
  CHECK(text.find("LAYER met1\n") == text.rfind("LAYER met1\n"));
  CHECK(text.find("END nwell\n\n") != std::string::npos);
}

TEST_CASE("a via record of a LEF read is a LAYER block only with magic-compat ON",
          "[cap:lef-write][req:Technology layers]") {
  for (const bool on : {true, false}) {
    Session s(on);
    LayerRecord via;
    via.layer_class = LayerClass::cut;
    via.type = s.type("m2c");
    via.canonical_name = "M1M2_PR";
    (void)s.editor.lefdef.layers.add("M1M2_PR", via);
    (void)s.editor.lefdef.vias.insert("M1M2_PR", ViaDefinition{});
    CHECK((tech_output(s).find("LAYER M1M2_PR\n  TYPE CUT ;\n") != std::string::npos) == on);
  }
}

// Regression: only layer-table records were written, so a via a LEF or DEF
// read made (a via-table record whose key has no layer record) never gave
// a LAYER block, although in Magic it is a LefInfo record like any other
// and is written with magic-compat ON (Q-LEFDEF-127; found with
// MASK-OS-016's first input, tests/unit/lef/read/magic_crash_test.cpp).
TEST_CASE("a via of a LEF read with no layer record is a LAYER block only with magic-compat ON",
          "[cap:lef-write][req:Technology layers]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Session s(on);
    ViaDefinition via;
    via.type = s.type("m2c");
    via.area = db::Rect{-10, -10, 10, 10};
    via.canonical_name = "VX1";
    s.editor.lefdef.layers.note_key("VX1");
    (void)s.editor.lefdef.vias.insert("VX1", via);
    ViaDefinition untyped;  // no geometry read: Magic's type -1, skipped
    untyped.canonical_name = "VX2";
    s.editor.lefdef.layers.note_key("VX2");
    (void)s.editor.lefdef.vias.insert("VX2", untyped);
    const std::string text = tech_output(s);
    // CUT AREA is 4x the area in half units (Q-LEFDEF-32): 20 * 20 * oscale^2.
    CHECK((text.find("LAYER VX1\n  TYPE CUT ;\n  CUT AREA ") != std::string::npos) == on);
    CHECK((text.find("END VX1\n\n") != std::string::npos) == on);
    CHECK(text.find("LAYER VX2") == std::string::npos);
  }
}

TEST_CASE("without -tech no LAYER block is written", "[cap:lef-write][req:Technology layers]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{0, 0, 10, 10});
  const std::string text = s.macro(inv);
  CHECK(text.find("LAYER met1\n") == std::string::npos);
  CHECK(text.find("UNITS") == std::string::npos);
}

TEST_CASE("an obstruction type is written under its route layer's name",
          "[cap:lef-write][req:Magic type to LEF layer name]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "obsm1", db::Rect{0, 0, 10, 10});
  const std::string text = s.macro(inv);
  CHECK(text.find("  OBS\n      LAYER met1 ;\n        RECT 0.000 0.000 0.100 0.100 ;\n  END\n") != std::string::npos);
}

TEST_CASE("a contact is written under the name of its first cut record in table order",
          "[cap:lef-write][req:Magic type to LEF layer name]") {
  Session s(false);  // a lone contact's cut is dropped with magic-compat ON (Q-LEFDEF-136)
  LayerRecord via;
  via.layer_class = LayerClass::cut;
  via.type = s.type("m2c");
  via.canonical_name = "M1M2_PR";
  (void)s.editor.lefdef.layers.add("M1M2_PR", via);
  const LayerRecord* first = s.editor.lefdef.layers.magic_to_lef(s.type("m2c"));
  REQUIRE(first != nullptr);
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "m2c", db::Rect{0, 0, 10, 10});
  const std::string text = s.macro(inv);
  CHECK(text.find("      LAYER " + first->canonical_name + " ;\n        RECT 0.000 0.000 0.100 0.100 ;\n") !=
        std::string::npos);
  // A type with no LEF record is never written.
  db::CellDef& poly = s.cell("polyonly");
  s.paint(poly, "poly", db::Rect{0, 0, 10, 10});
  CHECK(s.macro(poly).find("RECT") == std::string::npos);
}

// Regression (found with LEFDEF-RT-sky130_fd_sc_hd and -sg13g2_stdcell in
// round 5): defGetType enumerates all of LefInfo, so a VIA definition read
// from LEF, a ViaTable entry with no layer record, names a cut type when its
// key comes first (Magic wrote `LAYER M4M5_PR_C ;` for via4); lef write
// looked at the layer records only.
TEST_CASE("a VIA definition earlier in the key order names the cut type it holds",
          "[cap:lef-write][req:Magic type to LEF layer name]") {
  bool via_won = false;
  for (const std::string name : {"M1M2_PR", "M1M2_PR_C", "V12", "VIA12", "M1M2", "Via1_XY_s"}) {
    INFO(name);
    Session s(false);  // a lone contact's cut is dropped with magic-compat ON (Q-LEFDEF-136)
    ViaDefinition via;
    via.type = s.type("m2c");
    via.area = db::Rect{-10, -10, 10, 10};
    via.canonical_name = name;
    s.editor.lefdef.layers.note_key(name);  // as begin_via does (Q-LEFDEF-135)
    s.editor.lefdef.vias.insert(name, via);
    std::string first;
    s.editor.lefdef.layers.enumerate_keys([&](std::string_view key, const LayerRecord* record) {
      const bool cut = record != nullptr && record->layer_class == LayerClass::cut && record->type == s.type("m2c");
      if (first.empty() && (key == name || cut)) {
        first = key == name ? name : record->canonical_name;
      }
    });
    via_won = via_won || first == name;
    db::CellDef& inv = s.cell("inv");
    s.paint(inv, "m2c", db::Rect{0, 0, 10, 10});
    CHECK(s.macro(inv).find("      LAYER " + first + " ;\n        RECT 0.000 0.000 0.100 0.100 ;\n") !=
          std::string::npos);
  }
  CHECK(via_won);  // at least one name precedes the cut layer's key
}

}  // namespace
}  // namespace hefesto::lef::write_test
