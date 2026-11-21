// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The steps of the layout-layer mesh build (openspec/changes/m10-3d-view/
// design.md D6), shared by build.cpp and the unit tests:
//   flatten.cpp  the rendered cell's subtree, flattened through instance
//                transforms into one flat plane per tile type and clipped
//                in root coordinates (Q-MISC-70, fix in both modes)
//   chunks.cpp   the adaptive quadtree of chunks over the clip area and
//                their Morton ids
//   faces.cpp    top/bottom rectangles, split-tile triangles, side faces
//                on boundary and clip-edge segments, emitted per chunk
//
// Owner: MESH (design D3).

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d::detail {

/// About 65,536 tiles per chunk (design D6).
inline constexpr std::size_t kMaxChunkTiles = 65536;
/// No chunk is wider than 2^22 units, so chunk-relative float32 offsets
/// keep a quarter-unit precision anywhere on a 2^31-unit plane.
inline constexpr std::int64_t kMaxChunkSide = std::int64_t{1} << 22;
/// Morton ids have 16 bits per axis: the quadtree is at most 16 levels deep.
inline constexpr int kMaxChunkDepth = 16;

/// The common part of two rectangles (GeoClip); null when they do not overlap.
[[nodiscard]] constexpr db::Rect intersect(const db::Rect& a, const db::Rect& b) noexcept {
  return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

/// The flattened geometry of one tile type: only `type` and space, in root
/// coordinates. Manhattan tiles are clipped to the clip area; split tiles
/// are kept whole and clipped when their faces are emitted.
struct FlatType {
  tech::TileType type = tech::kSpace;
  db::Plane plane;
};

struct FlatLayout {
  std::vector<FlatType> types;           // one per distinct tile type of the stack, in stack order
  std::vector<std::ptrdiff_t> of_layer;  // per LayerId: index in `types`, or -1 (no tile type)
};

/// Flattens the tiles of the stack's tile types found in `cell` and every
/// descendant over `clip` (root coordinates). False when `progress` was
/// cancelled during the search.
[[nodiscard]] bool flatten_layout(const LayerStack& stack, const db::CellDef& cell, const db::Rect& clip,
                                  const mask::Progress& progress, FlatLayout& out);

/// One chunk of the plan: its area (inside the clip area) and Morton id.
struct ChunkCell {
  db::Rect area;
  std::uint32_t id = 0;

  friend bool operator==(const ChunkCell&, const ChunkCell&) = default;
};

/// The non-empty leaves of the quadtree over `clip`, in Morton order: a
/// square node is split while it holds more than `max_tiles` tiles or is
/// wider than kMaxChunkSide, down to kMaxChunkDepth levels or 1 unit.
[[nodiscard]] std::vector<ChunkCell> plan_chunks(const FlatLayout& flat, const db::Rect& clip,
                                                 std::size_t max_tiles = kMaxChunkTiles);

/// The Morton code of (x, y), 16 bits each (x in the even bits).
[[nodiscard]] std::uint32_t morton(std::uint32_t x, std::uint32_t y) noexcept;

/// Appends the faces of one layer inside `chunk.area` to `chunk`: a
/// rectangle per tile piece, triangles for split tiles and, unless the
/// layer has zero thickness (`sides` false), side faces on every boundary
/// segment against another type and closed faces on the clip edge.
void emit_layer(const FlatType& flat, LayerId layer, bool sides, const db::Rect& clip, Chunk& chunk);

/// build_from_layout with a chosen chunk size (the tests use small ones).
[[nodiscard]] std::optional<MeshModel> build_layout(const LayerStack& stack, const db::CellDef& cell,
                                                    const ClipArea& clip, BuildContext& ctx, std::size_t max_tiles);

}  // namespace hefesto::view3d::detail
