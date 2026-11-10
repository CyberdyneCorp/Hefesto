// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the HIER command tests (cmd-hierarchy,
// cmd-labels-properties): the MODEL and CELL commands on a dispatcher over
// in-memory files (tiny technology, cell search path /work), one window
// showing `top` (the edit cell), and helpers that build hierarchies, set
// the box and fill the selection the way the SELECT area stores it.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cell_fixture.hpp"
#include "hefesto/cell/commands.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/change_recorder.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::cell::test {

struct HierSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  db::CellDef* top = nullptr;
  view::WindowId window = 0;

  explicit HierSession(bool magic_compat = true) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(db::register_db_commands(registry));
    REQUIRE(register_cell_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = tiny_technology();
    editor.cell_session.host = make_memory_file_host(services.files(), "/work");
    editor.search_paths.path = {"/work"};
    editor.snap = editor::SnapMode::internal;
    top = cell("top");
    auto w = editor.windows.open(top, view::ScreenRect{0, 0, 999, 999});
    REQUIRE(w.ok());
    window = w.value()->id();
    editor.edit_cell = top;
    editor.edit_root = top;
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  /// A new, available cell.
  db::CellDef* cell(const std::string& name) {
    auto made = editor.cells.create(name, editor.technology->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return made.value();
  }

  void paint(db::CellDef& def, std::string_view layer, const db::Rect& r) {
    REQUIRE(db::paint(def, *editor.technology, r, type(layer)).ok());
    db::recompute_bbox(def, true);
  }

  db::CellUse* place(db::CellDef& parent, db::CellDef& child, const db::Transform& t, std::string id = {}) {
    db::PlaceOptions po;
    po.transform = t;
    if (!id.empty()) {
      po.id = std::move(id);
    }
    auto use = db::place(parent, child, po);
    REQUIRE(use.ok());
    db::recompute_bbox(parent, true);
    return use.value();
  }

  db::Label& label(db::CellDef& def, const std::string& text, const db::Rect& r, db::Position just = db::kCenter) {
    db::Label l;
    l.text = text;
    l.rect = r;
    l.justification = just;
    def.labels().push_back(l);
    return def.labels().back();
  }

  void box(db::Coord x0, db::Coord y0, db::Coord x1, db::Coord y1) {
    editor.box = db::Rect{x0, y0, x1, y1};
    editor.box_root = nullptr;
  }

  /// Selects `use` (a use of the window's root hierarchy, `to_root` its
  /// parent's transform to the root) as SELECT stores it: a copy in the
  /// selection cell in root coordinates.
  void select_use(const db::CellUse& use, const db::Transform& parent_to_root = db::kIdentity) {
    editor.selection.set_root(top, editor.technology->plane_count());
    db::PlaceOptions po;
    po.id = use.id;
    po.transform = db::compose(parent_to_root, use.transform);
    po.array = use.array;
    auto copy = db::place(*editor.selection.def(), *use.def, po);
    REQUIRE(copy.ok());
    editor.selection.provenance().push_back(
        select::Provenance{select::Provenance::Kind::use, use.parent, use.id, copy.value()->bbox});
  }

  /// Selects a label of the root hierarchy (its copy in root coordinates).
  void select_label(const db::Label& label, const db::Transform& to_root = db::kIdentity) {
    editor.selection.set_root(top, editor.technology->plane_count());
    db::Label copy = label;
    copy.rect = db::apply(to_root, label.rect);
    copy.justification = db::transform_position(to_root, label.justification);
    editor.selection.def()->labels().push_back(copy);
    editor.selection.provenance().push_back(
        select::Provenance{select::Provenance::Kind::label, nullptr, {}, copy.rect});
  }

  cmd::CommandResult run(const std::string& line) {
    cmd::Invocation inv;
    std::istringstream words(line);
    std::string word;
    while (words >> word) {
      (inv.name.empty() ? inv.name : inv.args.emplace_back()) = word;
    }
    return dispatcher->execute(inv);
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args) {
    return dispatcher->execute(cmd::Invocation{std::move(name), std::move(args), cmd::Origin::script, std::nullopt});
  }

  void ok(const std::string& line) {
    const auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }

  void write(const std::string& path, const std::string& text) {
    REQUIRE(services.files().write_atomic(path, text).ok());
  }
};

/// The text of the last diagnostic of `r` ("" when none).
inline std::string last_message(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text;
}

inline bool mentions(const cmd::CommandResult& r, std::string_view text) {
  for (const auto& d : r.diagnostics) {
    if (d.text.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

/// A recorder that keeps every label and property call (db::ChangeRecorder).
struct LabelLog final : db::NullChangeRecorder {
  std::vector<std::string> events;
  void label_put(db::CellDef& def, const db::Label& l) override { events.push_back("put " + def.name() + " " + l.text); }
  void label_erased(db::CellDef& def, const db::Label& l) override {
    events.push_back("erase " + def.name() + " " + l.text);
  }
  void property_changed(db::CellDef& def, std::string_view name, const std::optional<db::PropertyValue>& old_value,
                        const std::optional<db::PropertyValue>& new_value) override {
    events.push_back("property " + def.name() + " " + std::string{name} + (old_value ? " had" : " new") +
                     (new_value ? " set" : " removed"));
  }
};

}  // namespace hefesto::cell::test
