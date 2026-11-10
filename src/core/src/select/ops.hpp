// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private interface of the select area: the context a selection command runs
// in, and the operations that create the selection (create.cpp) and edit the
// layout through it (edit.cpp). Owner: SELECT.

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/connect/tree.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/select/selection.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::select::ops {

/// A parsed layer list of the selection commands: types plus the label and
/// subcell pseudo-layers (Magic's L_LABEL, L_CELL).
struct Layers {
  tech::TypeMask types;
  bool labels = false;
  bool cells = false;
};

/// Everything a selection command needs: the session, the window's root
/// (the selection root it selects in) and its expansion state.
struct Context {
  db::cmdutil::Session s;
  editor::EditorState* ed = nullptr;
  const tech::Technology* tech = nullptr;
  db::CellDef* root = nullptr;
  /// The window's expansion state (empty: no window, everything descends).
  connect::Descend descend;
  /// Types visible in the window (all types without a window).
  tech::TypeMask visible = tech::TypeMask::all();
  bool see_labels = true;
  bool see_cells = true;
};

/// The context of the current window (or, without one, of the edit root).
/// Fails when there is no cell to select in.
[[nodiscard]] diag::Outcome<Context> context(cmd::CommandContext& ctx);

/// The box in root coordinates; nullopt without a box.
[[nodiscard]] std::optional<db::Rect> root_box(const Context& c);
/// Sets the box from root coordinates (DBWSetBox on the window's root).
void set_root_box(const Context& c, const db::Rect& r);

/// Parses a layer list ("*", "labels", "subcell", type names; `space`
/// means "*,labels" per CmdSelect).
[[nodiscard]] diag::Outcome<Layers> parse_layers(const Context& c, std::string_view text);

/// User types excluding space and the DRC types (DBAllButSpaceAndDRCBits).
[[nodiscard]] tech::TypeMask paint_types(const tech::Technology& tech);

/// db::copy_paint with `area` first clipped to the part that `t` maps into
/// the usable plane (a whole-plane copy through a translation).
diag::Status copy_paint(const db::CellDef& source, const db::Rect& area, const db::Transform& t, db::CellDef& target,
                        const tech::Technology& tech, const tech::TypeMask& types);

/// Makes the window root the selection root (clearing on a change) and
/// returns the snapshot to record when the command's change is done.
[[nodiscard]] Snapshot begin(const Context& c);
/// Records the change since `before` and recomputes the selection bbox.
void commit(const Context& c, Snapshot before);

// ---- create.cpp -----------------------------------------------------------------

/// SelectArea: copies the paint, labels and unexpanded uses under `area`.
void select_area(const Context& c, const db::Rect& area, const Layers& layers, const std::string* glob);
/// SelRemoveArea: removes selected items of `layers` under `area`.
void remove_area(const Context& c, const db::Rect& area, const Layers& layers, const std::string* glob);
/// SelectChunk: returns the chunk rectangle (null when none).
db::Rect select_chunk(const Context& c, const db::Rect& area, tech::TileType type, bool less);
/// SelectRegion / SelectNet: returns the net's extended area.
db::Rect select_region(const Context& c, const db::Rect& area, tech::TileType type, bool less);
db::Rect select_net(const Context& c, const db::Rect& area, tech::TileType type, bool less);
/// SelectIntersect.
void select_intersection(const Context& c, const db::Rect& area, tech::TileType type, bool negate);
/// SelectFlat.
void select_flat(const Context& c);
/// The types present under `area` through descended cells (DBSeeTypesAll).
[[nodiscard]] tech::TypeMask types_under(const Context& c, const db::Rect& area);

/// A use of the hierarchy under the root, with its parent's root transform.
struct UseRef {
  db::CellUse* use = nullptr;
  db::Transform parent_to_root;
  std::string path;  // instance path of the use ("a/b")
  db::Point element; // the array element chosen
};
/// DBTreeFindUse: the use named by an instance path from the root.
[[nodiscard]] std::optional<UseRef> find_use_path(const Context& c, std::string_view path);
/// DBSelectCell's candidates at `area`: uses whose area contains it,
/// smallest first (expanded uses and unexpanded ones whose ancestors are all
/// expanded).
[[nodiscard]] std::vector<UseRef> uses_at(const Context& c, const db::Rect& area);
/// SelectCell: adds (or with `replace` swaps in) the whole use.
void select_cell(const Context& c, const UseRef& ref, bool replace);
/// SelectRemoveCellUse.
void unselect_cell(const Context& c, const UseRef& ref);

// ---- edit.cpp ---------------------------------------------------------------------

/// Warnings of the edit-cell restriction ("Edit-cell restriction for
/// selection operations"), printed with `verb` ("deleted", "moved", ...).
struct NonEdit {
  bool paint = false;
  bool cells = false;
  bool labels = false;
};
void warn_non_edit(const Context& c, const NonEdit& n, std::string_view verb);

/// A scratch cell (Magic's Select2Def) in root coordinates.
[[nodiscard]] std::unique_ptr<db::CellDef> scratch(const Context& c);
/// The edit-cell parts of the selection, moved by `t`, into a scratch cell.
[[nodiscard]] std::unique_ptr<db::CellDef> edit_parts(const Context& c, const db::Transform& t, NonEdit* non_edit);
/// The whole selection moved by `t` into a scratch cell (SelectCopy).
[[nodiscard]] std::unique_ptr<db::CellDef> whole_selection(const Context& c, const db::Transform& t);
/// SelectDelete: erases the edit-cell parts; `clear` empties the selection.
void delete_selection(const Context& c, std::string_view verb, bool clear);
/// SelectAndCopy2: paints the scratch into the edit cell and makes it the
/// selection; copied uses whose identifier is taken get a generated one.
void copy_into_edit(const Context& c, const db::CellDef& scratch);
/// SelectAndCopy1 (`select keep`): the whole selection with its offset into
/// the edit cell, the selection unchanged.
void keep_selection(const Context& c);
/// SelectTransform.
void transform_selection(const Context& c, const db::Transform& t);
/// SelectStretch (Manhattan dx, dy).
void stretch_selection(const Context& c, db::Coord dx, db::Coord dy);
/// SelectArray.
void array_selection(const Context& c, const db::ArrayInfo& info);
/// The layout use matching a selected use, and its parent's root transform.
[[nodiscard]] std::optional<UseRef> layout_use(const Context& c, const db::CellUse& selected);

}  // namespace hefesto::select::ops
