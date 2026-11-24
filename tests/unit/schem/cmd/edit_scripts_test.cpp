// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scripts that edit through `schematic` (capability schematic-command
// "Subcommand table", scenario "Script builds a divider", and golden case
// SCHEM-CMD-01, whose reference is xschem's netlist of the same circuit).
// The editing options are EDIT's (schem/edit/).
//
// The session folder stands for an installation root: CAD_ROOT is the
// folder and a copy of the system library is its `assets/schematic` (the
// project sandbox grants only that folder). The case's `xschemrc` is not
// used: it also creates `../out`, outside the granted folder, so the
// library path is set after `schematic new` instead.
//
// Owner: EDIT.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_fixture.hpp"

namespace cmd = hefesto::cmd;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kTable = "[cap:schematic-command][req:Subcommand table]";

const std::filesystem::path kCase = std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/SCHEM/cases/SCHEM-CMD-01";

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) out.push_back(line);
  return out;
}

bool has_line(const std::string& text, const std::string& line) {
  for (const std::string& l : lines_of(text)) {
    if (l == line) return true;
  }
  return false;
}

// Installs the system library copy and returns the `schematic path set`
// command that puts it on the search path.
std::string install_library(SchemSession& s) {
  const std::filesystem::path devices = s.folder().path() / "assets/schematic/xschem_library/devices";
  std::filesystem::create_directories(devices.parent_path());
  std::filesystem::copy(std::filesystem::path{HEFESTO_SOURCE_DIR} / "assets/schematic/xschem_library/devices",
                        devices, std::filesystem::copy_options::recursive);
  s.editor().script.sandbox.environment["CAD_ROOT"] = s.folder().str();
  return "schematic path set {" + devices.generic_string() + "}";
}

}  // namespace

TEST_CASE("Script builds a divider", kTable) {
  SchemSession s;
  const std::string path = install_library(s);
  const std::string script = "schematic new schematic " + s.folder().str("div.sch") + "; " + path +
                             "; schematic place res.sym 0 0; schematic place res.sym 0 100;"
                             " schematic wire 0 30 0 70; schematic label 0 -30 IN; schematic label 0 130 0;"
                             " schematic netlist";
  const cmd::CommandResult result = s.tcl(script);
  INFO(SchemSession::message(result));
  REQUIRE(result.ok());
  const std::string netlist = result.value.to_string();
  INFO(netlist);
  CHECK(has_line(netlist, "R1 IN net1 1k"));
  CHECK(has_line(netlist, "R2 net1 0 1k"));
}

TEST_CASE("SCHEM-CMD-01: the divider script netlists as xschem's div.sch", kTable) {
  SchemSession s;
  const std::string path = install_library(s);
  std::filesystem::create_directories(s.folder().path() / "out");
  for (std::string line : lines_of(read_file(kCase / "input/hefesto.hef"))) {
    // The case runs in its input folder; here the session folder.
    if (line == "schematic netlist out/div.spice") line = "schematic netlist " + s.folder().str("out/div.spice");
    if (line == "schematic new schematic div.sch") {
      line = "schematic new schematic " + s.folder().str("div.sch") + "; " + path;
    }
    const cmd::CommandResult result = s.tcl(line);
    INFO(line << ": " << SchemSession::message(result));
    REQUIRE(result.ok());
  }
  const std::vector<std::string> got = lines_of(s.folder().read("out/div.spice"));
  const std::vector<std::string> expected = lines_of(read_file(kCase / "expected/div.spice"));
  REQUIRE(got.size() == expected.size());
  // Line 1 is `** sch_path: <absolute path>` (mask schem-paths).
  CHECK(got[0].rfind("** sch_path: ", 0) == 0);
  CHECK(got[0].ends_with("/div.sch"));
  for (std::size_t i = 1; i < got.size(); ++i) CHECK(got[i] == expected[i]);
}
