// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF read of routes through vias that a LEF file defined (openspec/changes/
// m6-lef-def-oasis tasks.md 5.4, 7.3, 7.5; round-3 integration): `lef read`
// fills the session's via table, and `def read`, through the shared `lef`/
// `def` dispatcher, paints those vias where a route names them.
//
// Owner: INTEGRATION (cross-workstream test of LEFR, LEFVIA and DEFR).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "def_fixture.hpp"

namespace def_read_test {
namespace {

// No output style: LEF and DEF numbers are internal units (s = 1, oscale = 1).
constexpr std::string_view kVias =
    "VERSION 5.7 ;\n"
    "VIA V12 DEFAULT\n"
    "  LAYER met1 ;\n"
    "    RECT -100 -100 100 100 ;\n"
    "  LAYER via1 ;\n"
    "    RECT -50 -50 50 50 ;\n"
    "  LAYER met2 ;\n"
    "    RECT -120 -80 120 80 ;\n"
    "END V12\n"
    "END LIBRARY\n";

constexpr std::string_view kTop =
    "DESIGN top ;\n"
    "NETS 1 ;\n"
    "- n + ROUTED met1 ( 1000 1000 ) V12 ;\n"
    "END NETS\n"
    "END DESIGN\n";

TEST_CASE("def read paints a via that lef read defined",
          "[cap:def-read][req:Rectangle-based DEF vias][cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    s.editor.command_point = cmd::PointerState{};  // lef read needs a window or a point
    s.file("/t/vias.lef", std::string{kVias});
    s.file("/t/top.def", std::string{kTop});
    REQUIRE(s.run("lef read /t/vias.lef").ok());
    REQUIRE(s.editor.lefdef.vias.find("V12") != nullptr);
    REQUIRE(s.run("def read /t/top.def").ok());
    CHECK(s.extent("top", "metal1") == db::Rect{900, 900, 1100, 1100});
    CHECK(s.extent("top", "m2c") == db::Rect{950, 950, 1050, 1050});
    CHECK(s.extent("top", "metal2") == db::Rect{880, 920, 1120, 1080});
  }
}

TEST_CASE("a DEF VIAS entry overrides a via that lef read defined",
          "[cap:def-read][req:Via redefinition][cap:lef-read][req:Via redefinition]") {
  Session s;
  s.editor.command_point = cmd::PointerState{};
  s.file("/t/vias.lef", std::string{kVias});
  s.file("/t/top.def",
         "DESIGN top ;\nVIAS 1 ;\n- V12 + RECT met2 ( -300 -300 ) ( 300 300 ) ;\nEND VIAS\n"
         "NETS 1 ;\n- n + ROUTED met1 ( 1000 1000 ) V12 ;\nEND NETS\nEND DESIGN\n");
  REQUIRE(s.run("lef read /t/vias.lef").ok());
  const auto result = s.run("def read /t/top.def");
  REQUIRE(result.ok());
  bool redefined = false;
  for (const auto& d : result.diagnostics) {
    redefined = redefined || d.text.ends_with("Composite via \"V12\" redefined.");
  }
  CHECK(redefined);
  CHECK(s.painted("top", "m2c").empty());
  CHECK(s.extent("top", "metal2") == db::Rect{700, 700, 1300, 1300});
}

}  // namespace
}  // namespace def_read_test
