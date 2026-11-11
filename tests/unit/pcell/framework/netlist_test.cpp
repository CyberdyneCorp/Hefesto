// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability pcell-toolkit: "SPICE netlist to layout seeding", "Seeding a
// subcircuit cell", on tests/data/pcell/framework/netlists/ and the test
// library tpdk.tcl. The seeded cells are saved into a temporary directory.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "framework_fixture.hpp"

using hefesto::test::PcellSession;

namespace {

const std::string kNetlists = std::string{HEFESTO_TEST_DATA_DIR} + "/netlists/";

using hefesto::test::TempDirectory;

std::vector<std::string> texts(const hefesto::cmd::CommandResult& r, std::string_view code) {
  std::vector<std::string> out;
  for (const auto& d : r.diagnostics) {
    if (d.code == code) {
      out.push_back(d.text);
    }
  }
  return out;
}

bool contains(const std::vector<std::string>& v, std::string_view s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

void define_capacitor(PcellSession& s) {
  REQUIRE(s.tcl("proc tpdk::capacitor_defaults {} {return {value 1p}}; "
                "proc tpdk::capacitor_convert {p} {lappend tpdk::calls \"convert $p\"; return $p}; "
                "proc tpdk::capacitor_check {p} {return $p}; "
                "proc tpdk::capacitor_draw {p} {box values 0 0 5 5; paint m2}")
              .ok());
}

}  // namespace

TEST_CASE("Existing layout is skipped", "[cap:pcell-toolkit][req:SPICE netlist to layout seeding]") {
  const TempDirectory tmp;
  PcellSession s;
  define_capacitor(s);
  // inv already has layout (in memory, as `load inv -fail` finds it).
  REQUIRE(s.run("cellname create inv").ok());
  const auto r = s.run("magic::netlist_to_layout " + kNetlists + "seed.spice tpdk");
  REQUIRE(r.ok());
  const auto printed = texts(r, "PCELL-NETLIST-001");
  CHECK(contains(printed, "Creating layout from seed.spice"));
  CHECK(contains(printed, "Subcircuit inv successfully loaded."));
  CHECK(contains(printed, "Cell inv already contains layout. Skipping generation."));
  CHECK(contains(printed, "Subcircuit amp not found. Will generate."));
  CHECK(contains(printed, "Cell amp is empty. Generating initial layout..."));
  CHECK(s.value("cellname list children inv").empty());
}

TEST_CASE("a subcircuit is seeded with its devices", "[cap:pcell-toolkit][req:Seeding a subcircuit cell]") {
  const TempDirectory tmp;
  PcellSession s;
  define_capacitor(s);
  REQUIRE(s.run("magic::netlist_to_layout " + kNetlists + "seed.spice tpdk").ok());
  REQUIRE(s.run("load amp").ok());
  // XR1: magic::gencell tpdk::res XR1 -spice w 2 l {3*1} nf 2 (continuation joined).
  CHECK(s.value("instance list celldef XR1").rfind("res_", 0) == 0);
  const std::string cell = s.value("instance list celldef XR1");
  CHECK(s.value("cellname list property " + cell + " parameters") == "w 2 l 3*1 nf 2 compatible {res res2}");
  // Lines inside a top-level .command ... .endc and testbench sources are skipped.
  CHECK(s.value("instance list celldef XIGNORED").empty());
  CHECK(s.value("instance list celldef V1").empty());
  // The cell is saved.
  CHECK(std::filesystem::exists("amp.mag"));
}

TEST_CASE("Ideal capacitor", "[cap:pcell-toolkit][req:Seeding a subcircuit cell]") {
  const TempDirectory tmp;
  PcellSession s;
  define_capacitor(s);
  REQUIRE(s.tcl("set tpdk::calls {}").ok());
  REQUIRE(s.run("magic::netlist_to_layout " + kNetlists + "seed.spice tpdk").ok());
  CHECK(s.value("lsearch -exact $tpdk::calls {convert value 10f}") != "-1");
  REQUIRE(s.run("load amp").ok());
  CHECK(s.value("instance list celldef C1").rfind("capacitor_", 0) == 0);
}

TEST_CASE("Bus pin name preserved", "[cap:pcell-toolkit][req:Seeding a subcircuit cell]") {
  const TempDirectory tmp;
  PcellSession s;
  if (!s.fonts_loaded) {
    SKIP("Magic's fonts (build/ref/magic/graphics/FreeSans.pt3) are not available");
  }
  define_capacitor(s);
  REQUIRE(s.run("magic::netlist_to_layout " + kNetlists + "seed.spice tpdk").ok());
  REQUIRE(s.run("load amp").ok());
  std::vector<std::string> names;
  for (int i = 0; i < 4; ++i) {
    names.push_back(s.value("port " + std::to_string(i) + " name"));
  }
  CHECK(names == std::vector<std::string>{"in", "out", "vout[0]", "vss"});
}

TEST_CASE("a device that is not a gencell is placed with getcell",
          "[cap:pcell-toolkit][req:Seeding a subcircuit cell]") {
  const TempDirectory tmp;
  PcellSession s;
  define_capacitor(s);
  REQUIRE(s.run("cellname create nosuchdevice").ok());
  REQUIRE(s.run("pushstack nosuchdevice").ok());
  REQUIRE(s.run("box values 0 0 10 10").ok());
  REQUIRE(s.run("paint m1").ok());
  REQUIRE(s.run("popstack").ok());
  REQUIRE(s.run("magic::netlist_to_layout " + kNetlists + "seed.spice tpdk").ok());
  REQUIRE(s.run("load amp").ok());
  CHECK(s.value("instance list celldef XQ") == "nosuchdevice");
  REQUIRE(s.run("select cell XQ").ok());
  CHECK(s.value("array -list count").find("0 0 0 1") != std::string::npos);  // M=2: array 1 2
}

TEST_CASE("top-level elements of a CDL file seed the cell named after the file",
          "[cap:pcell-toolkit][req:SPICE netlist to layout seeding]") {
  const TempDirectory tmp;
  PcellSession s;
  REQUIRE(s.run("magic::netlist_to_layout " + kNetlists + "top.cdl tpdk").ok());
  REQUIRE(s.run("load top").ok());
  CHECK(s.value("instance list celldef XR9").rfind("res_", 0) == 0);
}

TEST_CASE("a missing netlist prints No such file", "[cap:pcell-toolkit][req:SPICE netlist to layout seeding]") {
  PcellSession s;
  const auto r = s.run("magic::netlist_to_layout " + kNetlists + "missing.spice tpdk");
  REQUIRE(r.ok());
  CHECK(contains(texts(r, "PCELL-GEN-002"), "No such file " + kNetlists + "missing.spice"));
}
