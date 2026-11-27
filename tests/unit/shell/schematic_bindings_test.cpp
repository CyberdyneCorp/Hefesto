// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic editor's xschem-style default bindings and their rebinding
// through the key-binding sheet (schematic_bindings.hpp,
// key_bindings_sheet.hpp; M8 task 13.2, Q-SCHEM-24). The sheet's lines run
// through the core's `macro` command, whose `schematic` client edits the
// editor's table the canvas reads (M8 round 6).

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/shell/key_bindings_sheet.hpp"
#include "hefesto/shell/schematic_bindings.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/ui/line.hpp"

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

// A headless session with the ui commands (`macro`, `imacro`), the map
// attached to its editor's macro tables as the Schematic tab attaches it.
struct MacroSession {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{
      .storage_root = "/app", .pdk_roots = {}, .in_memory = true, .inline_tasks = true}};
  hefesto::settings::ProjectSettings project;
  hefesto::cmd::ScriptSession script{project};
  hefesto::cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<hefesto::cmd::Dispatcher> dispatcher;
  SchematicKeyMap map;

  MacroSession() {
    (void)hefesto::cmd::register_builtins(registry);
    (void)ui::register_ui_commands(registry);
    dispatcher = std::make_unique<hefesto::cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    map.attach(editor.macros);
  }

  /// Runs a command line; true when every command succeeded.
  bool run(std::string_view line) {
    bool ok = true;
    for (const auto& r : ui::run_command_line(*dispatcher, line, hefesto::cmd::Origin::script)) ok = ok && r.ok();
    return ok;
  }
  /// The result value of one command line.
  std::string value(std::string_view line) {
    std::string out;
    for (const auto& r : ui::run_command_line(*dispatcher, line, hefesto::cmd::Origin::script)) {
      out = r.value.to_string();
    }
    return out;
  }
};

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
  MacroSession session;
  SchematicKeyMap& map = session.map;
  KeyBindingSheet sheet{std::string{kSchematicTable}};
  sheet.begin_add();
  sheet.set_text("schematic hilight clear all");
  REQUIRE(sheet.capture(ui::KeyEvent{'K', kShiftMask, "K"}, false, map.tables()));
  auto lines = sheet.confirm();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].starts_with("macro schematic K "));
  for (const std::string& line : lines) CHECK(session.run(line));
  CHECK(line_of(map, "K") == "schematic hilight clear all");

  // Binding a key the table already uses warns first (conflict), then Replace.
  sheet.begin_add();
  sheet.set_text("schematic netlist -flat");
  REQUIRE(sheet.capture(ui::KeyEvent{'n', 0, "n"}, false, map.tables()));
  CHECK(sheet.mode() == KeyBindingSheet::Mode::conflict);
  CHECK(sheet.confirm().empty());
  for (const std::string& line : sheet.replace()) CHECK(session.run(line));
  CHECK(line_of(map, "n") == "schematic netlist -flat");

  // A row's delete button.
  CHECK(session.run(sheet.delete_command(key("w"))));
  CHECK_FALSE(map.lookup(key("w")).has_value());

  // Reset table restores the defaults and drops the additions.
  const auto reset = map.reset_commands();
  CHECK_FALSE(reset.empty());
  for (const std::string& line : reset) CHECK(session.run(line));
  CHECK(action_of(map, "w") == "wire");
  CHECK(line_of(map, "n") == "schematic netlist");
  CHECK_FALSE(map.lookup(key("K")).has_value());
  CHECK(map.reset_commands().empty());
}

// Regression (M8 round 7, round-6 CARRY): the sheet's Reset table button
// (key_bindings_view.cpp) issues reset_table_commands, which restored the
// core's startup bindings only: for the `schematic` table it deleted every
// binding instead of restoring the xschem-style defaults.
TEST_CASE("the key-binding sheet's Reset table restores the schematic defaults",
          "[cap:ui-tools-and-input][req:Default key bindings]") {
  MacroSession session;
  SchematicKeyMap& map = session.map;
  REQUIRE(session.run(R"(macro schematic K "schematic hilight clear all")"));
  REQUIRE(session.run(R"(macro schematic n "schematic netlist -flat")"));
  REQUIRE(session.run(R"(macro schematic w "")"));

  const auto reset = reset_table_commands(map.tables(), kSchematicTable);
  CHECK(reset == map.reset_commands());
  for (const std::string& line : reset) CHECK(session.run(line));
  for (const DefaultBinding& binding : schematic_default_bindings()) {
    INFO(binding.key);
    const auto bound = map.lookup(key(binding.key));
    REQUIRE(bound.has_value());
    CHECK((bound->action == binding.text || bound->line == binding.text));
  }
  CHECK(line_of(map, "n") == "schematic netlist");
  CHECK_FALSE(map.lookup(key("K")).has_value());
  CHECK(reset_table_commands(map.tables(), kSchematicTable).empty());
}

TEST_CASE("the core macro command's schematic client sets, lists and clears the canvas bindings",
          "[cap:ui-command-dispatch][req:macro and imacro commands]") {
  MacroSession session;
  const SchematicKeyMap& map = session.map;
  // Regression (M8 round 6): `macro schematic KEY TEXT` used to do nothing
  // in the core (no such client: the silent unregistered-client rule), so
  // the sheet's lines never reached the table the canvas reads.
  REQUIRE(session.run(R"(macro schematic z "schematic zoom_full")"));
  CHECK(line_of(map, "z") == "schematic zoom_full");
  CHECK(session.value("macro schematic list z") == "schematic zoom_full");
  const std::string listed = session.value("macro schematic list");
  CHECK(listed.find("schematic zoom_full") != std::string::npos);
  CHECK(listed.find("schematic rotate") != std::string::npos);  // the defaults are in the same table

  // Interactive, with help.
  REQUIRE(session.run(R"(imacro schematic x "schematic select all" "select all")"));
  const ui::Macro* macro = map.tables().find(kSchematicTable, key("x"));
  REQUIRE(macro != nullptr);
  CHECK(macro->interactive);
  CHECK(macro->help == "select all");
  CHECK(session.run(macro_command(kSchematicTable, key("q"), "schematic select \"[x]\" $y")));
  CHECK(line_of(map, "q") == "schematic select \"[x]\" $y");  // the sheet's escapes undone

  // An empty text clears the binding; the layout table is not touched.
  REQUIRE(session.run(R"(macro schematic z "")"));
  CHECK_FALSE(map.lookup(key("z")).has_value());

  // Other tables' lines do not change the schematic bindings.
  REQUIRE(session.run(R"(macro w "box")"));
  CHECK(action_of(map, "w") == "wire");
}

TEST_CASE("attaching keeps bindings the editor's table already has",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  hefesto::editor::EditorState editor;
  editor.macros.define(kSchematicTable, key("w"), "schematic wire 0 0 10 0", std::nullopt, false);
  SchematicKeyMap map;
  map.attach(editor.macros);
  CHECK(line_of(map, "w") == "schematic wire 0 0 10 0");  // a startup file's binding stays
  CHECK(action_of(map, "m") == "move");                   // the other defaults are added
}
