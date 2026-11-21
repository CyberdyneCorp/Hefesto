// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace settings = hefesto::settings;
using hefesto::compat::CompatSettings;

TEST_CASE("magic-compat is on by default and recorded in headers",
          "[cap:hefesto-magic-compat-mode][req:magic-compat setting for value-changing quirks]") {
  CompatSettings compat;
  CHECK(compat.magic_compat);
  CHECK(hefesto::compat::magic_compat_header_tag(compat) == "magic-compat=on");
  compat.magic_compat = false;
  CHECK(hefesto::compat::magic_compat_header_tag(compat) == "magic-compat=off");
}

TEST_CASE("netgen-compat is on by default",
          "[cap:hefesto-magic-compat-mode][req:netgen-compat setting for netgen quirks]") {
  CompatSettings compat;
  CHECK(compat.netgen_compat);
  CHECK(hefesto::compat::netgen_compat_header_tag(compat) == "netgen-compat=on");
  compat.netgen_compat = false;
  // netgen-compat never changes the tag that layout/netlist writers record.
  CHECK(hefesto::compat::magic_compat_header_tag(compat) == "magic-compat=on");
}

TEST_CASE("settings are independent",
          "[cap:hefesto-magic-compat-mode][req:Reference-tool compatibility settings]") {
  settings::ProjectSettings project;
  REQUIRE(settings::set(project, "netgen-compat", "off").ok());
  CHECK(project.compat.magic_compat);
  CHECK_FALSE(project.compat.netgen_compat);
  CHECK_FALSE(project.script_strict);

  REQUIRE(settings::set(project, "script-strict", "on").ok());
  CHECK(project.compat.magic_compat);  // strict does not touch magic-compat
  REQUIRE(settings::set(project, "magic-compat", "off").ok());
  CHECK(project.script_strict);  // magic-compat does not touch strict
  CHECK(settings::get(project, "magic-compat").value() == false);
}

TEST_CASE("xschem-compat is on by default and recorded in a netlist only when off",
          "[cap:hefesto-magic-compat-mode][req:xschem-compat setting for xschem quirks]") {
  CompatSettings compat;
  CHECK(compat.xschem_compat);
  CHECK(hefesto::compat::xschem_compat_netlist_line(compat).empty());
  compat.xschem_compat = false;
  CHECK(hefesto::compat::xschem_compat_netlist_line(compat) == "** xschem-compat=off");
  // xschem-compat never changes the tags of layout, extraction or LVS output.
  CHECK(hefesto::compat::magic_compat_header_tag(compat) == "magic-compat=on");
  CHECK(hefesto::compat::netgen_compat_header_tag(compat) == "netgen-compat=on");

  const settings::ProjectSettings project;
  CHECK(settings::get(project, settings::kXschemCompat).value() == true);
}

TEST_CASE("magic-compat and netgen-compat never change the xschem-compat netlist line",
          "[cap:hefesto-magic-compat-mode][req:Reference-tool compatibility settings]") {
  for (const bool magic : {true, false}) {
    for (const bool netgen : {true, false}) {
      CompatSettings compat;
      compat.magic_compat = magic;
      compat.netgen_compat = netgen;
      CHECK(hefesto::compat::xschem_compat_netlist_line(compat).empty());
    }
  }
}

TEST_CASE("xschem-compat and portable-netlist-paths are independent project settings",
          "[cap:hefesto-magic-compat-mode][req:Reference-tool compatibility settings]") {
  settings::ProjectSettings project;
  CHECK_FALSE(project.portable_netlist_paths);
  REQUIRE(settings::set(project, "xschem-compat", "off").ok());
  CHECK_FALSE(project.compat.xschem_compat);
  CHECK(project.compat.magic_compat);
  CHECK(project.compat.netgen_compat);
  CHECK_FALSE(project.portable_netlist_paths);
  CHECK_FALSE(project.script_strict);

  REQUIRE(settings::set(project, "portable-netlist-paths", "on").ok());
  CHECK(project.portable_netlist_paths);
  CHECK_FALSE(project.compat.xschem_compat);
  REQUIRE(settings::set(project, "magic-compat", "off").ok());
  CHECK_FALSE(project.compat.xschem_compat);  // magic-compat does not govern SCHEM.md (D-21)
  CHECK(settings::get(project, "portable-netlist-paths").value() == true);
}

