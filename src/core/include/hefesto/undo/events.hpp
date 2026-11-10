// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Reusable undo events (capability layout-undo): the native compound event
// for Hefesto operations with no Magic counterpart ("Native undo events for
// Hefesto compound operations"), and the box event every command that moves
// the box records ("Recorded event kinds": changes of the box; Magic's
// dbwind/DBWundo.c box client). Other editor-level events (selection, edit
// cell, wiring, color map) are defined by their areas on undo::Event.
//
// Owner: UNDO (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::undo {

/// A sequence of events replayed as one: undone newest first, redone
/// oldest first. Producers of native operations (agent proposals, `sim`
/// testbench edits, later milestones) build one with the parts that
/// ordinary commands do not record (cell creation, rename, deletion,
/// properties) and record it with UndoLog::record(); the command that
/// applies the operation is one step whatever the magic-compat setting.
class CompoundEvent final : public Event {
 public:
  explicit CompoundEvent(std::string name) : name_(std::move(name)) {}

  void add(std::unique_ptr<Event> event) {
    if (event != nullptr) {
      parts_.push_back(std::move(event));
    }
  }
  [[nodiscard]] std::size_t size() const noexcept { return parts_.size(); }

  void undo(editor::EditorState& editor) override;
  void redo(editor::EditorState& editor) override;
  [[nodiscard]] std::string describe() const override;
  [[nodiscard]] bool references(const db::CellDef& def) const override;

 private:
  std::string name_;
  std::vector<std::unique_ptr<Event>> parts_;
};

/// The box and its root cell before and after a change
/// (editor::EditorState::box and box_root).
struct BoxState {
  std::optional<db::Rect> box;
  db::CellDef* root = nullptr;

  friend bool operator==(const BoxState&, const BoxState&) = default;
};

/// A box change (Magic's DBWundo box events): undo restores `before`,
/// redo `after`. A root cell deleted since the change counts as no root;
/// deleting it does not clear the history (Magic resets the box instead,
/// DBWResetBox), so references() stays false.
class BoxEvent final : public Event {
 public:
  BoxEvent(const BoxState& before, const BoxState& after) : before_(before), after_(after) {}

  void undo(editor::EditorState& editor) override;
  void redo(editor::EditorState& editor) override;
  [[nodiscard]] std::string describe() const override;

 private:
  BoxState before_;
  BoxState after_;
};

/// The current box state of `editor`.
[[nodiscard]] BoxState box_state(const editor::EditorState& editor);

/// Records the change from `before` to the editor's current box, if any
/// (call after changing editor.box / editor.box_root).
void record_box_change(editor::EditorState& editor, const BoxState& before);

}  // namespace hefesto::undo
