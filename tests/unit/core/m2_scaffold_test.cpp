// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M2 scaffold (openspec/changes/m2-interactive-editing tasks.md 1.3, 1.4,
// 11.5): the final M2 command set is registered, no stub is left
// (CORE-NOTIMPL-002 is retired), and the shared interfaces of design D4
// (PromptHost, PointerState, post_command, the edit frame and the new
// EditorState members) work.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/pointer.hpp"
#include "hefesto/cmd/prompt.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/editor/post_command.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/undo/db_recorder.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace editor = hefesto::editor;

namespace {

std::string read_source(const std::string& relative) {
  std::ifstream in(std::string{HEFESTO_SOURCE_DIR} + "/" + relative);
  REQUIRE(in);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

// The backquoted names of a Markdown table cell ("`a`, `b` (note)").
std::vector<std::string> quoted_names(const std::string& cell) {
  std::vector<std::string> names;
  std::size_t pos = 0;
  while ((pos = cell.find('`', pos)) != std::string::npos) {
    const std::size_t end = cell.find('`', pos + 1);
    if (end == std::string::npos) {
      break;
    }
    names.push_back(cell.substr(pos + 1, end - pos - 1));
    pos = end + 1;
  }
  return names;
}

// The commands of tests/golden/CMDEDIT/CASES.md "Command registration
// checklist" whose owner is an M2 capability (the cmd-* rows).
std::vector<std::string> checklist_m2_commands() {
  std::istringstream doc(read_source("tests/golden/CMDEDIT/CASES.md"));
  std::vector<std::string> names;
  bool in_checklist = false;
  for (std::string line; std::getline(doc, line);) {
    if (line.starts_with("## ")) {
      in_checklist = line.find("Command registration checklist") != std::string::npos;
      continue;
    }
    const std::size_t bar = line.rfind('|', line.size() - 2);
    if (!in_checklist || !line.starts_with("| `") || bar == std::string::npos) {
      continue;
    }
    if (line.substr(bar).find("| cmd-") != 0) {
      continue;
    }
    for (std::string& name : quoted_names(line.substr(0, bar))) {
      names.push_back(std::move(name));
    }
  }
  return names;
}

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&state);
  }
};

class Scripted final : public cmd::PromptHost {
 public:
  explicit Scripted(std::size_t answer) : answer_(answer) {}
  std::size_t ask(std::string_view, std::span<const std::string>, std::size_t) override { return answer_; }

 private:
  std::size_t answer_;
};

}  // namespace

TEST_CASE("every CMDEDIT checklist command of an M2 capability is registered", "[m2-scaffold]") {
  cmd::CommandRegistry registry;
  REQUIRE(cmd::register_all_commands(registry));
  const auto names = checklist_m2_commands();
  CHECK(names.size() >= 75);  // the cmd-* rows of the checklist (75 names)
  for (const std::string& name : names) {
    INFO(name);
    CHECK(registry.find(name) != nullptr);
  }
}

TEST_CASE("every M2 command outside the CMDEDIT checklist is registered", "[m2-scaffold]") {
  cmd::CommandRegistry registry;
  REQUIRE(cmd::register_all_commands(registry));
  // layout-undo, ui-command-dispatch, ui-tools-and-input, ui-box-stack-and-
  // measure, the `tool` command (ui-gui-managers "Tool modes"),
  // ui-wizard-and-debug, ui-feedback-and-highlights and wiring-tool.
  for (const char* name : {"undo", "redo", "macro", "imacro", "tool", "cursor", "logcommands", "pushbutton",
                           "setpoint", "sleep", "echo", "send", "history", "source", "help", "*bypass", "pushbox",
                           "popbox", "peekbox", "pushstack", "popstack", "clearstack", "suspendall", "resumeall",
                           "ruler", "measure", "unmeasure", "genkey", "autoincr", "*coord", "*watch", "*winddebug",
                           "*files", "feedback", "wire"}) {
    INFO(name);
    CHECK(registry.find(name) != nullptr);
  }
}

TEST_CASE("no registered command is a scaffold stub", "[m2-scaffold]") {
  // Task 11.5: every stub was replaced and CORE-NOTIMPL-002 is retired.
  const auto retired = hefesto::diag::find_code("CORE-NOTIMPL-002");
  REQUIRE(retired.has_value());
  CHECK(retired->summary.starts_with("Retired in M2"));
}

TEST_CASE("the prompt host gives the default answer without a user", "[m2-scaffold]") {
  const std::array<std::string, 2> answers{"no", "yes"};
  CHECK(cmd::ask(nullptr, "Recover?", answers, 0) == 0);
  cmd::HeadlessPromptHost headless;
  CHECK(cmd::ask(&headless, "Recover?", answers, 1) == 1);
  Scripted yes(1);
  CHECK(cmd::ask(&yes, "Recover?", answers, 0) == 1);
  Scripted out_of_range(7);
  CHECK(cmd::ask(&out_of_range, "Recover?", answers, 0) == 0);
}

TEST_CASE("editor state carries the M2 members with their defaults", "[m2-scaffold]") {
  editor::EditorState state;
  CHECK(state.units == cmd::UnitsState{});
  CHECK_FALSE(state.undo.suspended());
  CHECK(state.selection.empty());
  CHECK(state.tools.current() == nullptr);
  CHECK(state.box_stack.empty());
  CHECK(state.edit_stack.empty());
  CHECK(state.suspend_depth == 0);
  CHECK_FALSE(state.command_point.has_value());
  CHECK(state.command_count == 0);
  CHECK(state.feedback.size() == 0);
  CHECK_FALSE(state.color_map.modified);
  CHECK(state.tile_overlays.empty());
  CHECK_FALSE(state.wire.type.has_value());
  CHECK(state.edit_transform == db::kIdentity);
  CHECK(state.edit_root == nullptr);
  CHECK(state.prompt == nullptr);
  REQUIRE(state.undo_recorder != nullptr);
  CHECK(state.cells.recorder() == state.undo_recorder.get());
  state.command_point = cmd::PointerState{1, db::Point{3, 4}, db::Rect{3, 4, 3, 4}};
  CHECK(state.command_point->point == db::Point{3, 4});
}

TEST_CASE("post_command counts user commands, not *bypass or internal ones", "[m2-scaffold]") {
  editor::EditorState state;
  editor::post_command(state, cmd::Invocation{"paint", {"m1"}, cmd::Origin::script, std::nullopt});
  editor::post_command(state, cmd::Invocation{"*bypass", {"box"}, cmd::Origin::gui, std::nullopt});
  editor::post_command(state, cmd::Invocation{"crash", {"save"}, cmd::Origin::internal, std::nullopt});
  editor::post_command(state, cmd::Invocation{"box", {}, cmd::Origin::key, std::nullopt});
  CHECK(state.command_count == 2);
}

TEST_CASE("the edit frame converts between root and edit coordinates", "[m2-scaffold]") {
  editor::EditorState state;
  CHECK(editor::to_root(state, db::Point{5, 6}) == db::Point{5, 6});
  state.edit_transform = db::Transform{0, -1, 100, 1, 0, 0};  // 90 degrees, then (100, 0)
  const db::Point root = editor::to_root(state, db::Point{5, 6});
  CHECK(root == db::Point{94, 5});
  CHECK(editor::to_edit(state, root) == db::Point{5, 6});
  const db::Rect area = editor::to_root(state, db::Rect{0, 0, 10, 20});
  CHECK(editor::to_edit(state, area) == db::Rect{0, 0, 10, 20});
}
