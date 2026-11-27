// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The node visit order against Magic's (m7-extraction task 10.4, closing
// m6 task 9.2): the capture-only probe NL-P01
// (tests/data/extflat/visit_probe) ran Magic's `ext2sim` with labels and
// aliases on over the 17 DEF-write `.ext` inputs; `.nodes` and `.al` list
// the nodes and their names in EFVisitNodes order. Hefesto's ext2sim reads
// the same files here, in both modes, and must write the same text.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../spice/spice_fixture.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace {

const std::filesystem::path kProbe =
    std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/extflat/visit_probe/NETLIST/cases/NL-P01";

std::string slurp(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  REQUIRE(in.good());
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

struct Target {
  const char* name;
  const char* root;
};

constexpr Target kTargets[] = {
    {"LEFDEF-317", "top"}, {"LEFDEF-318", "top"}, {"LEFDEF-319", "top"}, {"LEFDEF-320", "top"},
    {"LEFDEF-321", "top"}, {"LEFDEF-322", "top"}, {"LEFDEF-323", "top"}, {"LEFDEF-324", "top"},
    {"LEFDEF-325", "top"}, {"LEFDEF-326", "top"}, {"LEFDEF-327", "top"}, {"LEFDEF-328", "top"},
    {"LEFDEF-330", "top"}, {"LEFDEF-331", "c1"},  {"LEFDEF-332", "top"}, {"LEFDEF-333", "top"},
    {"LEFDEF-334", "top"},
};

}  // namespace

TEST_CASE("ext2sim visits the nodes of the 17 DEF-write .ext inputs in Magic's order",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  for (const bool compat : {true, false}) {
    for (const Target& t : kTargets) {
      INFO(t.name << " magic-compat " << compat);
      spice_test::Session s;
      const std::string dir = std::string{"/w/"} + t.name;
      for (const auto& entry : std::filesystem::directory_iterator{kProbe / "input" / t.name}) {
        const auto path = hefesto::platform::Path{dir + "/" + entry.path().filename().string()};
        REQUIRE(s.services.files().write_atomic(path, slurp(entry.path())).ok());
      }
      REQUIRE(hefesto::settings::set(s.project, "magic-compat", compat ? "on" : "off").ok());
      REQUIRE(s.run("tech load " + dir + "/lefdef -noprompt").ok());
      REQUIRE(s.run("ext2sim labels on").ok());
      REQUIRE(s.run("ext2sim alias on").ok());
      REQUIRE(s.run("ext2sim -p " + dir + " " + t.root).ok());
      for (const char* ext : {".nodes", ".al"}) {
        INFO(ext);
        const auto text = s.services.files().read(hefesto::platform::Path{std::string{t.root} + ext});
        REQUIRE(text.ok());
        CHECK(text.value() == slurp(kProbe / "expected/collected" / (std::string{t.name} + ext)));
      }
    }
  }
}
