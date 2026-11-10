// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the undo tests: an editor session with the `mt` test
// technology (tests/unit/db/db_fixture.hpp), an undo::DbRecorder installed
// on its library, the MODEL and undo commands, and a run() that performs
// the post-command update after each command, as the dispatcher will
// (design.md D4; DISPATCH task 7.2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "../db/db_fixture.hpp"
#include "hefesto/cell/commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/editor/post_command.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/undo/commands.hpp"
#include "hefesto/undo/db_recorder.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::undo::test {

struct UndoSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<DbRecorder> recorder;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  db::CellDef* top = nullptr;

  explicit UndoSession(const db::test::MtTech& mt) {
    REQUIRE(db::register_db_commands(registry));
    REQUIRE(register_undo_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = mt.tech;
    recorder = std::make_unique<DbRecorder>(editor.undo);
    recorder->set_compat_source(&project.compat);
    editor.cells.set_recorder(recorder.get());
    top = cell("top");
    editor.edit_cell = top;
  }

  ~UndoSession() { editor.cells.set_recorder(nullptr); }
  UndoSession(const UndoSession&) = delete;
  UndoSession& operator=(const UndoSession&) = delete;

  db::CellDef* cell(const std::string& name) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;
    return def;
  }

  /// Runs one command line (space-separated words) and the post-command
  /// update; returns the result.
  cmd::CommandResult run(const std::string& line) {
    cmd::Invocation inv;
    std::size_t pos = 0;
    while (pos < line.size()) {
      const std::size_t end = line.find(' ', pos);
      std::string tok = line.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
      if (!tok.empty()) {
        (inv.name.empty() ? inv.name : inv.args.emplace_back()) = std::move(tok);
      }
      pos = end == std::string::npos ? line.size() : end + 1;
    }
    auto result = dispatcher->execute(inv);
    editor::post_command(editor, inv);
    return result;
  }

  void ok(const std::string& line) {
    const auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }

  /// One `paint` command over (x0,y0)-(x1,y1). The box is set directly so
  /// that only the paint makes a step (a `box` command records its own).
  void paint(const std::string& layer, db::Coord x0, db::Coord y0, db::Coord x1, db::Coord y1) {
    editor.box = db::Rect{x0, y0, x1, y1};
    ok("paint " + layer);
  }

  [[nodiscard]] std::vector<db::Rect> rects(tech::PlaneId plane) const {
    return db::test::rects(top->plane(plane));
  }
};

/// Every text of `result`'s diagnostics, joined by newlines.
inline std::string texts(const cmd::CommandResult& result) {
  std::string out;
  for (const auto& d : result.diagnostics) {
    if (!out.empty()) {
      out += '\n';
    }
    out += d.text;
  }
  return out;
}

}  // namespace hefesto::undo::test
