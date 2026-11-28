// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The units a `.ext` file's `style` record selects (EFread.c SCALE and
// EXT_STYLE, ExtCompareStyle): the style as it is in use, micron-converted
// and scaled to the grid. The PDK rc files run `scalegrid 1 2` before the
// technology loads, so their styles' `lambda 1` becomes 0.5 and the
// standard cells are extracted with `scale 1000 1 0.5`. Regression
// (NETLIST-PDK-*, task 17.4): the reads took the parsed style's units (1),
// so locScale was 0.5 and the area/perimeter values were rounded to half
// resolution and the sim units line read 1. The values below are Magic's
// arithmetic on the sg13g2_a21o_1 terminal values (6298, 235) of the
// captured NETLIST-PDK-sg13g2_stdcell case (ad=0.15745p pd=1.175u w=0.74u).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "spice_fixture.hpp"

using namespace spice_test;

namespace {

constexpr const char* kCell =
    "timestamp 0\nversion 8.3\ntech nltest\nstyle nltest\nscale 1000 1 0.5\nresistclasses 1000 5000 100\n"
    "node \"VSS\" 0 0 0 0 ndiff 6298 235 0 0 0 0\n"
    "node \"X\" 0 0 300 0 ndiff 9404 432 0 0 0 0\n"
    "node \"G\" 0 0 100 0 poly 0 0 0 0 0 0\n"
    "device mosfet nfet 253 140 279 288 3848 400 \"VSS\" \"G\" 52 0 \"X\" 148 0 \"VSS\" 148 0\n";

struct GridCase : Session {
  explicit GridCase(bool magic_compat) {
    write("nltest.tech", golden("NL-FLAT-06/input/nltest.tech"));
    write("a21o.ext", kCell);
    REQUIRE(hefesto::settings::set(project, "magic-compat", magic_compat ? "on" : "off").ok());
    REQUIRE(run("scalegrid 1 2").ok());
    REQUIRE(run("tech load /w/nltest -noprompt").ok());
    REQUIRE(run("ext2spice scale off").ok());
  }

  void write(const std::string& name, const std::string& text) {
    REQUIRE(services.files().write_atomic(hefesto::platform::Path{"/w/" + name}, text).ok());
  }

  std::string netlist(std::string_view command, const std::string& file) {
    const auto r = run(command);
    INFO(take_printed());
    REQUIRE(r.ok());
    auto text = services.files().read(hefesto::platform::Path{file});
    REQUIRE(text.ok());
    return text.value();
  }
};

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void check_spice(bool magic_compat) {
  GridCase c{magic_compat};
  const std::string text = c.netlist("ext2spice -p /w a21o", "a21o.spice");
  INFO(text);
  CHECK(has(text, "w=0.74u"));
  CHECK(has(text, "ad=0.15745p pd=1.175u"));
  CHECK(has(text, "as=0.2351p ps=2.16u"));
}

void check_sim(bool magic_compat) {
  GridCase c{magic_compat};
  const std::string text = c.netlist("ext2sim -p /w a21o", "a21o.sim");
  INFO(text);
  CHECK(text.starts_with("| units: 0.5 tech: nltest format: MIT\n"));
  CHECK(has(text, "s=9404,432 d=6298,235"));
}

}  // namespace

TEST_CASE("A grid-scaled style's units keep a scale 0.5 file's area and perimeter with magic-compat ON",
          "[cap:ext-flattening-read][req:Scale record]") {
  check_spice(true);
}

TEST_CASE("A grid-scaled style's units keep a scale 0.5 file's area and perimeter with magic-compat OFF",
          "[cap:ext-flattening-read][req:Scale record]") {
  check_spice(false);
}

TEST_CASE("ext2sim writes the grid-scaled style's units with magic-compat ON",
          "[cap:netlist-sim][req:File structure and order]") {
  check_sim(true);
}
