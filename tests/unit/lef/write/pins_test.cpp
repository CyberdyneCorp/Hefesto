// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-write: macro geometry, the boundary and header lines, pins, antenna
// areas and the PORT records (tasks.md 6.4, 6.5; Q-LEFDEF-36, 39, 40, 44,
// 46, 50, 52, 53).
//
// Owner: LEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "hefesto/app/settings_layers.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

using db::PortClass;
using db::PortShape;
using db::PortUse;

TEST_CASE("subcell paint is flattened into the macro", "[cap:lef-write][req:Macro source geometry and layer masks]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{0, 0, 10, 10});
  db::CellDef& buf = s.cell("buf");
  s.place(buf, inv);
  s.place(buf, inv, db::Transform{1, 0, 20, 0, 1, 0});
  s.label(buf, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0, PortClass::input});
  const std::string text = s.macro(buf);
  CHECK(block(text, "  PIN A", "  END A") ==
        "  PIN A\n    DIRECTION INPUT ;\n    PORT\n      LAYER met1 ;\n        RECT 0.000 0.000 0.100 0.100 ;\n"
        "    END\n  END A\n");
  CHECK(block(text, "  OBS", "  END") == "  OBS\n      LAYER met1 ;\n        RECT 0.200 0.000 0.300 0.100 ;\n  END\n");
}

TEST_CASE("-nomaster drops masterslice geometry unless a port sits on it",
          "[cap:lef-write][req:Macro source geometry and layer masks]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "nwell", db::Rect{0, 0, 100, 50});
  s.paint(inv, "metal1", db::Rect{0, 0, 10, 10});
  CHECK(s.macro(inv).find("LAYER nwell ;") != std::string::npos);
  LefWriteOptions o;
  o.do_master = false;
  CHECK(s.macro(inv, o).find("LAYER nwell ;") == std::string::npos);
  s.label(inv, Port{"VPB", "nwell", db::Rect{0, 0, 100, 50}, 0});
  CHECK(s.macro(inv, o).find("  PIN VPB\n    PORT\n      LAYER nwell ;\n") != std::string::npos);
}

TEST_CASE("the type sets follow the lef section and the extraction style",
          "[cap:lef-write][req:Macro source geometry and layer masks]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  MacroContext ctx;
  ctx.tech = s.editor.technology.get();
  ctx.layers = &s.editor.lefdef.layers;
  MacroBuilder without_style(ctx, inv, MacroOptions{});
  without_style.type_sets();
  // No extraction style: G and D are empty (Q-LEFDEF-40), and so is W.
  CHECK(without_style.sets().gate.empty());
  CHECK(without_style.sets().diffusion.empty());
  CHECK(without_style.sets().well.empty());
  CHECK(without_style.sets().route.test(s.type("metal1")));
  CHECK(without_style.sets().route.test(s.type("obsm1")));
  CHECK(without_style.sets().route.test(s.type("poly")));  // a residue of the licon contact
  CHECK_FALSE(without_style.sets().route.test(s.type("bnd")));
  CHECK(without_style.sets().hide == (tech::TypeMask::of(s.type("metal1")) | tech::TypeMask::of(s.type("metal2"))));
  CHECK(without_style.sets().boundary == tech::TypeMask::of(s.type("bnd")));
  diag::NullSink quiet;
  const extract::StyleView style = extract::StyleView::from_technology(*s.editor.technology, quiet);
  ctx.style = &style;
  MacroBuilder with_style(ctx, inv, MacroOptions{});
  with_style.type_sets();
  CHECK(with_style.sets().gate == tech::TypeMask::of(s.type("nfet")));
  CHECK(with_style.sets().diffusion == tech::TypeMask::of(s.type("ndiff")));
  CHECK(with_style.sets().well == tech::TypeMask::of(s.type("nwell")));  // pwell is a substrate type
  CHECK(with_style.sets().master_planes == tech::plane_bit(*s.editor.technology->home_plane(s.type("nwell"))));
}

TEST_CASE("FIXED_BBOX overrides the boundary", "[cap:lef-write][req:Macro boundary determination]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{-5, 0, 105, 200});
  inv.flags().fixed_bbox = true;
  s.property(inv, "FIXED_BBOX", db::DimensionList{{0, 0, 100, 200}});
  const std::string text = s.macro(inv);
  CHECK(text.find("  ORIGIN 0.000 0.000 ;\n  SIZE 1.000 BY 2.000 ;\n") != std::string::npos);
}

