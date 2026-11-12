// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A project session with its interpreter bound to the command registry
// (script::bind_registry) for the scripting-api tests: every command of the
// registry through the dispatcher, on tests/data/tech/tiny.tech with two
// cifoutput styles and a cifinput style, a window `.layout1.pane.top.magic`
// on cell `top` with the box at (0,0)-(10,20). Diagnostics the dispatcher
// publishes are collected in `printed`.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/app/settings_layers.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::script::test {

/// tests/data/tech/tiny.tech with a cifoutput style of 0.01 um per
/// internal unit and a cifinput style.
inline std::string technology_text() {
  std::ifstream in{std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/tech/tiny.tech", std::ios::binary};
  std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  const std::string empty = "cifoutput\nend\n\ncifinput\nend\n";
  const std::size_t at = text.find(empty);
  REQUIRE(at != std::string::npos);
  text.replace(at, empty.size(),
               "cifoutput\nstyle lambda\n    scalefactor 1\n    layer CM1 m1\n"
               "style coarse\n    scalefactor 10\n    layer CM1 m1\nend\n\n"
               "cifinput\nstyle in variants (a),(b)\n    scalefactor 0.5\n    layer m1 CM1\nend\n");
  return text;
}

struct BindingFixture {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession session{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  Interpreter* interp = nullptr;
  db::CellDef* top = nullptr;

  /// `console`: a console owns the terminal (no batch-mode stubs).
  explicit BindingFixture(bool magic_compat = true, bool with_window = true, bool console = false) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, session);
    dispatcher->set_editor(&editor);
    dispatcher->set_typed_abbreviations(false);
    REQUIRE(services.view().files.write_atomic("/t/bind.tech", technology_text()).ok());
    const cmd::CommandResult loaded = dispatcher->execute(cmd::Invocation{"tech", {"load", "/t/bind.tech", "-noprompt"}, cmd::Origin::script, std::nullopt});
    INFO((loaded.diagnostics.empty() ? std::string{} : loaded.diagnostics.front().text));
    REQUIRE(loaded.ok());
    dispatcher->add_sink(printed);
    editor.script.output.console = console;
    top = editor.cells.create("top", editor.technology->plane_count()).value();
    top->flags().available = true;
    if (with_window) {
      editor.edit_cell = top;
      auto window = editor.windows.open_frame(top);
      REQUIRE(window.ok());
      window.value()->set_name(".layout1.pane.top.magic");
      editor.box = db::Rect{0, 0, 10, 20};
      editor.box_root = top;
    }
    app::install_builtin_defaults(editor.settings);
    editor.path_variables.install_root = "/opt/hefesto";
    interp = &open_project_interpreter(editor, policy());
    const cmd::CommandResult bound = bind_registry(*interp, registry, *dispatcher);
    INFO((bound.diagnostics.empty() ? std::string{} : bound.diagnostics.front().text));
    REQUIRE(bound.ok());
  }

  /// The test data and the temporary directory are granted roots.
  static SandboxPolicy policy() {
    SandboxPolicy p;
    p.granted_roots.push_back(std::filesystem::weakly_canonical(HEFESTO_TEST_DATA_DIR));
    p.granted_roots.push_back(std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()));
    return p;
  }

  ~BindingFixture() {
    // The interpreter refers to the dispatcher and the DRC events: close it first.
    close_project_interpreter(editor);
  }

  BindingFixture(const BindingFixture&) = delete;
  BindingFixture& operator=(const BindingFixture&) = delete;

  /// Evaluates `script`; the result (failed: the Tcl error message).
  cmd::CommandResult eval(std::string_view script) { return interp->evaluate(script); }

  /// The result of `script` as a string; the script must succeed.
  std::string value(std::string_view script) {
    const cmd::CommandResult r = eval(script);
    INFO(std::string{script} + " -> " + (r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text));
    REQUIRE(r.ok());
    return r.value.to_string();
  }

  /// The Tcl error message of `script`; the script must fail.
  std::string error(std::string_view script) {
    const cmd::CommandResult r = eval(script);
    INFO(script);
    REQUIRE_FALSE(r.ok());
    return r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text;
  }

  /// Every diagnostic text the dispatcher published since the last call.
  std::string take_printed() {
    std::string text;
    for (const auto& d : printed.take()) {
      text += d.text + "\n";
    }
    return text;
  }
};

}  // namespace hefesto::script::test