TEST_CASE("setting names come in a fixed order", "[cap:hefesto-settings][req:Project files]") {
  const auto names = settings::setting_names();
  REQUIRE(names.size() == 5);
  CHECK(names[0] == "magic-compat");
  CHECK(names[1] == "netgen-compat");
  CHECK(names[2] == "xschem-compat");
  CHECK(names[3] == "portable-netlist-paths");
  CHECK(names[4] == "script-strict");
  settings::ProjectSettings project;
  const auto unknown = settings::set(project, "other", "on");
  REQUIRE_FALSE(unknown.ok());
  CHECK(unknown.error().text ==
        "unknown project setting 'other' (known: magic-compat, netgen-compat, xschem-compat, "
        "portable-netlist-paths, script-strict)");
}

TEST_CASE("script-strict is off by default", "[cap:hefesto-diagnostics][req:Script strict mode switch]") {
  const settings::ProjectSettings project;
  CHECK(settings::get(project, settings::kScriptStrict).value() == false);
}

TEST_CASE("setting names and values are validated",
          "[cap:hefesto-magic-compat-mode][req:Reference-tool compatibility settings]") {
  settings::ProjectSettings project;
  const auto bad_name = settings::set(project, "spectre-compat", "on");
  REQUIRE_FALSE(bad_name.ok());
  CHECK(bad_name.error().code == hefesto::diag::codes::kSettingsName);
  const auto bad_value = settings::set(project, "magic-compat", "maybe");
  REQUIRE_FALSE(bad_value.ok());
  CHECK(bad_value.error().code == hefesto::diag::codes::kSettingsValue);
  CHECK(project == settings::ProjectSettings{});
  CHECK(settings::parse_bool("yes") == true);
  CHECK(settings::parse_bool("0") == false);
  CHECK_FALSE(settings::parse_bool("ON").has_value());
}

TEST_CASE("project settings persist per project as JSON",
          "[cap:hefesto-magic-compat-mode][req:Reference-tool compatibility settings]") {
  hefesto::platform::MemoryFileSystem fs;
  settings::ProjectSettings project;
  project.compat.netgen_compat = false;
  REQUIRE(settings::save(fs, "/proj/hefesto-project.json", project).ok());
  CHECK(fs.read("/proj/hefesto-project.json").value() ==
        "{\n  \"magic-compat\": true,\n  \"netgen-compat\": false,\n  \"xschem-compat\": true,\n"
        "  \"portable-netlist-paths\": false,\n  \"script-strict\": false\n}\n");
  const auto loaded = settings::load(fs, "/proj/hefesto-project.json");
  REQUIRE(loaded.ok());
  CHECK(loaded.value() == project);

  CHECK(settings::from_json("{}").value() == settings::ProjectSettings{});
  // Files written before M8 lack the two schematic settings and read their defaults.
  const auto old = settings::from_json("{\"magic-compat\": true, \"netgen-compat\": true, \"script-strict\": true}");
  REQUIRE(old.ok());
  CHECK(old.value().compat.xschem_compat);
  CHECK_FALSE(old.value().portable_netlist_paths);
  const auto schematic = settings::from_json("{\"xschem-compat\": false, \"portable-netlist-paths\": true}");
  REQUIRE(schematic.ok());
  CHECK_FALSE(schematic.value().compat.xschem_compat);
  CHECK(schematic.value().portable_netlist_paths);
  for (const char* bad : {"[]", "{\"magic-compat\": 1}", "{\"other\": true}", "{"}) {
    INFO(bad);
    const auto r = settings::from_json(bad, "p.json");
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == hefesto::diag::codes::kSettingsFile);
  }
}
