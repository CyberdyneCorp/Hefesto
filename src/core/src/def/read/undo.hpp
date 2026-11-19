// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The undo events of a DEF read (capability layout-undo "Undoable LEF and
// DEF reads", Q-LEFDEF-22; private to def/read). Changes to an existing
// design cell are recorded by the database recorder as ordinary edits; the
// read adds:
// - CreatedCellEvent, for a design cell the read created: undo moves the
//   cell's paint, labels, properties and uses out and deletes it (as
//   `cellname delete` does: windows showing it show "(UNNAMED)", and the
//   edit cell, box and pending DRC drop it); redo creates it again under
//   its name with them. The read records nothing else for that cell (its
//   recording is suspended from the cell's creation on);
// - DieAreaEvent, for the FIXED_BBOX an existing design cell got (property
//   changes are not recorded otherwise with magic-compat ON).
//
// Owner: DEFR (design D2).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::def::detail {

class CreatedCellEvent final : public undo::Event {
 public:
  explicit CreatedCellEvent(db::CellDef& def) : def_(&def), name_(def.name()) {}

  void undo(editor::EditorState& editor) override;
  void redo(editor::EditorState& editor) override;
  [[nodiscard]] std::string describe() const override { return "def read: created cell " + name_; }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == def_; }

 private:
  struct Use {
    std::string child;
    std::string id;
    db::Transform transform;
    db::ArrayInfo array;
    bool locked = false;
  };

  db::CellDef* def_;  // nullptr while undone
  std::string name_;
  std::vector<db::Plane> planes_;
  std::vector<db::Label> labels_;
  db::PropertyStore properties_;
  db::CellFlags flags_;
  std::vector<Use> uses_;
};

class DieAreaEvent final : public undo::Event {
 public:
  DieAreaEvent(db::CellDef& def, std::optional<db::Rect> before, std::optional<db::Rect> after)
      : def_(&def), before_(before), after_(after) {}

  void undo(editor::EditorState& editor) override { set(editor, before_); }
  void redo(editor::EditorState& editor) override { set(editor, after_); }
  [[nodiscard]] std::string describe() const override { return "def read: FIXED_BBOX of " + def_->name(); }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == def_; }

 private:
  void set(editor::EditorState& editor, const std::optional<db::Rect>& value);

  db::CellDef* def_;
  std::optional<db::Rect> before_;
  std::optional<db::Rect> after_;
};

}  // namespace hefesto::def::detail
