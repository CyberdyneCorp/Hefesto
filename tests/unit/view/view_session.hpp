// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A dispatcher session with the view commands for the M2 VIEW tests
// (feedback, highlights, color window, wizard commands).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view/commands.hpp"

namespace hefesto::test {

struct ViewSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  ViewSession() {
    REQUIRE(view::register_view_commands(registry));
    REQUIRE(view::register_view_and_zoom(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args = {}) {
    cmd::Invocation inv;
    inv.name = std::move(name);
    inv.args = std::move(args);
    return dispatcher->execute(inv);
  }

  db::CellDef* cell(const std::string& name, const db::Rect& bbox) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->set_bboxes(bbox, bbox);
    return def;
  }

  view::LayoutWindow& window(db::CellDef* root, const view::ScreenRect& screen = {0, 0, 499, 399}) {
    auto w = editor.windows.open(root, screen);
    REQUIRE(w.ok());
    return *w.value();
  }
};

/// Every diagnostic text of a result, joined with newlines.
inline std::string all_text(const cmd::CommandResult& r) {
  std::string out;
  for (const auto& d : r.diagnostics) {
    out += (out.empty() ? "" : "\n") + d.text;
  }
  return out;
}

}  // namespace hefesto::test
