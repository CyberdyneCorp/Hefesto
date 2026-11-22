// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test session of the `wind3d` client: every command registered, the
// project's Tcl interpreter bound to them with hefesto-cli's window-command
// hook, and the MISC-3D technology (tools/golden/authoring/m10_cases.py:
// output styles `gdsii` with scalefactor 10 and `coarse` with 5, `render
// MET1 metal1 1.376 0.36`, an extract style with heights) with the cell c3d
// of bounding box (0,0)-(100,50), loaded and boxed. Owner: WIND3D.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/binding/window_commands.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view3d/client/commands.hpp"

namespace hefesto::view3d::test {

inline constexpr std::string_view kTech = R"(tech
    format 35
    v3d
end
planes
    active,a
    metal1,m1
    metal2,m2
end
types
    active  polysilicon,poly
    metal1  metal1,m1
    metal2  metal2,m2
end
styles
    styletype mos
    poly 1
    metal1 20
    metal2 21
end
contact
end
compose
end
connect
    poly    poly
    metal1  metal1
    metal2  metal2
end
cifoutput
style gdsii
    scalefactor 10
    layer POLY poly
        calma 66 20
    layer MET1 m1
        calma 68 20
    layer MET2 m2
        calma 69 20
    render MET1 metal1 1.376 0.36
style coarse
    scalefactor 5
    layer MET1 m1
        calma 68 20
style nano
    scalefactor 10 nanometers
    layer MET1 m1
        calma 68 20
end
cifinput
end
drc
end
extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    height poly 0.2 0.1
    height metal1 0.5 0.3
end
)";

/// Counts what the client asks of the front end's display.
struct CountingDisplay final : WindowDisplay {
  int redraws = 0;
  int closes = 0;
  WindowState last;
  void redraw(const editor::EditorState& /*editor*/, const WindowState& window) override {
    ++redraws;
    last = window;
  }
  void closed(const WindowState& window) override {
    ++closes;
    last = window;
  }
};

struct ClientSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession session{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  script::Interpreter* interp = nullptr;
  db::CellDef* cell = nullptr;
  CountingDisplay display;

  explicit ClientSession(bool magic_compat = true, std::string_view tech_text = kTech) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, session);
    dispatcher->set_editor(&editor);
    dispatcher->set_typed_abbreviations(false);
    REQUIRE(services.view().files.write_atomic("/t/v3d.tech", tech_text).ok());
    ok("tech load /t/v3d.tech -noprompt");
    cell = editor.cells.create("c3d", editor.technology->plane_count()).value();
    cell->flags().available = true;
    paint("metal1", db::Rect{0, 0, 100, 50});
    paint("metal2", db::Rect{20, 10, 60, 40});
    editor.edit_cell = cell;
    editor.box = db::Rect{0, 0, 100, 50};
    editor.box_root = cell;
    dispatcher->add_sink(printed);
    interp = &script::open_project_interpreter(editor, script::SandboxPolicy{});
    REQUIRE(script::bind_registry(*interp, registry, *dispatcher).ok());
    // hefesto-cli's wrapper mode: each announced window path becomes a command.
    interp->define_command("::hefesto::tk::window_command", [this](const cmd::Invocation& inv) {
      return script::register_window_commands(*interp, *dispatcher, inv.args);
    });
    editor.view3d.display = &display;
  }
  ~ClientSession() { script::close_project_interpreter(editor); }
  ClientSession(const ClientSession&) = delete;
  ClientSession& operator=(const ClientSession&) = delete;

  void paint(std::string_view type, const db::Rect& r) {
    const auto t = editor.technology->find_type(type);
    REQUIRE(t.id);
    REQUIRE(db::paint(*cell, *editor.technology, r, *t.id).ok());
    db::recompute_bbox(*cell);
  }

  /// Runs a command line (layout client); the result of its last command.
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }
  void ok(std::string_view line) {
    const cmd::CommandResult r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }

  /// Runs a command line sent to the 3D window (the `wind3d` table only,
  /// as the binding sends `.magic3d <cmd>`).
  cmd::CommandResult w3d(std::string_view line) {
    const std::string client = dispatcher->client();
    dispatcher->set_client(std::string{kWind3dClient});
    dispatcher->set_window_commands(false);
    cmd::CommandResult r = run(line);
    dispatcher->set_client(client);
    dispatcher->set_window_commands(true);
    return r;
  }
  /// The value of a 3D window command that must succeed, as Tcl prints it.
  std::string w3d_value(std::string_view line) {
    const cmd::CommandResult r = w3d(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
    return r.value.to_string();
  }

  /// Evaluates Tcl; the result (failed: the Tcl error message).
  cmd::CommandResult eval(std::string_view script) { return interp->evaluate(script); }
  std::string value(std::string_view script) {
    const cmd::CommandResult r = eval(script);
    INFO(script << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text));
    REQUIRE(r.ok());
    return r.value.to_string();
  }

  /// Every diagnostic published since the last call, one per line, each as
  /// "CODE: text".
  std::string take_printed() {
    std::string out;
    for (const diag::Diagnostic& d : printed.take()) {
      out += d.code + ": " + d.text + "\n";
    }
    return out;
  }

  [[nodiscard]] WindowState& window() {
    REQUIRE(editor.view3d.window.has_value());
    return *editor.view3d.window;
  }
};

/// The first error diagnostic of `r` as "CODE: text"; empty if none.
inline std::string first_error(const cmd::CommandResult& r) {
  for (const diag::Diagnostic& d : r.diagnostics) {
    if (d.severity == diag::Severity::error) {
      return d.code + ": " + d.text;
    }
  }
  return {};
}

}  // namespace hefesto::view3d::test
