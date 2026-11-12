// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test session of the tools area: every command registered, an in-memory
// file system with the working directory /work, and a small technology with
// `poly` and `metal1`. Owner: TOOLS.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/host.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/ui/line.hpp"

namespace hefesto::tools::test {

inline std::shared_ptr<const tech::Technology> small_tech() {
  tech::TechnologyBuilder b;
  b.set_name("small");
  bool ok = b.add_plane("active").ok() && b.add_plane("metal1").ok();
  ok = ok && b.add_type("active", "poly").ok() && b.add_type("metal1", "metal1").ok();
  REQUIRE(ok);
  auto t = b.finalize();
  REQUIRE(t.ok());
  return t.value();
}

struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->set_typed_abbreviations(false);
    editor.technology = small_tech();
    editor.cell_session.host = cell::make_memory_file_host(services.files(), "/work");
    REQUIRE(services.files().create_directories("/work").ok());
  }

  /// Runs a command line; the result of its last command.
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

  void write(const std::string& path, std::string_view text) {
    REQUIRE(services.files().write_atomic(path, text).ok());
  }

  /// A cell `name` (in memory, available) with a metal1 label per entry of
  /// `ports` at x = 10 * i; entries with a positive index are ports.
  void cell_with_labels(const std::string& name, const std::vector<std::pair<std::string, int>>& ports) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;
    editor.edit_cell = def;
    ok("snap internal");
    ok("box 0 0 " + std::to_string(10 * static_cast<int>(ports.size()) + 4) + " 4");
    ok("paint metal1");
    for (std::size_t i = 0; i < ports.size(); ++i) {
      const std::string x = std::to_string(10 * i + 2);
      ok("box " + x + " 2 " + x + " 2");
      ok("label " + ports[i].first + " north metal1");
      if (ports[i].second > 0) {
        ok("port make " + std::to_string(ports[i].second));
      }
    }
    def->flags().modified = false;
  }

  /// The port index of the label `text` of `cell` (0 when not a port).
  std::uint32_t index_of(const std::string& cell, const std::string& text) {
    const db::CellDef* def = editor.cells.find(cell);
    REQUIRE(def != nullptr);
    for (const db::Label& l : def->labels()) {
      if (l.text == text) {
        return l.is_port() ? l.port_index : 0;
      }
    }
    FAIL("no label " << text);
    return 0;
  }
};

/// The texts of the diagnostics of `result` with code prefix `prefix`.
inline std::vector<std::string> texts(const cmd::CommandResult& result, std::string_view prefix) {
  std::vector<std::string> out;
  for (const auto& d : result.diagnostics) {
    if (d.code.starts_with(prefix)) {
      out.push_back(d.text);
    }
  }
  return out;
}

}  // namespace hefesto::tools::test
