// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-write: the output scale, the LEF database units and lefPrint's
// float32 formatting (tasks.md 6.2; Q-LEFDEF-34, Q-LEFDEF-45).
//
// Owner: LEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "hefesto/lef/write/number.hpp"
#include "hefesto/mask/style/style.hpp"
#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

mask::OutputStyle style_of(std::int32_t factor, std::int32_t expander, std::int32_t limit, bool angstroms) {
  mask::OutputStyle style;
  style.scale.scale_factor = factor;
  style.scale.expander = expander;
  style.grid_limit = limit;
  if (angstroms) {
    style.options |= mask::style_option::angstroms;
  }
  return style;
}

TEST_CASE("the output scale and the database units come from the output style",
          "[cap:lef-write][req:Output scale and LEF database units]") {
  CHECK(output_scale(nullptr) == 1.0F);
  CHECK(database_units(nullptr) == LefUnits{1000});
  const mask::OutputStyle nm = style_of(5, 10, 5, false);
  CHECK(output_scale(&nm) == 50.0F / 10000.0F);
  CHECK(database_units(&nm) == LefUnits{200});
  const mask::OutputStyle ang = style_of(5, 100, 0, true);
  CHECK(database_units(&ang) == LefUnits{10000});
  const auto units_of = [](const mask::OutputStyle& st) { return database_units(&st); };
  CHECK(units_of(style_of(1, 1, 10, true)) == LefUnits{1000});
  CHECK(units_of(style_of(1, 1, 1, false)) == LefUnits{1000});
  CHECK(units_of(style_of(1, 10, 5, false)) == LefUnits{200});
  CHECK(units_of(style_of(1, 1, 3, false)) == LefUnits{1000});  // only 1, 5 and 10 divide
  CHECK(LefUnits{}.database == 1000);  // Magic's LEFdbUnits before any header
}

TEST_CASE("distances round half away from zero in float32 to the units' grid",
          "[cap:lef-write][req:Distance number formatting]") {
  CHECK(format_distance(0.005F * 23.0F, LefUnits{1000}) == "0.115");
  CHECK(format_distance(0.01F * 123.0F, LefUnits{100}) == "1.23");
  // 0.0005f * 3 is exactly 0.0015f, which rounds up (pinned with numpy).
  CHECK(format_distance(0.0005F * 3.0F, LefUnits{1000}) == "0.002");
  CHECK(format_distance(0.0005F * -3.0F, LefUnits{1000}) == "-0.002");
  CHECK(format_distance(0.0F, LefUnits{1000}) == "0.000");
  // A value in (-1/(2U), 0) prints as -0.000 (Q-LEFDEF-45, both modes).
  CHECK(format_distance(-0.0004F, LefUnits{1000}) == "-0.000");
  CHECK(format_distance(1.23456F, LefUnits{200}) == "1.235");
  CHECK(format_distance(1.23456F, LefUnits{2000}) == "1.2345");  // the half grid of 2000
  CHECK(format_distance(1.23456F, LefUnits{10000}) == "1.2346");
  CHECK(format_distance(1.234567F, LefUnits{20000}) == "1.23455");
  // Any other unit takes the 100000 path (double product, float truncf).
  CHECK(format_distance(0.123456F, LefUnits{5}) == "0.12346");
  CHECK(format_real(0.0016) == "0.001600");
}

TEST_CASE("a header-less write keeps the session's units with magic-compat ON and recomputes them OFF",
          "[cap:lef-write][req:Output scale and LEF database units]") {
  for (const bool on : {true, false}) {
    Session s(on);
    REQUIRE(s.run("cif ostyle ang").ok());  // oscale 0.0005, U 10000
    db::CellDef& inv = s.cell("inv");
    s.paint(inv, "metal1", db::Rect{0, 0, 20, 20});
    s.editor.edit_root = &s.cell("top");
    REQUIRE(s.write(inv, "inv").ok());
    const std::string text = s.read("inv.lef");
    CHECK(text.find(on ? "  SIZE 0.010 BY 0.010 ;\n" : "  SIZE 0.0100 BY 0.0100 ;\n") != std::string::npos);
    CHECK(s.editor.lefdef.units == LefUnits{1000});
  }
}

TEST_CASE("a header sets the session's units for later header-less writes with magic-compat ON",
          "[cap:lef-write][req:Single-cell file layout]") {
  Session s;
  REQUIRE(s.run("cif ostyle ang").ok());
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{0, 0, 20, 20});
  CHECK(s.macro(inv).find("  SIZE 0.0100 BY 0.0100 ;\n") != std::string::npos);
  CHECK(s.editor.lefdef.units == LefUnits{10000});
  REQUIRE(s.run("cif ostyle lef").ok());  // oscale 0.01, U recomputed only by a header
  s.editor.edit_root = &s.cell("top");
  REQUIRE(s.write(inv, "inv").ok());
  CHECK(s.read("inv.lef").find("  SIZE 0.2000 BY 0.2000 ;\n") != std::string::npos);
}

TEST_CASE("Magic prints the progress messages of a write", "[cap:lef-write][req:Progress messages]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "metal1", db::Rect{0, 0, 20, 20});
  (void)s.macro(inv);
  CHECK(s.texts("LEF-WRITE-001") == std::vector<std::string>{"Generating LEF output inv.lef for cell inv:",
                                                               "Diagnostic:  Write LEF header for cell inv",
                                                               "Diagnostic:  Writing LEF output for cell inv",
                                                               "Diagnostic:  Scale value is 0.010000"});
}

}  // namespace
}  // namespace hefesto::lef::write_test
