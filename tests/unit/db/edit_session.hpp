// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A scripted session for the EDIT command tests (cmd-coordinates-and-units,
// cmd-box, cmd-paint-erase, cmd-misc-edit): the db and cmd commands through
// the dispatcher on the golden test technology `mt` (db_fixture.hpp).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::db::test {

struct EditSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit EditSession(const MtTech& mt) {
    REQUIRE(register_db_commands(registry));
    REQUIRE(cmd::register_units_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = mt.tech;
    editor.edit_cell = cell("top");
  }

  CellDef* cell(const std::string& name) {
    CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;
    return def;
  }

  void compat(bool on) { project.compat.magic_compat = on; }

  /// Splits on single spaces; `{...}` keeps its spaces (one word).
  static cmd::Invocation parse(const std::string& line) {
    cmd::Invocation inv;
    std::string word;
    int depth = 0;
    const auto flush = [&] {
      if (!word.empty()) {
        (inv.name.empty() ? inv.name : inv.args.emplace_back()) = word;
        word.clear();
      }
    };
    for (const char c : line) {
      if (c == '{' && depth++ == 0) {
        continue;
      }
      if (c == '}' && --depth == 0) {
        continue;
      }
      if (c == ' ' && depth == 0) {
        flush();
        continue;
      }
      word += c;
    }
    flush();
    return inv;
  }

  cmd::CommandResult run(const std::string& line) { return dispatcher->execute(parse(line)); }

  cmd::CommandResult ok(const std::string& line) {
    auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
    return r;
  }

  /// The result of `line` as Tcl text.
  std::string value(const std::string& line) { return ok(line).value.to_string(); }

  /// Every diagnostic text of `r`, one per line.
  static std::string texts(const cmd::CommandResult& r) {
    std::string out;
    for (const auto& d : r.diagnostics) {
      out += d.text + "\n";
    }
    return out;
  }

  [[nodiscard]] Rect box() const {
    REQUIRE(editor.box.has_value());
    return *editor.box;
  }
};

/// The rectangles of `type` on `plane` of `def`.
inline std::vector<Rect> rects_of(const CellDef& def, tech::PlaneId plane, tech::TileType type) {
  return rects(def.plane(plane), tech::TypeMask::of(type));
}

}  // namespace hefesto::db::test
