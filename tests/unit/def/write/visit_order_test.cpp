// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFVisitNodes order against Magic's (m6-lef-def-oasis task 9.2): `def
// write` writes SPECIALNETS and NETS in the netlist's visit order, so the
// net records of Magic's captured `top.def` of each DEF-write net case
// (tests/golden/LEFDEF/cases/<id>/expected/) are the visit order Magic's
// EFFlatBuild and EFVisitNodes give over the case's own `.ext` files.
// Each case's inputs are read from its `input/` as the golden replay does,
// and the written net names are compared in order, in both modes (the
// visit order has no magic-compat dependence).
//
// Owner: DEFW, with EXTSUB's visit order (design D2).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "write_fixture.hpp"

namespace def_write_test {
namespace {

std::filesystem::path case_dir(const std::string& id) {
  return std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/LEFDEF/cases" / id;
}

std::string slurp(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// The `- <name>` records of the SPECIALNETS section, then of NETS.
std::vector<std::string> net_records(const std::string& def) {
  std::vector<std::string> names;
  for (const std::string_view kind : {"SPECIALNETS", "NETS"}) {
    std::istringstream in(section(def, kind));
    for (std::string line; std::getline(in, line);) {
      if (line.starts_with("   - ")) {
        const std::string rest = line.substr(5);
        names.push_back(rest.substr(0, rest.find(' ')));
      }
    }
  }
  return names;
}

/// Writes the case's `top` with `command` from its golden inputs.
std::string write_case(const std::string& id, const std::string& command, bool magic_compat) {
  const std::filesystem::path input = case_dir(id) / "input";
  Session s(magic_compat, slurp(input / "lefdef.tech"));
  for (const auto& entry : std::filesystem::directory_iterator(input)) {
    const std::string ext = entry.path().extension().string();
    if (ext == ".mag" || ext == ".ext") {
      s.file("/t/" + entry.path().filename().string(), slurp(entry.path()));
    }
  }
  REQUIRE(s.run("load top").ok());
  s.editor.edit_root = s.editor.cells.find("top");
  REQUIRE(s.editor.edit_root != nullptr);
  const auto result = s.run(command);
  for (const auto& d : s.messages) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(result.ok());
  return s.read("top.def");
}

TEST_CASE("the nets are visited in Magic's EFVisitNodes order on every DEF-write net case",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  // LEFDEF-319 sets Tcl globals and LEFDEF-331 writes two cells; the golden
  // replay covers them.
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"LEFDEF-317", "def write"}, {"LEFDEF-318", "def write -allspecial"},
      {"LEFDEF-320", "def write"}, {"LEFDEF-321", "def write"},
      {"LEFDEF-322", "def write"}, {"LEFDEF-323", "def write"},
      {"LEFDEF-324", "def write"}, {"LEFDEF-325", "def write"},
      {"LEFDEF-326", "def write"}, {"LEFDEF-327", "def write"},
      {"LEFDEF-328", "def write"}, {"LEFDEF-330", "def write -anal"},
      {"LEFDEF-332", "def write"}, {"LEFDEF-333", "def write"},
      {"LEFDEF-334", "def write"},
  };
  for (const auto& [id, command] : cases) {
    const std::vector<std::string> magic = net_records(slurp(case_dir(id) / "expected/top.def"));
    for (const bool on : {true, false}) {
      INFO(id << ", magic-compat " << (on ? "ON" : "OFF"));
      CHECK(net_records(write_case(id, command, on)) == magic);
    }
  }
}

}  // namespace
}  // namespace def_write_test
