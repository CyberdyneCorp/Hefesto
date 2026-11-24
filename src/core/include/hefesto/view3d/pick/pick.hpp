// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Picking, the 3D <-> 2D link and net highlighting (hefesto-3d-visualization
// "Interactive 3D inspection"; openspec/changes/m10-3d-view/design.md D10).
//
// Each mesh instance carries a 32-bit PickId (pick_id.hpp: 1 + its ordinal
// in the model); the renderer writes it to its id image and a click reads
// one pixel. resolve() maps an id back to a PickTarget: the instance's
// layer and root rectangle from the model and, given a PickContext (the
// layer stack the model was built from and the rendered cell), the tile
// type (the (type, style) pair mapped back, Q-MISC-77) and the source tile
// (instance path, cell-local and root rectangles), found by one
// hierarchical tile search of the rendered cell under the picked face. No
// side table is kept with the mesh.
// - to_selection (3D -> 2D): selects the target's material as `select area
//   <type>` over the source tile would when the tile is in the rendered
//   cell; under a subcell, the rendered cell's child use on the target's
//   path (Magic's selection of unexpanded material). link_to_2d also
//   returns the area the 2D view scrolls to.
// - from_selection (2D -> 3D): the instances overlapping the selection's
//   flattened paint, per layer; given a PickContext, only a selection
//   rooted in the rendered cell.
// - net_highlight: the instances of the net through `connect` (flat or
//   hierarchical search); isolate_others gives the rest, for "Isolate
//   selected net".
//
// Owner: SCAFFOLD (this header, M10 task 2.3); INSPECT fills in
// src/core/src/view3d/pick/ (design D3, tasks 9.3-9.5) and extended this
// header in round 3 (additions only).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/mode.hpp"
#include "hefesto/view3d/pick/pick_id.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::select {
class Selection;
}

namespace hefesto::view3d {

/// What a pick id stands for.
struct PickTarget {
  LayerId layer = 0;                     // the stack entry
  std::optional<tech::TileType> type;    // layout-layer mode: the tile type (pair mapped back, Q-MISC-77)
  db::Rect root_rect;                    // the face's rectangle (bbox for a triangle or side), root coordinates
  bool triangle = false;                 // a split-tile triangle
  std::vector<std::string> use_path;     // layout-layer mode: the instance path from the rendered cell
  db::Rect cell_rect;                    // the tile's rectangle in the cell that holds it
  db::Rect material;                     // the same tile in root coordinates (null: not found)
  db::CellDef* cell = nullptr;           // the rendered cell, root of use_path

  friend bool operator==(const PickTarget&, const PickTarget&) = default;
};

/// A set of instances to draw highlighted (sorted, unique).
struct HighlightSet {
  std::vector<PickId> ids;

  [[nodiscard]] bool empty() const noexcept { return ids.empty(); }
};

/// What resolve() needs beyond the model: the layer stack the model was
/// built from (maps a layer back to its tile type) and the rendered cell
/// (where the source tile is looked up). Without them only the layer, the
/// root rectangle and the triangle flag are filled in.
struct PickContext {
  const LayerStack* stack = nullptr;
  db::CellDef* cell = nullptr;
  bool magic_compat = true;
};

/// The target of `id`, or nullopt for the background or a stale id.
[[nodiscard]] std::optional<PickTarget> resolve(const MeshModel& mesh, PickId id);

/// The target of `id` with its tile type and source tile (layout-layer
/// mode); a tile no longer under the face leaves `material` null.
[[nodiscard]] std::optional<PickTarget> resolve(const MeshModel& mesh, const PickContext& context, PickId id);

/// What the 2D view shows after the 3D -> 2D link: the selection root (the
/// rendered cell) and the area to scroll into view, in its coordinates.
struct SelectionLink {
  db::CellDef* root = nullptr;
  db::Rect area;
  bool use = false;  // a subcell use was selected (else paint)
};

/// Selects `target` in the 2D view (3D -> 2D link). A target whose source
/// tile was not found, or whose cell is no longer loaded, changes nothing.
[[nodiscard]] diag::Status to_selection(editor::EditorState& editor, const PickTarget& target);

/// to_selection, returning what the 2D view scrolls to; nullopt when
/// nothing was selected.
[[nodiscard]] std::optional<SelectionLink> link_to_2d(editor::EditorState& editor, const PickTarget& target);

/// The instances overlapping the 2D selection's paint (2D -> 3D link).
/// Without the stack a layer cannot be mapped to its type, so an instance
/// over selected paint of any type is highlighted; front ends pass the
/// stack (overload below).
[[nodiscard]] HighlightSet from_selection(const MeshModel& mesh, const select::Selection& selection);

/// The instances whose layer's tile type is selected under them.
[[nodiscard]] HighlightSet from_selection(const MeshModel& mesh, const LayerStack& stack,
                                          const select::Selection& selection);

/// from_selection for the model of `context`: empty when the selection's
/// root is not the rendered cell (a selection in another window or an
/// edited subcell holds paint in that cell's coordinates); with no stack in
/// `context` every selected type matches, as the plain overload.
[[nodiscard]] HighlightSet from_selection(const MeshModel& mesh, const PickContext& context,
                                          const select::Selection& selection);

/// Which connectivity search net highlighting runs.
enum class NetScope : std::uint8_t {
  flat,          // connect::search_flat in the rendered cell (a seed in a subcell finds nothing)
  hierarchical,  // connect::search_hierarchical over the rendered cell and every subcell
};

struct NetOptions {
  NetScope scope = NetScope::hierarchical;
  bool magic_compat = true;
  /// The stack the model was built from; nullptr: the editor's layout-layer stack.
  const LayerStack* stack = nullptr;
};

/// The instances of the net of `seed`, through `connect` (hierarchical
/// search, magic-compat ON, the editor's layout-layer stack).
[[nodiscard]] HighlightSet net_highlight(const MeshModel& mesh, const editor::EditorState& editor,
                                         const PickTarget& seed);

/// The same with the search, the compat setting and the stack chosen.
[[nodiscard]] HighlightSet net_highlight(const MeshModel& mesh, const editor::EditorState& editor,
                                         const PickTarget& seed, const NetOptions& options);

/// "Isolate selected net": every instance of `mesh` not in `keep`, to hide
/// or dim.
[[nodiscard]] HighlightSet isolate_others(const MeshModel& mesh, const HighlightSet& keep);

}  // namespace hefesto::view3d
