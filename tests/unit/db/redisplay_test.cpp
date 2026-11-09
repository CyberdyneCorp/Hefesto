// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Editing commands report the areas they change to the windows
// (capability ui-windows-and-views "Change-driven redisplay").

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view = hefesto::view;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit Session(const db::test::MtTech& mt) {
    REQUIRE(db::register_db_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = mt.tech;
    editor.edit_cell = editor.cells.create("top", mt.tech->plane_count()).value();
    editor.edit_cell->flags().available = true;
  }

  void ok(std::string name, std::vector<std::string> args) {
    cmd::Invocation inv;
    inv.name = std::move(name);
    inv.args = std::move(args);
    const auto r = dispatcher->execute(inv);
    INFO(inv.name << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }
};

}  // namespace

TEST_CASE("paint, erase and label mark the edited area in the windows",
          "[cap:ui-windows-and-views][req:Change-driven redisplay]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  view::LayoutWindow* w = s.editor.windows.open(s.editor.edit_cell, {0, 0, 99, 99}).value();
  view::LayoutWindow* hidden = s.editor.windows.open(s.editor.edit_cell, {0, 0, 99, 99}).value();
  tech::TypeMask visible = hidden->visible_types();
  visible.set(mt.m1, false);
  hidden->set_visible_types(visible);
  s.ok("snap", {"internal"});
  s.ok("box", {"0", "0", "10", "10"});
  s.ok("paint", {"metal1"});
  CHECK(w->take_redisplay().areas == std::vector<db::Rect>{{0, 0, 10, 10}});
  CHECK(hidden->pending_redisplay().empty());  // metal1 is hidden there

  s.ok("box", {"2", "2", "4", "4"});
  s.ok("label", {"A"});
  // The label's area grown by one, with labels (the label extent).
  CHECK(w->take_redisplay().areas == std::vector<db::Rect>{{1, 1, 5, 5}});
  CHECK(hidden->take_redisplay().areas == std::vector<db::Rect>{{1, 1, 5, 5}});

  s.ok("box", {"0", "0", "10", "10"});
  s.ok("erase", {});  // paint and labels
  CHECK(w->take_redisplay().areas == std::vector<db::Rect>{{0, 0, 10, 10}});
  CHECK(hidden->take_redisplay().areas == std::vector<db::Rect>{{0, 0, 10, 10}});  // labels went too
}
