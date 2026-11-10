// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Root/edit frame conversion (design.md D4). Owner: INTEGRATION.

#include "hefesto/editor/edit_frame.hpp"

#include "hefesto/db/transform.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::editor {

db::Point to_edit(const EditorState& editor, const db::Point& root_point) {
  return db::apply(db::invert(editor.edit_transform), root_point);
}

db::Rect to_edit(const EditorState& editor, const db::Rect& root_area) {
  return db::apply(db::invert(editor.edit_transform), root_area);
}

db::Point to_root(const EditorState& editor, const db::Point& edit_point) {
  return db::apply(editor.edit_transform, edit_point);
}

db::Rect to_root(const EditorState& editor, const db::Rect& edit_area) {
  return db::apply(editor.edit_transform, edit_area);
}

}  // namespace hefesto::editor
