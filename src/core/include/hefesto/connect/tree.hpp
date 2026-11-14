// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchical searches over a cell and the instances it descends into, in
// root coordinates (Magic's DBTreeSrTiles, DBTreeSrLabels, DBTreeSrCells).
// The connectivity searches (connect/search.hpp) and the selection (select/)
// build on them; later the netlist and extraction tools reuse them.
//
// "Descending" follows a predicate that stands for the expansion state of a
// window (Magic's xMask): a use the predicate refuses is visited as a whole
// use by search_tree_cells() and its contents are not searched. An empty
// predicate descends into every use. The uses of each cell are visited in
// the use index's order under the session's `magic_compat`
// (db::uses_in_search_order(def, compat): the bin walk with magic-compat
// ON, the canonical order with it OFF).
// (Magic ref: database/DBcellsrch.c:300-700, database/DBcell.c DBCellSrArea)
//
// Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::connect {

/// Whether a search descends into `use` (its expansion state); empty: always.
using Descend = std::function<bool(const db::CellUse& use)>;

/// One tile found by search_tree_tiles(). `tile` is in the coordinates of
/// `def`; `to_root` maps them to the root. `path` names the instance path
/// from the root (array element names joined by '/', empty for the root).
struct TreeTile {
  const db::CellDef* def = nullptr;
  tech::PlaneId plane = 0;
  db::TileView tile;
  db::Transform to_root;
  const std::string* path = nullptr;
};

/// One label found by search_tree_labels() (same conventions).
struct TreeLabel {
  const db::CellDef* def = nullptr;
  const db::Label* label = nullptr;
  db::Transform to_root;
  const std::string* path = nullptr;
};

/// One use found by search_tree_cells(): `parent_to_root` maps the
/// coordinates of the use's parent to the root (compose it with
/// `use->transform` for the whole use); `path` is the parent's path.
struct TreeCell {
  const db::CellUse* use = nullptr;
  db::Transform parent_to_root;
  const std::string* path = nullptr;
};

/// Return false to stop a search.
using TileFn = std::function<bool(const TreeTile&)>;
using LabelFn = std::function<bool(const TreeLabel&)>;
using CellFn = std::function<bool(const TreeCell&)>;

/// DBTreeSrTiles: every tile of `types` overlapping `area` (root
/// coordinates) in the root and every descended cell, root first, then the
/// uses in Magic's search order and each array element. With `triangle`,
/// only the tiles overlapping that triangle of `area` (DBTreeSrNMTiles).
/// Returns false when the visitor stopped the search.
bool search_tree_tiles(const db::CellDef& root, const db::Rect& area, const tech::TypeMask& types,
                       const Descend& descend, bool magic_compat, const TileFn& visit,
                       std::optional<db::TriangleSide> triangle = std::nullopt);

/// DBTreeSrLabels with TF_LABEL_ATTACH: every label whose type is in
/// `types` and whose rectangle touches `area` (root coordinates), in the
/// root and every descended cell.
bool search_tree_labels(const db::CellDef& root, const db::Rect& area, const tech::TypeMask& types,
                        const Descend& descend, bool magic_compat, const LabelFn& visit);

/// DBTreeSrCells: every use overlapping `area` that the search does not
/// descend into (an unexpanded use; with an empty predicate, none), found
/// through the descended cells.
bool search_tree_cells(const db::CellDef& root, const db::Rect& area, const Descend& descend, bool magic_compat,
                       const CellFn& visit);

/// The root and every descended cell instance (array element) overlapping
/// `area` (root coordinates), depth first: `visit(def, to_root, path)`.
using DefFn = std::function<bool(const db::CellDef& def, const db::Transform& to_root, const std::string& path)>;
bool search_tree_defs(const db::CellDef& root, const db::Rect& area, const Descend& descend, bool magic_compat,
                      const DefFn& visit);

/// A label moved by `t` (GeoTransRect/GeoTransPos/GeoTransAngle and
/// GeoTransPointDelta of the offset, as dbCopyAllLabels does): rectangle,
/// justification, rotation, offset and rendered text box.
[[nodiscard]] db::Label transform_label(const db::Label& label, const db::Transform& t, bool magic_compat);

/// `root_area` in the coordinates of a cell whose transform to the root is
/// `to_root`, clipped to the usable plane (a whole-plane area transformed
/// by a translation would otherwise leave it).
[[nodiscard]] db::Rect to_local(const db::Transform& to_root, const db::Rect& root_area);

/// The path of an array element below `parent_path` ("a/b[1,0]").
[[nodiscard]] std::string child_path(const std::string& parent_path, const db::CellUse& use, std::int32_t x,
                                     std::int32_t y);

}  // namespace hefesto::connect