TEST_CASE("the boundary layer's extent is the boundary, and an unpainted one falls back to the bounding box",
          "[cap:lef-write][req:Macro boundary determination]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{-30, 0, 10, 10});
  s.paint(inv, "bnd", db::Rect{-20, -10, 80, 190});
  CHECK(s.macro(inv).find("  ORIGIN 0.200 0.100 ;\n  SIZE 1.000 BY 2.000 ;\n") != std::string::npos);
  db::CellDef& bare = s.cell("bare");
  s.paint(bare, "metal1", db::Rect{0, 0, 50, 20});
  // Q-LEFDEF-53: the boundary set is not empty but holds no paint.
  CHECK(s.macro(bare).find("  ORIGIN 0.000 0.000 ;\n  SIZE 0.500 BY 0.200 ;\n") != std::string::npos);
}

TEST_CASE("the macro header lines", "[cap:lef-write][req:Macro header lines]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{-20, -10, 80, 190});
  const std::string text = s.macro(inv);
  CHECK(text.find("MACRO inv\n  CLASS BLOCK ;\n  FOREIGN inv ;\n  ORIGIN 0.200 0.100 ;\n  SIZE 1.000 BY 2.000 ;\n") !=
        std::string::npos);
  CHECK(text.find("SYMMETRY") == std::string::npos);
  CHECK(text.find("  SITE") == std::string::npos);
}

TEST_CASE("pins are written in port-index order", "[cap:lef-write][req:Pin enumeration and ordering]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.paint(c, "metal1", db::Rect{20, 0, 30, 10});
  s.paint(c, "metal1", db::Rect{40, 0, 50, 10});
  s.label(c, Port{"Y", "metal1", db::Rect{20, 0, 30, 10}, 1});
  s.label(c, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0});
  s.label(c, Port{"VDD", "metal1", db::Rect{40, 0, 50, 10}, 2});
  const std::string text = s.macro(c);
  const std::size_t a = text.find("  PIN A\n");
  const std::size_t y = text.find("  PIN Y\n");
  const std::size_t v = text.find("  PIN VDD\n");
  REQUIRE(a != std::string::npos);
  REQUIRE(y != std::string::npos);
  REQUIRE(v != std::string::npos);
  CHECK(a < y);
  CHECK(y < v);
}

TEST_CASE("labels of one index make one PIN, with a PORT per unconnected label",
          "[cap:lef-write][req:Pin enumeration and ordering]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 100, 10});
  s.paint(c, "metal1", db::Rect{0, 90, 100, 100});
  s.label(c, Port{"VDD", "metal1", db::Rect{0, 0, 10, 10}, 2});
  s.label(c, Port{"VDD", "metal1", db::Rect{0, 90, 10, 100}, 2});
  const std::string pin = block(s.macro(c), "  PIN VDD", "  END VDD");
  CHECK(pin == "  PIN VDD\n    PORT\n      LAYER met1 ;\n        RECT 0.000 0.000 1.000 0.100 ;\n    END\n"
               "    PORT\n      LAYER met1 ;\n        RECT 0.000 0.900 1.000 1.000 ;\n    END\n  END VDD\n");

  Session t;
  db::CellDef& d = t.cell("d");
  t.paint(d, "metal1", db::Rect{0, 0, 100, 10});
  t.label(d, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0});
  t.label(d, Port{"A", "metal1", db::Rect{90, 0, 100, 10}, 0});
  CHECK(block(t.macro(d), "  PIN A", "  END A") ==
        "  PIN A\n    PORT\n      LAYER met1 ;\n        RECT 0.000 0.000 1.000 0.100 ;\n    END\n  END A\n");
}

TEST_CASE("a shared index with another text is reported and merged into one PIN",
          "[cap:lef-write][req:Pin enumeration and ordering]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.paint(c, "metal1", db::Rect{20, 0, 30, 10});
  s.label(c, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0});
  s.label(c, Port{"B", "metal1", db::Rect{20, 0, 30, 10}, 0});
  const std::string text = s.macro(c);
  CHECK(text.find("  PIN B") == std::string::npos);
  CHECK(block(text, "  PIN A", "  END A").find("RECT 0.200 0.000 0.300 0.100") != std::string::npos);
  CHECK(s.texts("LEF-WRITE-004") == std::vector<std::string>{"Index 0 is used for ports \"A\" and \"B\""});
}

TEST_CASE("pin header records", "[cap:lef-write][req:Pin header records]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.label(c, Port{"Z", "metal1", db::Rect{0, 0, 10, 10}, 0, PortClass::tristate, PortUse::clock, PortShape::ring});
  CHECK(s.macro(c).find("  PIN Z\n    DIRECTION OUTPUT TRISTATE ;\n    USE CLOCK ;\n    SHAPE RING ;\n    PORT\n") !=
        std::string::npos);
}

TEST_CASE("reset, scan and tie-off uses write Magic's empty keyword only with magic-compat ON",
          "[cap:lef-write][req:Pin header records]") {
  for (const bool on : {true, false}) {
    for (const PortUse use : {PortUse::reset, PortUse::scan, PortUse::tieoff}) {
      Session s(on);
      db::CellDef& c = s.cell("c");
      s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
      s.label(c, Port{"RN", "metal1", db::Rect{0, 0, 10, 10}, 0, PortClass::input, use});
      CHECK(s.macro(c).find(on ? "    USE  ;\n" : "    USE SIGNAL ;\n") != std::string::npos);
    }
  }
}

