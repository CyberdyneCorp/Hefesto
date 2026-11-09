// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-settings "Project files"; hefesto-pdk-manager "Per-project
// version pinning" (the pin as stored).

#include <catch2/catch_test_macros.hpp>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/project_file.hpp"
#include "hefesto/platform/file_system.hpp"
#include "temp_dir.hpp"

namespace app = hefesto::app;

namespace {

app::ProjectFile sample() {
  app::ProjectFile p;
  p.pdk = app::PdkPin{"sky130", "sky130A", "0fe599b2afb6708d281543108caf8310912f54af", {"sky130_fd_sc_hd"}};
  p.cell_search_paths = {"cells", "../shared"};
  p.settings.compat.magic_compat = false;
  p.settings.script_strict = true;
  p.extra_settings = {{"units", "microns"}, {"snap", "lambda"}};
  p.nets.global_nets = std::vector<std::string>{"VGND", "VPWR"};
  p.nets.vdd = "VPWR";
  p.open_documents = {"cells/inv.mag"};
  p.window_layout = R"({"panels":["layers"]})";
  return p;
}

}  // namespace

TEST_CASE("project file text is deterministic, sorted JSON", "[cap:hefesto-settings][req:Project files]") {
  const std::string text = app::to_text(sample());
  CHECK(text ==
        "{\n"
        "  \"cellSearchPaths\": [\n    \"cells\",\n    \"../shared\"\n  ],\n"
        "  \"documents\": [\n    \"cells/inv.mag\"\n  ],\n"
        "  \"format\": \"hefesto.project\",\n"
        "  \"globalNets\": {\n    \"globalNets\": [\n      \"VGND\",\n      \"VPWR\"\n    ],\n    \"vdd\": \"VPWR\"\n  },\n"
        "  \"pdk\": {\n    \"family\": \"sky130\",\n    \"libraries\": [\n      \"sky130_fd_sc_hd\"\n    ],\n"
        "    \"variant\": \"sky130A\",\n    \"version\": \"0fe599b2afb6708d281543108caf8310912f54af\"\n  },\n"
        "  \"settings\": {\n    \"magic-compat\": false,\n    \"netgen-compat\": true,\n    \"script-strict\": true,\n"
        "    \"snap\": \"lambda\",\n    \"units\": \"microns\"\n  },\n"
        "  \"version\": 1,\n"
        "  \"windowLayout\": {\n    \"panels\": [\n      \"layers\"\n    ]\n  }\n"
        "}\n");
  CHECK(app::to_text(sample()) == text);
}

TEST_CASE("reopening a project applies the same PDK, paths and settings", "[cap:hefesto-settings][req:Project files]") {
  hefesto::test::TempDir dir;
  hefesto::platform::LocalFileSystem fs;
  REQUIRE(app::save_project(fs, dir.path(), sample()).ok());
  const auto loaded = app::load_project(fs, dir.path());
  REQUIRE(loaded.ok());
  CHECK(loaded.value().pdk == sample().pdk);
  CHECK(loaded.value().cell_search_paths == sample().cell_search_paths);
  CHECK(loaded.value().extra_settings == sample().extra_settings);
  CHECK(loaded.value().nets == sample().nets);
  CHECK(app::to_text(loaded.value()) == app::to_text(sample()));
}

TEST_CASE("the project pins family, variant, full hash and libraries",
          "[cap:hefesto-pdk-manager][req:Per-project version pinning]") {
  const auto parsed = app::parse_project(app::to_text(sample()));
  REQUIRE(parsed.ok());
  REQUIRE(parsed.value().pdk.has_value());
  CHECK(parsed.value().pdk->family == "sky130");
  CHECK(parsed.value().pdk->variant == "sky130A");
  CHECK(parsed.value().pdk->version.size() == 40);
  CHECK(parsed.value().pdk->libraries == std::vector<std::string>{"sky130_fd_sc_hd"});
  const auto partial = app::parse_project(R"({"format":"hefesto.project","version":1,"pdk":{"family":"sky130"}})");
  REQUIRE_FALSE(partial.ok());
  CHECK(partial.error().code == app::codes::kProjectFormat);
}

TEST_CASE("unknown keys are ignored; malformed files are errors", "[cap:hefesto-settings][req:Project files]") {
  const auto future = app::parse_project(R"({"format":"hefesto.project","version":1,"newKey":{"x":1}})");
  REQUIRE(future.ok());
  CHECK_FALSE(future.value().pdk.has_value());
  for (const char* text : {"[]", "{\"format\":\"other\",\"version\":1}", "{\"format\":\"hefesto.project\",\"version\":2}",
                           "{\"format\":\"hefesto.project\",\"version\":1,\"settings\":{\"magic-compat\":\"yes\"}}",
                           "{\"format\":\"hefesto.project\",\"version\":1,\"cellSearchPaths\":[1]}"}) {
    INFO(text);
    const auto r = app::parse_project(text);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == app::codes::kProjectFormat);
  }
  hefesto::test::TempDir dir;
  hefesto::platform::LocalFileSystem fs;
  const auto missing = app::load_project(fs, dir.path());
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == app::codes::kProjectIo);
}
