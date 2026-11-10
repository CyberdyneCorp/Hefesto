// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Root and edit coordinate frames (capability cmd-coordinates-and-units,
// requirement "Root and edit coordinate frames"; cmd-hierarchy "Edit
// command"; design.md D4). Commands take and report coordinates in the
// root frame unless they say otherwise (`box -edit`, `polygon`, ...);
// these convert through editor::EditorState::edit_transform, which HIER
// maintains when the edit cell changes (`edit`, `load`, `down`).
//
// Owner: INTEGRATION (shared, read by every area); HIER maintains the state.

#pragma once

#include "hefesto/db/geometry.hpp"

namespace hefesto::editor {

struct EditorState;

/// Root coordinates to edit-cell coordinates.
[[nodiscard]] db::Point to_edit(const EditorState& editor, const db::Point& root_point);
[[nodiscard]] db::Rect to_edit(const EditorState& editor, const db::Rect& root_area);

/// Edit-cell coordinates to root coordinates.
[[nodiscard]] db::Point to_root(const EditorState& editor, const db::Point& edit_point);
[[nodiscard]] db::Rect to_root(const EditorState& editor, const db::Rect& edit_area);

}  // namespace hefesto::editor