// metal1 pin A over pc onto poly, which runs into a 5 x 6 gate.
db::CellDef& gate_cell(Session& s, std::string_view text = "A") {
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.paint(c, "pc", db::Rect{0, 0, 4, 4});
  s.paint(c, "poly", db::Rect{4, 0, 20, 4});
  s.paint(c, "nfet", db::Rect{20, 0, 25, 6});
  s.label(c, Port{std::string{text}, "metal1", db::Rect{0, 0, 10, 10}, 0, PortClass::input});
  return c;
}

TEST_CASE("the gate area of a pin's net", "[cap:lef-write][req:Antenna areas]") {
  Session s;
  db::CellDef& c = gate_cell(s);
  const std::string text = s.macro(c);
  CHECK(text.find("    DIRECTION INPUT ;\n    ANTENNAGATEAREA 0.003000 ;\n    PORT\n") != std::string::npos);
  CHECK(text.find("ANTENNADIFFAREA") == std::string::npos);
}

TEST_CASE("lef nocheck nets get no antenna lines", "[cap:lef-write][req:Antenna areas]") {
  for (const auto* names : {"A", "*"}) {
    Session s;
    s.editor.command_point = cmd::PointerState{};  // a script: no window
    REQUIRE(s.run(std::string{"lef nocheck "} + names).ok());
    db::CellDef& c = gate_cell(s);
    CHECK(s.macro(c).find("ANTENNA") == std::string::npos);
  }
}

TEST_CASE("the diffusion area of a pin's net", "[cap:lef-write][req:Antenna areas]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.paint(c, "pc", db::Rect{0, 0, 4, 4});
  s.paint(c, "poly", db::Rect{4, 0, 20, 4});
  s.paint(c, "ndiff", db::Rect{20, 0, 30, 4});
  s.label(c, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0});
  const std::string text = s.macro(c);
  CHECK(text.find("  PIN A\n    ANTENNADIFFAREA 0.004000 ;\n    PORT\n") != std::string::npos);
  CHECK(text.find("ANTENNAGATEAREA") == std::string::npos);
}

TEST_CASE("a pin named like the ground net is a ground rail without antenna lines",
          "[cap:lef-write][req:Power and ground pin detection]") {
  Session s;
  s.editor.settings.set(app::SettingLayer::session, app::kGnd, "VSS");
  db::CellDef& c = gate_cell(s, "VSS");
  const std::string text = s.macro(c);
  CHECK(text.find("  PIN VSS\n    DIRECTION INPUT ;\n    USE GROUND ;\n    PORT\n") != std::string::npos);
  CHECK(text.find("ANTENNA") == std::string::npos);
}

TEST_CASE("the power names come from the global nets setting", "[cap:lef-write][req:Power-name source in Magic]") {
  Session s;
  s.editor.settings.set(app::SettingLayer::session, app::kVdd, "vccd1");
  s.editor.settings.set(app::SettingLayer::session, app::kGnd, "vccd1");
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.label(c, Port{"vccd1", "metal1", db::Rect{0, 0, 10, 10}, 0});
  CHECK(s.macro(c).find("  PIN vccd1\n    USE POWER ;\n    USE GROUND ;\n    PORT\n") != std::string::npos);
}

TEST_CASE("the whole net is the default pin and leaves no obstruction", "[cap:lef-write][req:Default pin geometry]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 100});
  s.paint(c, "metal2", db::Rect{0, 90, 100, 100});
  s.paint(c, "m2c", db::Rect{0, 90, 10, 100});
  s.label(c, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0});
  const std::string text = s.macro(c);
  CHECK(block(text, "  PIN A", "  END A") ==
        "  PIN A\n    PORT\n      LAYER met1 ;\n        RECT 0.000 0.000 0.100 1.000 ;\n"
        "      LAYER via1 ;\n        RECT 0.000 0.900 0.100 1.000 ;\n"
        "      LAYER met2 ;\n        RECT 0.000 0.900 1.000 1.000 ;\n    END\n  END A\n");
  CHECK(text.find("  OBS\n") == std::string::npos);
}

TEST_CASE("a via in a pin is one cut record of the whole contact area", "[cap:lef-write][req:Contact cut records]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 20, 100});
  s.paint(c, "m2c", db::Rect{0, 0, 20, 20});
  s.label(c, Port{"A", "metal1", db::Rect{0, 50, 20, 60}, 0});
  CHECK(block(s.macro(c), "    PORT", "    END") ==
        "    PORT\n      LAYER met1 ;\n        RECT 0.000 0.000 0.200 1.000 ;\n"
        "      LAYER via1 ;\n        RECT 0.000 0.000 0.200 0.200 ;\n"
        "      LAYER met2 ;\n        RECT 0.000 0.000 0.200 0.200 ;\n    END\n");
}

