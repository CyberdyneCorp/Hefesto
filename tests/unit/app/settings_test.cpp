// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-settings: layered resolution, startup-file settings, global nets,
// preference export/import.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/settings_layers.hpp"
#include "hefesto/app/settings_stack.hpp"
#include "hefesto/platform/file_system.hpp"
#include "temp_dir.hpp"

namespace app = hefesto::app;
namespace settings = hefesto::settings;
using L = app::SettingLayer;

TEST_CASE("project overrides user and reports the project layer",
          "[cap:hefesto-settings][req:Layered settings resolution]") {
  app::SettingsStack stack;
  stack.set(L::user, "units", "lambda");
  stack.set(L::project, "units", "microns");
  const auto units = stack.effective("units");
  REQUIRE(units.has_value());
  CHECK(units->value == "microns");
  CHECK(units->layer == L::project);
  CHECK(app::layer_name(units->layer) == "project");
}

TEST_CASE("five layers from built-in to session", "[cap:hefesto-settings][req:Layered settings resolution]") {
  app::SettingsStack stack;
  const std::array layers = {L::builtin, L::pdk, L::user, L::project, L::session};
  for (const auto layer : layers) {
    stack.set(layer, "snap", std::string{app::layer_name(layer)});
    CHECK(stack.effective("snap")->layer == layer);
  }
  for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
    CHECK(stack.effective("snap")->layer == *it);
    stack.clear(*it, "snap");
  }
  CHECK_FALSE(stack.effective("snap").has_value());
  stack.set(L::user, "b", "1");
  stack.set(L::pdk, "a", "2");
  CHECK(stack.names() == std::vector<std::string>{"a", "b"});
  CHECK(stack.value_in(L::pdk, "a") == "2");
  CHECK_FALSE(stack.value_in(L::user, "a").has_value());
}

TEST_CASE("built-in defaults and the hand-over to the typed project settings",
          "[cap:hefesto-settings][req:Layered settings resolution]") {
  app::SettingsStack stack;
  app::install_builtin_defaults(stack);
  CHECK(stack.effective("magic-compat")->value == "on");
  CHECK(stack.effective("vdd")->value == "vdd!");
  CHECK(stack.effective("gnd")->value == "gnd!");
  CHECK_FALSE(stack.effective("sub").has_value());
  stack.set(L::session, "magic-compat", "off");
  stack.set(L::project, "script-strict", "on");
  settings::ProjectSettings typed;
  app::apply_effective(stack, typed);
  CHECK_FALSE(typed.compat.magic_compat);
  CHECK(typed.script_strict);
}

TEST_CASE("PDK startup file seeds power names into the PDK layer",
          "[cap:hefesto-settings][req:Global nets and power names]") {
  const auto values = app::startup_file_settings("# sky130A\nset VDD VPWR\nset GND VGND\nset SUB VSUBS\n");
  CHECK(values == std::vector<std::pair<std::string, std::string>>{{"vdd", "VPWR"}, {"gnd", "VGND"}, {"sub", "VSUBS"}});
  app::SettingsStack stack;
  app::install_builtin_defaults(stack);
  for (const auto& [name, value] : values) {
    stack.set(L::pdk, name, value);
  }
  CHECK(stack.effective("vdd")->value == "VPWR");
  CHECK(stack.effective("sub")->layer == L::pdk);
  CHECK(stack.effective("globalNets")->value.empty());
}

TEST_CASE("globals array becomes a sorted list", "[cap:hefesto-settings][req:Global nets and power names]") {
  const auto values = app::startup_file_settings("set globals(VPWR) 1\nset globals(VGND) 1\n");
  CHECK(values == std::vector<std::pair<std::string, std::string>>{{"globalNets", "VGND VPWR"}});
}

TEST_CASE("startup-file settings skip scripts, substitutions and nested bodies",
          "[cap:hefesto-settings][req:Magic startup-file compatibility]") {
  const auto values = app::startup_file_settings(
      "proc x {} {\n  set VDD inner\n}\n"
      "if {1} {\n set GND nested\n}\n"
      "set VDD $env(VDD)\n"
      "set GND [string toupper vss]\n"
      "set SUB \"VSUBS\" ;# comment\n"
      "tech load sky130A\n");
  CHECK(values == std::vector<std::pair<std::string, std::string>>{{"sub", "VSUBS"}});
}

TEST_CASE("the real sky130A startup file seeds VDD, GND and SUB",
          "[cap:hefesto-settings][req:Global nets and power names]") {
  const std::filesystem::path rc = std::filesystem::path{HEFESTO_SOURCE_DIR} /
                                   ".cache/pdks/ciel/sky130/versions/0fe599b2afb6708d281543108caf8310912f54af/sky130A/"
                                   "libs.tech/magic/sky130A.magicrc";
  if (!std::filesystem::exists(rc)) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const auto values = app::startup_file_settings(hefesto::test::read_file(rc));
  std::map<std::string, std::string> found(values.begin(), values.end());
  CHECK(found["vdd"] == "VPWR");
  CHECK(found["gnd"] == "VGND");
  CHECK(found["sub"] == "VSUBS");
}

TEST_CASE("project global nets override the PDK", "[cap:hefesto-settings][req:Global nets and power names]") {
  app::SettingsStack stack;
  app::install_builtin_defaults(stack);
  stack.set(L::pdk, "gnd", "VGND");
  app::ProjectFile project;
  project.nets.gnd = "VSS";
  project.nets.global_nets = std::vector<std::string>{"b", "a", "b"};
  app::apply_project(stack, project);
  CHECK(stack.effective("gnd")->value == "VSS");
  CHECK(stack.effective("gnd")->layer == L::project);
  CHECK(stack.effective("globalNets")->value == "a b");
  CHECK(stack.effective("vdd")->layer == L::builtin);  // unset fields leave the lower layer in place
}

TEST_CASE("preferences export and import as a file", "[cap:hefesto-settings][req:Preference sync]") {
  hefesto::test::TempDir dir;
  hefesto::platform::LocalFileSystem fs;
  const auto path = dir.path() / "prefs.json";
  app::SettingsStack stack;
  stack.set(L::user, "units", "microns");
  stack.set(L::user, "theme", "dark");
  stack.set(L::project, "snap", "internal");  // not a preference
  REQUIRE(app::save_preferences(fs, path, stack).ok());
  CHECK(hefesto::test::read_file(path) == "{\n  \"theme\": \"dark\",\n  \"units\": \"microns\"\n}\n");
  app::SettingsStack other;
  REQUIRE(app::load_preferences(fs, path, other).ok());
  CHECK(other.effective("units")->value == "microns");
  CHECK(other.effective("units")->layer == L::user);
  CHECK_FALSE(other.effective("snap").has_value());
  CHECK(app::load_preferences(fs, dir.path() / "missing.json", other).ok());
  hefesto::test::write_file(dir.path() / "bad.json", "[1]");
  const auto bad = app::load_preferences(fs, dir.path() / "bad.json", other);
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == app::codes::kSettingsFile);
}
