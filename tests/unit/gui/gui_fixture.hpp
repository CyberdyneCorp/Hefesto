// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test session of the gui area: every command registered, an in-memory file
// system with the working directory /work, a small technology (`poly`,
// `metal1`), and helpers to run a model function inside a command context.
// Owner: GUIMGR.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "hefesto/cell/host.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/ui/line.hpp"

namespace hefesto::gui::test {

struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  std::function<cmd::CommandResult(cmd::CommandContext&)> probe;

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    REQUIRE_FALSE(registry
                      .add({"gui_test_probe", "", "",
                            [this](cmd::CommandContext& ctx, const cmd::Invocation&) { return probe(ctx); }})
                      .has_value());
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->set_typed_abbreviations(false);
    tech::TechnologyBuilder b;
    b.set_name("small");
    REQUIRE((b.add_plane("active").ok() && b.add_plane("metal1").ok()));
    REQUIRE((b.add_type("active", "poly").ok() && b.add_type("metal1", "metal1").ok()));
    editor.technology = b.finalize().value();
    editor.cell_session.host = cell::make_memory_file_host(services.files(), "/work");
    REQUIRE(services.files().create_directories("/work").ok());
  }

  /// Opens the project interpreter (the wrapper's state lives in it).
  script::Interpreter& interp() {
    if (!editor.script.interp) {
      (void)script::open_project_interpreter(editor, script::SandboxPolicy{});
    }
    return *editor.script.interp;
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  void ok(std::string_view line) {
    const auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }

  /// Runs `fn` inside a dispatched command (a CommandContext of this session).
  cmd::CommandResult in_command(std::function<cmd::CommandResult(cmd::CommandContext&)> fn) {
    probe = std::move(fn);
    return run("gui_test_probe");
  }

  void write(const std::string& path, std::string_view text) {
    REQUIRE(services.files().write_atomic(path, text).ok());
  }

  std::string read(const std::string& path) {
    auto text = services.files().read(path);
    REQUIRE(text.ok());
    return text.value();
  }

  /// Loads a technology with a CIF output style (scalefactor 10: 0.1 µm
  /// per internal unit), so that micron coordinates can be parsed.
  void load_cif_tech() {
    write("/work/cift.tech",
          "tech\n format 35\n cift\nend\nplanes\n metal1,m1\nend\ntypes\n metal1 metal1,m1\nend\n"
          "contact\nend\nstyles\n styletype mos\n metal1 20\nend\ncompose\nend\nconnect\nend\n"
          "cifoutput\nstyle gdsii\n scalefactor 10\n layer CM1 metal1\n calma 1 0\nend\n"
          "cifinput\nend\ndrc\nend\nextract\nend\n");
    ok("tech load /work/cift.tech");
  }

  db::CellDef* cell(const std::string& name) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;
    return def;
  }
};

inline std::string all_text(const cmd::CommandResult& r) {
  std::string out;
  for (const auto& d : r.diagnostics) {
    out += d.text + "\n";
  }
  return out;
}

}  // namespace hefesto::gui::test
