// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the EDIT commands (box, paint, erase and the misc
// editing commands): the box and its root cell, the edit target of a paint
// command and the common effects of edit-cell paint changes
// (cmd-box "Box state", "Box availability to other commands";
// cmd-coordinates-and-units "Root and edit coordinate frames";
// cmd-paint-erase "Common effects of edit-cell paint changes").
// Owner: EDIT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "db/command_util.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/transform.hpp"

namespace hefesto::db::edit {

/// The root cell the edit cell is shown in (Magic's EditRootDef): the
/// edit root, or the edit cell itself when it is the root.
[[nodiscard]] CellDef* edit_root(const editor::EditorState& editor) noexcept;

/// The box's root cell (Magic's boxRootDef); nullptr when there is no box.
[[nodiscard]] CellDef* box_root(const editor::EditorState& editor) noexcept;

/// The root cell a new box gets when there is none: the current window's
/// top cell, else the edit root.
[[nodiscard]] CellDef* default_root(const editor::EditorState& editor) noexcept;

/// DBWSetBox: makes `area` (root coordinates of `root`) the box, or removes
/// it (nullopt), recording the change as an undo event (cmd-box "Box state").
void set_box(editor::EditorState& editor, CellDef* root, std::optional<Rect> area);

/// The box in edit-cell coordinates (ToolGetEditBox): fails with "Box tool
/// must be present" or "The box isn't in a window on the edit cell."
[[nodiscard]] diag::Outcome<Rect> edit_box(const cmdutil::Session& s);

/// What a paint command works on: the edit cell, the box in root and in
/// edit coordinates.
struct Target {
  cmdutil::Session s;
  CellDef* def = nullptr;
  Rect root_area;
  Rect area;
};

/// The target of a paint command. `no_edit_cell` is the command's text for
/// a missing edit cell ("Non-editable context" of cmd-paint-erase).
[[nodiscard]] diag::Outcome<Target> paint_target(cmd::CommandContext& ctx, std::string_view no_edit_cell);

/// PaintOptions of the session (locked types, magic-compat).
[[nodiscard]] PaintOptions paint_options(const cmdutil::Session& s);

/// Which of the common effects a command applies besides the modified flag
/// and the bounding box (cmd-paint-erase "Common effects").
struct Effects {
  bool adjust_labels = true;    // re-attach labels in the area
  bool clear_selection = true;
  bool labels_changed = false;  // labels were erased: redisplay every layer
};

/// The common effects after paint changed in `area` (edit coordinates) of
/// `t.def`: label adjustment with its notes, selection, bounding box and the
/// change-driven redisplay of the changed `layers`.
void finish(const Target& t, const Rect& area, const tech::TypeMask& layers, const Effects& effects = {});

/// One tile found by search_tree(), in root coordinates.
struct FoundTile {
  Rect area;
  tech::TileType type = tech::kSpace;
  std::optional<TriangleSide> side;  // the reported side of a split tile
  const CellDef* def = nullptr;      // the cell holding the tile
  Transform to_root;                 // def -> root coordinates
};

/// Which subcells search_tree() descends into: those expanded in the
/// window showing the root (Magic's DBTreeSrTiles with the window's expand
/// mask; only the root without such a window), or all of them.
enum class Depth : std::uint8_t { expanded, all };

/// DBTreeSrTiles: every tile of a type in `types` overlapping `area` (root
/// coordinates) in `root` and the subcells `depth` selects, plane by plane.
void search_tree(const editor::EditorState& editor, const CellDef& root, const Rect& area,
                 const tech::TypeMask& types, Depth depth, const std::function<void(const FoundTile&)>& visit);

/// The layout window showing `root` (the current window first); nullptr
/// when none does.
[[nodiscard]] const view::LayoutWindow* window_on(const editor::EditorState& editor, const CellDef* root);

/// A diagnostic with code DB-CMD-004 (bad argument) and Magic's usage text.
[[nodiscard]] cmd::CommandResult usage(std::string text);

}  // namespace hefesto::db::edit
