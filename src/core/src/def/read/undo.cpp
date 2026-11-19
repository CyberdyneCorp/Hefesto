// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The undo events of a DEF read (def/read/undo.hpp; Q-LEFDEF-22).
//
// Owner: DEFR (design D2).

#include "def/read/undo.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::def::detail {

namespace {

// What `cellname delete` clears besides the definition (DBCellDelete,
// WindUnload): windows showing it show "(UNNAMED)", and the edit cell, the
// box and the pending DRC areas drop it.
void forget(editor::EditorState& editor, const db::CellDef& def) {
  editor.windows.for_each([&](view::LayoutWindow& w) {
    if (w.root() != &def) {
      return;
    }
    db::CellDef* unnamed = editor.cells.find("(UNNAMED)");
    if (unnamed == nullptr && editor.technology != nullptr) {
      auto made = editor.cells.create("(UNNAMED)", editor.technology->plane_count());
      unnamed = made.ok() ? made.value() : nullptr;
      if (unnamed != nullptr) {
        unnamed->flags().available = true;
      }
    }
    w.set_root(unnamed);
  });
  (void)editor.drc.pending.remove(def);
  if (editor.edit_cell == &def) {
    editor.edit_cell = nullptr;
    editor.current_use.clear();
  }
  if (editor.edit_root == &def) {
    editor.edit_root = nullptr;
  }
  if (editor.box_root == &def) {
    editor.box_root = nullptr;
    editor.box.reset();
  }
}

}  // namespace

void CreatedCellEvent::undo(editor::EditorState& editor) {
  if (def_ == nullptr || !editor.undo.is_live(def_) || !editor.cells.check_removable(*def_).ok()) {
    return;  // deleted since, or placed in another cell: it stays
  }
  db::CellDef& def = *def_;
  planes_.clear();
  for (std::size_t p = 0; p < def.plane_count(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    planes_.push_back(std::move(def.plane(id)));
    def.plane(id) = db::Plane{};
  }
  labels_ = std::move(def.labels());
  def.labels().clear();
  properties_ = def.properties();
  flags_ = def.flags();
  uses_.clear();
  while (!def.uses().empty()) {
    const db::CellUse& u = *def.uses().back();
    uses_.insert(uses_.begin(), Use{u.def->name(), u.id, u.transform, u.array, u.locked});
    (void)db::remove_use(*def.uses().back());
  }
  forget(editor, def);
  if (editor.cells.remove(def).ok()) {
    def_ = nullptr;
  }
}

void CreatedCellEvent::redo(editor::EditorState& editor) {
  if (def_ != nullptr || editor.cells.find(name_) != nullptr) {
    return;  // a cell of that name was made since
  }
  auto made = editor.cells.create(name_, planes_.size());
  if (!made.ok()) {
    return;
  }
  db::CellDef& def = *made.value();
  for (std::size_t p = 0; p < std::min(planes_.size(), def.plane_count()); ++p) {
    def.plane(static_cast<tech::PlaneId>(p)) = std::move(planes_[p]);
  }
  planes_.clear();
  def.labels() = std::move(labels_);
  labels_.clear();
  def.properties() = properties_;
  def.flags() = flags_;
  for (const Use& u : uses_) {
    db::CellDef* child = editor.cells.find(u.child);
    if (child == nullptr) {
      continue;  // deleted since
    }
    db::PlaceOptions options;
    options.id = u.id;
    options.transform = u.transform;
    options.array = u.array;
    options.locked = u.locked;
    (void)db::place(def, *child, options);
  }
  db::recompute_bbox(def, editor.drc.compat_settings().magic_compat);
  db::mark_modified(def);
  editor.undo.note_changed(def, def.bbox());
  def_ = &def;
}

void DieAreaEvent::set(editor::EditorState& editor, const std::optional<db::Rect>& value) {
  if (!editor.undo.is_live(def_)) {
    return;
  }
  std::optional<db::PropertyValue> property;
  if (value) {
    property = db::PropertyValue{db::DimensionList{{value->x0, value->y0, value->x1, value->y1}}};
  }
  db::set_property(*def_, "FIXED_BBOX", std::move(property));
  editor.undo.note_changed(*def_, db::Rect{});
}

}  // namespace hefesto::def::detail
