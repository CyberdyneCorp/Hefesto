// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Picking, the 3D <-> 2D link and net highlighting (hefesto-3d-visualization
// "Interactive 3D inspection"; openspec/changes/m10-3d-view/design.md D10).
//
// Each mesh instance carries a 32-bit PickId (chunk id and chunk-local
// index); the renderer writes it to its R32_UINT id image and a click reads
// one pixel. resolve() maps an id back to a PickTarget through the per-chunk
// side table the mesh builder keeps (not uploaded).
// - to_selection (3D -> 2D): selects the target's material as `select area
//   <type>` at its point would, in the edit cell directly, or the subcell
//   use with its path; the 2D view scrolls to it.
// - from_selection (2D -> 3D): the instances overlapping the selection's
//   flattened paint, per layer.
// - net_highlight: the instances of the net through `connect` (flat or
//   hierarchical search), for "Isolate selected net".
//
// Owner: SCAFFOLD (this header, M10 task 2.3); INSPECT fills in
// src/core/src/view3d/pick/ (design D3, tasks 9.3-9.5).

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

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::select {
class Selection;
}

namespace hefesto::view3d {

/// The id an instance writes to the pick image; kNoPick is the background.
using PickId = std::uint32_t;
inline constexpr PickId kNoPick = 0;

/// What a pick id stands for.
struct PickTarget {
  LayerId layer = 0;                     // the stack entry
  std::optional<tech::TileType> type;    // layout-layer mode: the tile type (pair mapped back, Q-MISC-77)
  db::Rect root_rect;                    // the face's rectangle (bbox for a triangle), root coordinates
  bool triangle = false;                 // a split-tile triangle
  std::vector<std::string> use_path;     // layout-layer mode: the instance path from the rendered cell
  db::Rect cell_rect;                    // the tile's rectangle in the cell that holds it

  friend bool operator==(const PickTarget&, const PickTarget&) = default;
};

/// A set of instances to draw highlighted (sorted, unique).
struct HighlightSet {
  std::vector<PickId> ids;

  [[nodiscard]] bool empty() const noexcept { return ids.empty(); }
};

/// The target of `id`, or nullopt for the background or a stale id.
[[nodiscard]] std::optional<PickTarget> resolve(const MeshModel& mesh, PickId id);

/// Selects `target` in the 2D view (3D -> 2D link).
[[nodiscard]] diag::Status to_selection(editor::EditorState& editor, const PickTarget& target);

/// The instances overlapping the 2D selection's paint (2D -> 3D link).
[[nodiscard]] HighlightSet from_selection(const MeshModel& mesh, const select::Selection& selection);

/// The instances of the net of `seed`, through `connect`.
[[nodiscard]] HighlightSet net_highlight(const MeshModel& mesh, const editor::EditorState& editor,
                                         const PickTarget& seed);

}  // namespace hefesto::view3d
