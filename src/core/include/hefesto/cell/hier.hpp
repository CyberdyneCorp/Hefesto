// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchy editing helpers shared by the cmd-hierarchy and
// cmd-labels-properties commands (and offered to other areas): the edit
// frame (`edit`, `down`, `load`; design.md D4 `edit_transform`/`edit_root`),
// instance paths (Magic's DBTreeFindUse), the layout items a selection
// stands for (SelEnumCells, SelEnumLabels with editOnly) and the cell and
// instance naming rules (cmd-hierarchy "Cell and instance naming rules").
// (Magic ref: commands/CmdE.c:82-270, database/DBlabel2.c:276-470,
// select/selEnum.c:780-990, commands/CmdSubrs.c:1343-1375)
//
// Owner: HIER (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/transform.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cell {

// --- naming rules ---------------------------------------------------------------

/// CmdIllegalChars(text, bad, what): "<what> contains illegal character
/// \"c\"" for a character of `bad`, "... illegal control character 0x.."
/// for one that is not printable ASCII; nullopt when `text` is legal.
[[nodiscard]] std::optional<std::string> illegal_chars(std::string_view text, std::string_view bad,
                                                       std::string_view what);

/// CmdIllegalChars(name, "[],", "Cell name"): the error text when a cell
/// name holds `[`, `]`, `,` or a character that is not printable; nullopt
/// when it is legal.
[[nodiscard]] std::optional<std::string> illegal_cell_name(std::string_view name);

/// The error text when an instance identifier holds `,` or `/` (or a
/// character that is not printable); nullopt when it is legal.
[[nodiscard]] std::optional<std::string> illegal_use_id(std::string_view id);

// --- instances ------------------------------------------------------------------

/// One instance (array element) of the layout, as Magic's SearchContext
/// after DBTreeFindUse / SelEnumCells.
struct InstanceRef {
  db::CellUse* use = nullptr;
  /// The element's child coordinates to the root's (array element included).
  db::Transform to_root;
  /// The parent's coordinates to the root's.
  db::Transform parent_to_root;
  std::int32_t x = 0;  // array element
  std::int32_t y = 0;
  std::string path;  // instance path from the root, '/'-separated
};

/// Reads an unavailable definition from its file (CELL); may be empty.
using CellReader = std::function<void(db::CellDef&)>;

/// DBTreeFindUse: the instance named by `path` (`x1/x2`, array subscripts
/// `[i]`, `[i][j]` or `[i,j]`) below `root`. A component naming an array
/// without a subscript means its first element. Unavailable cells met on
/// the way are read with `read`. nullopt when a component is not found or
/// its subscript does not match the array.
[[nodiscard]] std::optional<InstanceRef> find_instance(db::CellDef& root, std::string_view path,
                                                       const CellReader& read = {});

/// SelEnumCells: for each instance in the selection, the instance of the
/// layout below the selection root it stands for (same child, identifier,
/// array and transform to the root), in selection order. Selected
/// instances with no layout counterpart are skipped.
[[nodiscard]] std::vector<InstanceRef> selected_instances(editor::EditorState& editor);

/// cmdEditEnumFunc: among `candidates`, the first whose area overlaps
/// `point_area` (root coordinates), narrowed to the array element there;
/// otherwise the last one. nullopt when there is none.
[[nodiscard]] std::optional<InstanceRef> choose_instance(const std::vector<InstanceRef>& candidates,
                                                         const std::optional<db::Rect>& point_area);

// --- selected labels --------------------------------------------------------------

/// One label of the edit cell that a selected label stands for.
struct SelectedLabel {
  db::CellDef* def = nullptr;  // the edit cell
  std::size_t index = 0;       // in def->labels()
  db::Transform to_root;       // the edit cell's labels to root coordinates
  db::Label* copy = nullptr;   // the selection's copy (SelEnumLabelsMirror)
};

/// SelEnumLabels(all types, editOnly = TRUE): the edit-cell labels matching
/// the selected labels (same rectangle, justification and text in root
/// coordinates; failing that, a label differing in one of justification and
/// text). The text of each selected label not found in the layout is
/// appended to `missing` ("The selected label %s is not in the layout.").
[[nodiscard]] std::vector<SelectedLabel> selected_edit_labels(editor::EditorState& editor,
                                                              std::vector<std::string>* missing = nullptr);

// --- edit frame -------------------------------------------------------------------

/// The edit cell, the root its transform maps to, and that transform
/// (EditCellUse's definition, EditRootDef, EditToRootTransform).
struct EditFrame {
  db::CellDef* cell = nullptr;
  db::CellDef* root = nullptr;
  db::Transform to_root;

  friend bool operator==(const EditFrame&, const EditFrame&) = default;
};

[[nodiscard]] EditFrame edit_frame(const editor::EditorState& editor);

/// The identifier of a window's root use (DBWloadWindow's topmost use),
/// which `what`, `select cell` and edit-cell messages report for the top
/// cell itself.
inline constexpr std::string_view kTopUseId = "Topmost cell in the window";

/// DBWUndoOldEdit/DBWUndoNewEdit: makes `frame` the session's edit frame
/// (EditorState::edit_cell, edit_root, edit_transform) and records an edit
/// cell undo event with the previous frame (undo::UndoLog::record) when
/// they differ.
void set_edit_frame(editor::EditorState& editor, const EditFrame& frame);

/// cmd-hierarchy "Edit command" with magic-compat ON: the warning when the
/// box is mapped through an edit transform that matches no instance of the
/// edit cell below the edit root ("Warning: edit transform does not match
/// instance <INSTPATH>; the box is mapped through the previous edit
/// transform"); nullopt when it matches, when the edit cell is the edit
/// root, or with magic-compat OFF (where `edit INSTPATH` sets the full
/// transform). Commands that map the box into the edit cell print it.
[[nodiscard]] std::optional<std::string> edit_transform_warning(const editor::EditorState& editor,
                                                                bool magic_compat);

}  // namespace hefesto::cell
