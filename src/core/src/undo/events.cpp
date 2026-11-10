// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// undo::CompoundEvent and undo::BoxEvent (layout-undo). Owner: UNDO.
// Box events follow Magic VLSI 8.3.694 (fc5daa9e) dbwind/DBWundo.c
// (dbwUndoBoxForw/Back), Copyright (C) 1985, 1990 Regents of the University
// of California (permissive license, THIRD_PARTY_NOTICES.md).

#include "hefesto/undo/events.hpp"

#include <algorithm>
#include <string>

#include "hefesto/db/cell.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::undo {

namespace {

std::string box_text(const BoxState& s) {
  if (!s.box) {
    return "none";
  }
  const db::Rect& r = *s.box;
  std::string text = "(" + std::to_string(r.x0) + "," + std::to_string(r.y0) + ")-(" + std::to_string(r.x1) + "," +
                     std::to_string(r.y1) + ")";
  if (s.root != nullptr) {
    text += " in " + s.root->name();
  }
  return text;
}

void apply_box(editor::EditorState& editor, const BoxState& s) {
  editor.box = s.box;
  editor.box_root = editor.undo.is_live(s.root) ? s.root : nullptr;
}

}  // namespace

void CompoundEvent::undo(editor::EditorState& editor) {
  for (auto it = parts_.rbegin(); it != parts_.rend(); ++it) {
    (*it)->undo(editor);
  }
}

void CompoundEvent::redo(editor::EditorState& editor) {
  for (auto& part : parts_) {
    part->redo(editor);
  }
}

std::string CompoundEvent::describe() const {
  return name_ + " (" + std::to_string(parts_.size()) + " parts)";
}

bool CompoundEvent::references(const db::CellDef& def) const {
  return std::any_of(parts_.begin(), parts_.end(), [&](const auto& p) { return p->references(def); });
}

void BoxEvent::undo(editor::EditorState& editor) {
  apply_box(editor, before_);
}

void BoxEvent::redo(editor::EditorState& editor) {
  apply_box(editor, after_);
}

std::string BoxEvent::describe() const {
  return "box " + box_text(before_) + " -> " + box_text(after_);
}

BoxState box_state(const editor::EditorState& editor) {
  return BoxState{editor.box, editor.box_root};
}

void record_box_change(editor::EditorState& editor, const BoxState& before) {
  const BoxState after = box_state(editor);
  if (after != before) {
    editor.undo.record(std::make_unique<BoxEvent>(before, after));
  }
}

}  // namespace hefesto::undo
