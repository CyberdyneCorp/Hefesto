// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The modal key-binding sheet (key_bindings_sheet.hpp; board
// M2-Key-Bindings; M4 CARRY task 11.3): key capture, the conflict warning
// with Replace, Reset table and Load file…, each as `macro` commands run
// through a real core session.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/key_bindings_sheet.hpp"
#include "hefesto/ui/bindings.hpp"

using namespace hefesto::shell;
namespace ui = hefesto::ui;

namespace {

hefesto::platform::PortableOptions test_options() {
  return hefesto::platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                           .inline_tasks = true};
}

struct Session {
  CoreSession core{test_options()};
  ui::MacroTables& macros() { return core.editor().macros; }
  void run_all(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
      INFO(line);
      REQUIRE(CoreCommandSink(core.dispatcher()).submit(line).ok());
    }
  }
};

constexpr std::uint32_t kControl = 4U;
ui::KeyEvent key(std::uint32_t keysym, std::uint32_t state = 0, std::string text = {}) {
  return ui::KeyEvent{keysym, state, std::move(text)};
}
ui::Key parsed(std::string_view name) { return ui::parse_key_name(name).value(); }

}  // namespace

TEST_CASE("adding a binding captures the next chord and writes it with macro",
          "[cap:ui-tools-and-input][req:Default key bindings]") {
  Session s;
  KeyBindingSheet sheet("box");
  sheet.begin_add();
  CHECK(sheet.mode() == KeyBindingSheet::Mode::capturing);
  CHECK(sheet.confirm().empty());  // nothing to write yet
  CHECK(sheet.capture(key(0xFFE3U), true, s.macros()));  // Control alone: still capturing
  CHECK(sheet.mode() == KeyBindingSheet::Mode::capturing);
  // F13 is free in the box table.
  REQUIRE(s.macros().find("box", parsed("F13")) == nullptr);
  CHECK(sheet.capture(key(0xFFCAU), true, s.macros()));
  CHECK(sheet.mode() == KeyBindingSheet::Mode::browse);
  CHECK(sheet.key_name() == ui::key_display_name(parsed("F13")));
  CHECK(sheet.warning().empty());
  sheet.set_text("echo [box] $x");
  const std::string name = sheet.key_name();
  const auto lines = sheet.confirm();
  REQUIRE(lines.size() == 1U);
  CHECK(lines[0] == "macro box " + name + " \"echo \\[box\\] \\$x\"");
  s.run_all(lines);
  const ui::Macro* bound = s.macros().find("box", parsed("F13"));
  REQUIRE(bound != nullptr);
  CHECK(bound->text == "echo [box] $x");
}

TEST_CASE("Escape cancels the capture and nothing is written", "[cap:ui-tools-and-input][req:Default key bindings]") {
  Session s;
  KeyBindingSheet sheet("box");
  sheet.begin_add();
  sheet.set_text("echo hi");
  CHECK(sheet.capture(key(0xFF1BU), true, s.macros()));
  CHECK(sheet.mode() == KeyBindingSheet::Mode::browse);
  CHECK_FALSE(sheet.key().has_value());
  CHECK(sheet.confirm().empty());
  // Escape with a modifier is a chord like any other.
  sheet.begin_add();
  CHECK(sheet.capture(key(0xFF1BU, kControl), true, s.macros()));
  CHECK(sheet.key_name() == ui::key_display_name(parsed("Control_Escape")));
}

TEST_CASE("a key bound in the same table warns and is written only after Replace",
          "[cap:ui-tools-and-input][req:Default key bindings]") {
  Session s;
  const ui::Key u = parsed("u");
  const ui::Macro* undo = s.macros().find("box", u);
  REQUIRE(undo != nullptr);
  KeyBindingSheet sheet("box");
  sheet.begin_add();
  sheet.set_text("echo mine");
  CHECK(sheet.capture(key('u', 0, "u"), true, s.macros()));
  CHECK(sheet.mode() == KeyBindingSheet::Mode::conflict);
  CHECK(sheet.warning() == "\u25B2 u is bound to \u201C" + undo->text + "\u201D in Layout (box tool)");
  CHECK(sheet.confirm().empty());  // not until Replace
  CHECK(s.macros().find("box", u)->text == undo->text);
  const auto lines = sheet.replace();
  REQUIRE(lines == std::vector<std::string>{"macro box u \"echo mine\""});
  s.run_all(lines);
  CHECK(s.macros().find("box", u)->text == "echo mine");
}