TEST_CASE("a cut below the pin's first routing record is dropped only with magic-compat ON",
          "[cap:lef-write][req:Contact cut records]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& c = gate_cell(s);
    const std::string port = block(s.macro(c), "    PORT", "    END");
    CHECK((port.find("LAYER licon ;") != std::string::npos) == !on);
    CHECK(port.find("LAYER met1 ;") != std::string::npos);
  }
}

TEST_CASE("a port without LEF geometry writes no PIN and is reported", "[cap:lef-write][req:PORT blocks]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "poly", db::Rect{0, 0, 10, 10});
  s.paint(c, "metal1", db::Rect{20, 0, 30, 10});
  s.label(c, Port{"X", "poly", db::Rect{0, 0, 10, 10}, 0});
  const std::string text = s.macro(c);
  CHECK(text.find("PIN X") == std::string::npos);
  CHECK(text.find("END X") == std::string::npos);
  CHECK(s.texts("LEF-WRITE-005") ==
        std::vector<std::string>{"Port \"X\" (index 0) has no geometry on a LEF layer; no PIN is written (Q-LEFDEF-46)"});
}

TEST_CASE("an L shape is written as its maximal horizontal strips", "[cap:lef-write][req:Geometry record emission]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 30, 10});
  s.paint(c, "metal1", db::Rect{0, 10, 10, 40});
  s.label(c, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0});
  CHECK(block(s.macro(c), "    PORT", "    END") ==
        "    PORT\n      LAYER met1 ;\n        RECT 0.000 0.100 0.100 0.400 ;\n        RECT 0.000 0.000 0.300 0.100 ;\n"
        "    END\n");
}

TEST_CASE("a split tile is written as a POLYGON by its right-angle corner",
          "[cap:lef-write][req:Geometry record emission]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.triangle(c, "metal1", db::Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true);  // lower-right corner
  CHECK(s.macro(c).find("      LAYER met1 ;\n        POLYGON 0.100 0.100 0.100 0.000 0.000 0.000 ;\n") !=
        std::string::npos);
  db::CellDef& d = s.cell("d");
  s.triangle(d, "metal1", db::Rect{0, 0, 10, 10}, db::Diagonal::nw_se, true);  // upper-right corner
  CHECK(s.macro(d).find("        POLYGON 0.000 0.100 0.100 0.100 0.100 0.000 ;\n") != std::string::npos);
}

TEST_CASE("a split tile with two routing halves writes the second only with magic-compat OFF",
          "[cap:lef-write][req:Geometry record emission]") {
  for (const bool on : {true, false}) {
    Session s(on, "    routing m1pin met1pin\n");
    db::CellDef& c = s.cell("c");
    s.triangle(c, "metal1", db::Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, false);
    s.triangle(c, "m1pin", db::Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true);
    const std::string obs = block(s.macro(c), "  OBS", "  END");
    CHECK(obs.find("      LAYER met1 ;\n        POLYGON 0.000 0.100 0.100 0.100 0.000 0.000 ;\n") !=
          std::string::npos);
    CHECK((obs.find("      LAYER met1pin ;\n        POLYGON 0.100 0.100 0.100 0.000 0.000 0.000 ;\n") !=
           std::string::npos) == !on);
  }
}

TEST_CASE("unlabelled material is written as OBS", "[cap:lef-write][req:Obstructions without hiding]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal2", db::Rect{0, 50, 100, 60});
  CHECK(block(s.macro(c), "  OBS", "  END") == "  OBS\n      LAYER met2 ;\n        RECT 0.000 0.500 1.000 0.600 ;\n  END\n");
  db::CellDef& empty = s.cell("empty");
  s.label(empty, Port{"note", "metal2", db::Rect{0, 0, 0, 0}, std::nullopt});
  CHECK(s.macro(empty).find("  OBS") == std::string::npos);
}

TEST_CASE("names are written verbatim and reported when they break LEF tokens",
          "[cap:lef-write][req:Names written verbatim]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.label(c, Port{"a#1", "metal1", db::Rect{0, 0, 10, 10}, 0});
  s.label(c, Port{"b-*$", "metal1", db::Rect{0, 0, 10, 10}, 1});
  const std::string text = s.macro(c);
  CHECK(text.find("  PIN a#1\n") != std::string::npos);
  CHECK(text.find("  END a#1\n") != std::string::npos);
  CHECK(s.texts("LEF-WRITE-008").size() == 1);
  CHECK(s.texts("LEF-WRITE-008").front().find("a#1") != std::string::npos);
}

}  // namespace
}  // namespace hefesto::lef::write_test
