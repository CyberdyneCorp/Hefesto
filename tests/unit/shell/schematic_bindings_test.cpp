// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic editor's xschem-style default bindings and their rebinding
// through the key-binding sheet (schematic_bindings.hpp,
// key_bindings_sheet.hpp; M8 task 13.2, Q-SCHEM-24).

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/shell/key_bindings_sheet.hpp"
#include "hefesto/shell/schematic_bindings.hpp"

using namespace hefesto::shell;
namespace ui = hefesto::ui;

namespace {

ui::Key key(std::string_view name) {
  auto parsed = ui::parse_key_name(name);
  REQUIRE(parsed.ok());
  return parsed.value();
}

std::string line_of(const SchematicKeyMap& map, std::string_view name) {
  const auto bound = map.lookup(key(name));
  REQUIRE(bound.has_value());
  return bound->line;
}

std::string action_of(const SchematicKeyMap& map, std::string_view name) {
  const auto bound = map.lookup(key(name));
  REQUIRE(bound.has_value());
  return bound->action;
}

constexpr std::uint32_t kShiftMask = 1U;
constexpr std::uint32_t kControlMask = 4U;

}  // namespace

TEST_CASE("the schematic tab has xschem's default bindings",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  const SchematicKeyMap map;
  CHECK(action_of(map, "w") == "wire");
  CHECK(action_of(map, "m") == "move");
  CHECK(action_of(map, "c") == "copy");
  CHECK(line_of(map, "R") == "schematic rotate");
  CHECK(line_of(map, "F") == "schematic flip");
  CHECK(action_of(map, "e") == "descend");
  CHECK(line_of(map, "Control_e") == "schematic ascend");
  CHECK(action_of(map, "q") == "properties");
  CHECK(line_of(map, "k") == "schematic hilight selection");
  CHECK(line_of(map, "u") == "schematic undo");
  CHECK(line_of(map, "U") == "schematic redo");
  CHECK(action_of(map, "Insert") == "place");
  CHECK(line_of(map, "n") == "schematic netlist");
  CHECK(line_of(map, "Delete") == "schematic delete");
  CHECK_FALSE(map.lookup(key("z")).has_value());
  // Every default parses and carries help for the sheet.
  for (const DefaultBinding& b : schematic_default_bindings()) {
    INFO(b.key);
    CHECK(ui::parse_key_name(b.key).ok());
    CHECK_FALSE(b.help.empty());
  }
}

TEST_CASE("key events reach the schematic bindings", "[cap:schematic-editing][req:Desktop and touch interaction]") {
  const SchematicKeyMap map;
  const auto rotate = map.lookup(ui::KeyEvent{'R', kShiftMask, "R"}, false);
  REQUIRE(rotate.has_value());
  CHECK(rotate->line == "schematic rotate");
  const auto ascend = map.lookup(ui::KeyEvent{'e', kControlMask, "\x05"}, false);  // as the toolkit reports Ctrl-e
  REQUIRE(ascend.has_value());
  CHECK(ascend->line == "schematic ascend");
  const auto wire = map.lookup(ui::KeyEvent{'w', 0, "w"}, false);
  REQUIRE(wire.has_value());
  CHECK(wire->action == "wire");
}

TEST_CASE("the key-binding sheet rebinds the schematic table", "[cap:ui-tools-and-input][req:Default key bindings]") {
  SchematicKeyMap map;
  KeyBindingSheet sheet{std::string{kSchematicTable}};
  sheet.begin_add();
  sheet.set_text("schematic hilight clear all");
  REQUIRE(sheet.capture(ui::KeyEvent{'K', kShiftMask, "K"}, false, map.tables()));
  auto lines = sheet.confirm();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].starts_with("macro schematic K "));
  for (const std::string& line : lines) CHECK(map.apply(line));
  CHECK(line_of(map, "K") == "schematic hilight clear all");

  // Binding a key the table already uses warns first (conflict), then Replace.
  sheet.begin_add();
  sheet.set_text("schematic netlist -flat");
  REQUIRE(sheet.capture(ui::KeyEvent{'n', 0, "n"}, false, map.tables()));
  CHECK(sheet.mode() == KeyBindingSheet::Mode::conflict);
  CHECK(sheet.confirm().empty());
  for (const std::string& line : sheet.replace()) CHECK(map.apply(line));
  CHECK(line_of(map, "n") == "schematic netlist -flat");

  // A row's delete button.
  CHECK(map.apply(sheet.delete_command(key("w"))));
  CHECK_FALSE(map.lookup(key("w")).has_value());

  // Reset table restores the defaults and drops the additions.
  const auto reset = map.reset_commands();
  CHECK_FALSE(reset.empty());
  for (const std::string& line : reset) CHECK(map.apply(line));
  CHECK(action_of(map, "w") == "wire");
  CHECK(line_of(map, "n") == "schematic netlist");
  CHECK_FALSE(map.lookup(key("K")).has_value());
  CHECK(map.reset_commands().empty());
}

TEST_CASE("only schematic binding lines apply", "[cap:ui-tools-and-input][req:Default key bindings]") {
  SchematicKeyMap map;
  std::vector<hefesto::diag::Diagnostic> diagnostics;
  CHECK_FALSE(map.apply("macro layout w \"box\"", &diagnostics));
  CHECK_FALSE(map.apply("paint m1", &diagnostics));
  CHECK_FALSE(map.apply("macro schematic NoSuchKeyName \"x\"", &diagnostics));
  CHECK(diagnostics.size() == 3);
  CHECK(map.apply("imacro schematic x \"schematic select all\" {select all}"));
  const ui::Macro* macro = map.tables().find(kSchematicTable, key("x"));
  REQUIRE(macro != nullptr);
  CHECK(macro->interactive);
  CHECK(macro->help == "select all");
  CHECK(map.apply(macro_command(kSchematicTable, key("q"), "schematic select \"[x]\" $y")));
  CHECK(line_of(map, "q") == "schematic select \"[x]\" $y");  // the sheet's escapes undone
}