TEST_CASE("editing a row to another key moves the binding", "[cap:ui-tools-and-input][req:Default key bindings]") {
  Session s;
  const ui::Key f13 = parsed("F13");
  const ui::Key f14 = parsed("F14");
  const std::string n13 = ui::key_display_name(f13);
  const std::string n14 = ui::key_display_name(f14);
  s.run_all({"macro box " + n13 + " \"echo thirteen\""});
  KeyBindingSheet sheet("box");
  sheet.begin_edit(f13, *s.macros().find("box", f13));
  CHECK(sheet.text() == "echo thirteen");
  // Re-capturing the same key is not a conflict.
  CHECK(sheet.capture(key(0xFFCAU), true, s.macros()));
  CHECK(sheet.mode() == KeyBindingSheet::Mode::browse);
  sheet.begin_edit(f13, *s.macros().find("box", f13));
  CHECK(sheet.capture(key(0xFFCBU), true, s.macros()));  // F14
  const auto lines = sheet.confirm();
  CHECK(lines == std::vector<std::string>{"macro box " + n13 + " \"\"", "macro box " + n14 + " \"echo thirteen\""});
  s.run_all(lines);
  CHECK(s.macros().find("box", f13) == nullptr);
  CHECK(s.macros().find("box", f14)->text == "echo thirteen");
  CHECK(sheet.delete_command(f14) == "macro box " + n14 + " \"\"");
}

TEST_CASE("Reset table restores the table's startup bindings", "[cap:ui-tools-and-input][req:Default key bindings]") {
  Session s;
  hefesto::editor::EditorState fresh;
  ui::install_defaults(fresh);
  CHECK(reset_table_commands(s.macros(), "box").empty());  // nothing to do
  const std::string undo_text = s.macros().find("box", parsed("u"))->text;
  const std::string n13 = ui::key_display_name(parsed("F13"));
  s.run_all({"macro box u \"echo changed\"", "macro box " + n13 + " \"echo added\"", "macro box U \"\""});
  const auto lines = reset_table_commands(s.macros(), "box");
  CHECK(lines.size() == 3U);
  CHECK(lines.front() == "macro box " + n13 + " \"\"");  // deletions first
  s.run_all(lines);
  CHECK(s.macros().find("box", parsed("u"))->text == undo_text);
  CHECK(s.macros().find("box", parsed("F13")) == nullptr);
  CHECK(s.macros().find("box", parsed("U")) != nullptr);
  CHECK(binding_changes(s.macros(), fresh.macros, "box").empty());
}

TEST_CASE("Load file applies only the file's macro lines, as commands",
          "[cap:ui-tools-and-input][req:Default key bindings]") {
  Session s;
  const std::string n13 = ui::key_display_name(parsed("F13"));
  const std::string n14 = ui::key_display_name(parsed("F14"));
  const std::string file =
      "macro box F13 \"echo thirteen\"\n"
      "imacro box F14 \"box size \"\n"
      "macro box u \"\"\n"
      "puts hello\n";
  const LoadedBindings loaded = load_file_commands(s.macros(), file, "keys.magicrc");
  CHECK(loaded.commands == std::vector<std::string>{"macro box u \"\"", "macro box " + n13 + " \"echo thirteen\"",
                                                    "imacro box " + n14 + " \"box size \""});
  REQUIRE(loaded.diagnostics.size() == 1U);  // `puts hello` is not a binding
  s.run_all(loaded.commands);
  CHECK(s.macros().find("box", parsed("u")) == nullptr);
  CHECK(s.macros().find("box", parsed("F13"))->text == "echo thirteen");
  CHECK(s.macros().find("box", parsed("F14"))->interactive);
}
