// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF read and LEF write together (openspec/changes/m6-lef-def-oasis tasks.md
// 5.4, 6.4; round-3 integration): a macro read by `lef read` is written back
// by `lef write` as the same text, and a written file read into a new
// session is written again unchanged, with either magic-compat setting.
//
// Owner: INTEGRATION (cross-workstream test of LEFR and LEFW).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

// lef-write "LEF file grammar"'s annotated example, as lef read takes it.
constexpr std::string_view kInv =
    "VERSION 5.7 ;\n"
    "  NOWIREEXTENSIONATPIN ON ;\n"
    "  DIVIDERCHAR \"/\" ;\n"
    "  BUSBITCHARS \"[]\" ;\n"
    "MACRO inv\n"
    "  CLASS CORE ;\n"
    "  FOREIGN inv ;\n"
    "  ORIGIN 0.000 0.000 ;\n"
    "  SIZE 1.000 BY 2.000 ;\n"
    "  SYMMETRY X Y ;\n"
    "  SITE unit ;\n"
    "  PIN A\n"
    "    DIRECTION INPUT ;\n"
    "    USE SIGNAL ;\n"
    "    PORT\n"
    "      LAYER met1 ;\n"
    "        RECT 0.100 0.500 0.300 1.500 ;\n"
    "    END\n"
    "  END A\n"
    "  PIN Y\n"
    "    DIRECTION OUTPUT ;\n"
    "    PORT\n"
    "      LAYER met1 ;\n"
    "        RECT 0.700 0.500 0.900 1.500 ;\n"
    "    END\n"
    "  END Y\n"
    "  OBS\n"
    "      LAYER met1 ;\n"
    "        RECT 0.400 0.000 0.600 0.200 ;\n"
    "  END\n"
    "END inv\n"
    "END LIBRARY\n"
    "\n";

/// `lef read` of `text` (as /in/<name>.lef), then `lef write` of cell `name`
/// as the edit root; returns the written text.
std::string read_then_write(bool magic_compat, std::string_view text, const std::string& name) {
  Session s(magic_compat);
  s.editor.command_point = cmd::PointerState{};
  s.file("/in/" + name + ".lef", std::string{text});
  REQUIRE(s.run("lef read /in/" + name + ".lef").ok());
  db::CellDef* def = s.editor.cells.find(name);
  REQUIRE(def != nullptr);
  return s.macro(*def);
}

TEST_CASE("a macro read by lef read is written back unchanged by lef write",
          "[cap:lef-write][req:LEF file grammar][cap:lef-read][req:MACRO and SITE cell creation]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    CHECK(read_then_write(on, kInv, "inv") == kInv);
  }
}

TEST_CASE("a file lef write wrote reads back to the same file",
          "[cap:lef-write][req:LEF file grammar][cap:lef-read][req:PORT and OBS geometry]") {
  // A macro with a two-port pin, an OBS on a second layer and properties:
  // the first write is a fixed point of read-then-write.
  constexpr std::string_view kCells =
      "VERSION 5.7 ;\n"
      "MACRO buf\n"
      "  CLASS CORE ;\n"
      "  SIZE 2 BY 2 ;\n"
      "  PROPERTY k1 v1 ;\n"
      "  PIN Z\n"
      "    DIRECTION OUTPUT ;\n"
      "    USE POWER ;\n"
      "    PORT\n"
      "      LAYER met1 ;\n"
      "        RECT 0.1 0.1 0.5 0.5 ;\n"
      "    END\n"
      "    PORT\n"
      "      LAYER met2 ;\n"
      "        RECT 1.1 1.1 1.5 1.9 ;\n"
      "    END\n"
      "  END Z\n"
      "  OBS\n"
      "      LAYER met2 ;\n"
      "        RECT 0.0 1.0 0.8 1.2 ;\n"
      "  END\n"
      "END buf\n"
      "END LIBRARY\n";
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    const std::string first = read_then_write(on, kCells, "buf");
    CHECK(first.find("  PROPERTY k1 v1 ;\n") != std::string::npos);
    CHECK(first.find("    USE POWER ;\n") != std::string::npos);
    CHECK(first.find("      LAYER met2 ;\n        RECT 1.100 1.100 1.500 1.900 ;\n") != std::string::npos);
    CHECK(read_then_write(on, first, "buf") == first);
  }
}

}  // namespace
}  // namespace hefesto::lef::write_test
