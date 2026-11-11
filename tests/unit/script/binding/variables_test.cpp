// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Script variables read by the engine (tasks.md 5.4; capability
// scripting-api "Script variables read by the engine"; hefesto-settings
// "Global nets and power names" (D-14); Q-SCRIPT-5, compat: both modes).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "binding_fixture.hpp"
#include "hefesto/app/settings_layers.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/script/binding/variables.hpp"

namespace script = hefesto::script;
namespace app = hefesto::app;
namespace cell = hefesto::cell;
using script::test::BindingFixture;

namespace {

std::string effective(const BindingFixture& f, std::string_view name) {
  const auto value = f.editor.settings.effective(name);
  return value ? value->value : std::string{"<unset>"};
}

std::string expand(const BindingFixture& f, std::string_view entry) {
  return cell::expand_entry(entry, f.editor.path_variables).value_or("<unusable>");
}

std::size_t warnings_naming(BindingFixture& f, std::string_view name) {
  std::size_t count = 0;
  for (const auto& d : f.printed.take()) {
    if (d.code == "SCRIPT-BIND-001" && d.text.find("\"" + std::string{name} + "\"") != std::string::npos) {
      ++count;
    }
  }
  return count;
}

}  // namespace

TEST_CASE("Substrate variable substitution", "[cap:scripting-api][req:Script variables read by the engine]") {
  // The extraction that names the substrate node is M7; the variable reaches
  // the setting it reads ("sub" of "global nets and power names").
  BindingFixture f;
  f.value("set SUB VSUBS");
  CHECK(effective(f, app::kSub) == "VSUBS");
  CHECK(f.editor.settings.effective(app::kSub)->layer == app::SettingLayer::session);
}

TEST_CASE("Undefined substrate variable", "[cap:scripting-api][req:Script variables read by the engine]") {
  BindingFixture f;
  CHECK(f.value("info exists SUB") == "0");  // unset by default
  f.value("set SUB VSUBS; unset SUB");
  CHECK(f.editor.settings.effective(app::kSub)->value.empty());
  CHECK(f.value("info exists SUB") == "0");
  f.value("set SUB again");  // the traces survive the unset
  CHECK(effective(f, app::kSub) == "again");
}

TEST_CASE("Power names read the effective setting", "[cap:scripting-api][req:Script variables read by the engine]") {
  BindingFixture f;
  CHECK(f.value("set VDD") == "vdd!");  // Magic's system startup values (built-in layer)
  CHECK(f.value("set GND") == "gnd!");
  f.editor.settings.set(app::SettingLayer::project, app::kVdd, "VPWR");
  CHECK(f.value("set VDD") == "VPWR");
  f.value("set GND VGND");
  CHECK(effective(f, app::kGnd) == "VGND");
}

TEST_CASE("The globals array is the global net list", "[cap:scripting-api][req:Script variables read by the engine]") {
  BindingFixture f;
  f.value("set globals(VPWR) 1; set globals(VGND) 1");
  CHECK(effective(f, app::kGlobalNets) == "VGND VPWR");
  f.value("unset globals(VPWR)");
  CHECK(effective(f, app::kGlobalNets) == "VGND");
  f.editor.settings.set(app::SettingLayer::session, app::kGlobalNets, "VGND VDDIO");
  CHECK(f.value("info exists globals(VDDIO)") == "1");
  CHECK(f.value("lsort [array names globals]") == "VDDIO VGND");
  // A startup file's values move to its layer.
  script::adopt_engine_variables(f.editor.settings, app::SettingLayer::pdk);
  CHECK(f.editor.settings.effective(app::kGlobalNets)->layer == app::SettingLayer::pdk);
  CHECK_FALSE(f.editor.settings.value_in(app::SettingLayer::session, app::kGlobalNets).has_value());
}

TEST_CASE("Path variable from script", "[cap:scripting-api][req:Script variables read by the engine]") {
  BindingFixture f;
  f.value("set PDKPATH /pdk/sky130A");
  CHECK(f.value("addpath {$PDKPATH/libs.ref/mag/sky130_fd_sc_hd}") == "");
  CHECK(expand(f, "$PDKPATH/libs.ref/mag/sky130_fd_sc_hd") == "/pdk/sky130A/libs.ref/mag/sky130_fd_sc_hd");
  CHECK(expand(f, "${PDKPATH}/x") == "/pdk/sky130A/x");
  // CAD_ROOT is defined when the binding is installed.
  CHECK(f.value("info exists CAD_ROOT") == "1");
}

TEST_CASE("Changed path variable with magic-compat ON", "[cap:scripting-api][req:Script variables read by the engine]") {
  BindingFixture f{true};
  static_cast<void>(f.printed.take());
  f.value("set LIB /a");
  CHECK(expand(f, "$LIB/c") == "/a/c");
  f.value("set LIB /b");
  CHECK(expand(f, "$LIB/d") == "/a/d");
  CHECK(expand(f, "$LIB/e") == "/a/e");
  CHECK(warnings_naming(f, "LIB") == 1);
}

TEST_CASE("Changed path variable with magic-compat OFF", "[cap:scripting-api][req:Script variables read by the engine]") {
  BindingFixture f{false};
  static_cast<void>(f.printed.take());
  f.value("set LIB /a");
  CHECK(expand(f, "$LIB/c") == "/a/c");
  f.value("set LIB /b");
  CHECK(expand(f, "$LIB/d") == "/b/d");
  CHECK(expand(f, "$LIB/e") == "/b/e");
  CHECK(warnings_naming(f, "LIB") == 1);
}

TEST_CASE("An unresolved path variable is retried", "[cap:scripting-api][req:Script variables read by the engine]") {
  BindingFixture f;
  CHECK(expand(f, "$HEFESTO_NO_SUCH_VARIABLE/x") == "$HEFESTO_NO_SUCH_VARIABLE/x");
  f.value("set HEFESTO_NO_SUCH_VARIABLE /late");
  CHECK(expand(f, "$HEFESTO_NO_SUCH_VARIABLE/x") == "/late/x");
}

TEST_CASE("The path cache keeps the first value and warns once", "[cap:scripting-api][req:Script variables read by the engine]") {
  script::PathVariableCache cache;
  int warnings = 0;
  cache.set_reporter([&](const hefesto::diag::Diagnostic&) { ++warnings; });
  CHECK(cache.resolve("V", std::nullopt, true) == std::nullopt);
  CHECK(cache.resolve("V", std::string{"1"}, true) == "1");
  CHECK(cache.resolve("V", std::string{"2"}, true) == "1");
  CHECK(cache.resolve("V", std::string{"2"}, false) == "2");
  CHECK(cache.resolve("V", std::string{"1"}, false) == "1");
  CHECK(warnings == 1);
}
