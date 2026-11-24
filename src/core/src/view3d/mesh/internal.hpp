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
//   heights.cpp  the h and t each layer is drawn with (J1 "Span connected
//                layers", task 6.5)
//   mesh.cpp     full builds, incremental rebuilds and progressive order
//   worker.cpp   the background worker (MeshWorker)
//
// Owner: MESH (design D3).

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
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

/// One split tile as painted into a flat plane (root coordinates), with the
/// left type of the source tile: Magic draws both triangles of a split
/// tile with that type's h and t (w3dRenderVolume, ExtGetZAxis), which the
/// flat plane, holding one type, no longer knows.
struct SplitSource {
  db::Rect area;
  db::Diagonal diagonal = db::Diagonal::nw_se;
  bool right_side = false;
  tech::TileType z_type = tech::kSpace;

  friend bool operator==(const SplitSource&, const SplitSource&) = default;
};

/// The flattened geometry of one tile type: only `type` and space, in root
/// coordinates. Manhattan tiles are clipped to the clip area; split tiles
/// are kept whole and clipped when their faces are emitted.
struct FlatType {
  tech::TileType type = tech::kSpace;
  db::Plane plane;
  std::vector<SplitSource> splits;  // sorted by (area, diagonal, side) after the flatten

  /// The type whose h and t the split tile `t` of `plane` is drawn with:
  /// the source left type of the painted split tile that holds it (equal,
  /// or containing it when later paint cut it), else `type`.
  [[nodiscard]] tech::TileType z_type_of(const db::TileView& t) const;
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

/// The same over several areas (an incremental rebuild), each searched
/// and clipped on its own into the same planes. False as soon as `stop`
/// answers true during a search.
[[nodiscard]] bool flatten_layout(const LayerStack& stack, const db::CellDef& cell, std::span<const db::Rect> areas,
                                  const std::function<bool()>& stop, FlatLayout& out);

/// flatten_layout under `ctx.lock` held shared when it is set: waits while
/// a writer waits and restarts the flatten when one arrives during it.
/// False only when the build was cancelled.
[[nodiscard]] bool flatten_snapshot(const LayerStack& stack, const db::CellDef& cell, std::span<const db::Rect> areas,
                                    const BuildContext& ctx, FlatLayout& out);

/// The h and t each layer and tile type is drawn with in one build: the
/// stack's, except the contact types spanned by the J1 option (task 6.5).
class HeightTable {
 public:
  HeightTable(const LayerStack& stack, const BuildContext& ctx);

  [[nodiscard]] const LayerStack& stack() const noexcept { return *stack_; }
  /// The h and t of layer `layer` (h = t = 0 out of range).
  [[nodiscard]] DrawHeight layer(LayerId layer) const;
  /// The h and t of a tile of `type` (the left type of a split tile).
  [[nodiscard]] DrawHeight type(tech::TileType type) const;
  /// Per LayerId, for MeshModel::layer_heights.
  [[nodiscard]] const std::vector<DrawHeight>& layers() const noexcept { return layers_; }

 private:
  const LayerStack* stack_;
  std::vector<DrawHeight> layers_;
  std::vector<std::optional<DrawHeight>> spanned_;  // per tile type; empty without the option
};

/// J1 "Span connected layers": the h and t that draw `contact` from the
/// top of its lowest residue to the bottom of its highest, residues from
/// `tech` (the full residue set of a stacked type) and their h and t from
/// `stack`. Nullopt when `contact` is not a contact, or its residues give
/// no gap (fewer than two, or the upper one starts at or below the lower
/// one's top): it keeps its technology h and t.
[[nodiscard]] std::optional<DrawHeight> spanned_height(const LayerStack& stack, const tech::Technology& tech,
                                                       tech::TileType contact);

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

/// The leaves to add to an existing plan over `clip` (an incremental
/// rebuild): the same quadtree, visiting only nodes that meet a `changed`
/// rectangle and hold no `existing` chunk area; a node inside an existing
/// chunk is skipped (that chunk is rebuilt), a node holding some is split.
/// In Morton order.
[[nodiscard]] std::vector<ChunkCell> plan_new_chunks(const FlatLayout& flat, const db::Rect& clip,
                                                     std::span<const db::Rect> existing,
                                                     std::span<const db::Rect> changed, std::size_t max_tiles);

/// The Morton code of (x, y), 16 bits each (x in the even bits).
[[nodiscard]] std::uint32_t morton(std::uint32_t x, std::uint32_t y) noexcept;

/// Appends the faces of one layer inside `chunk.area` to `chunk`: a
/// rectangle per tile piece, triangles for split tiles and, unless the
/// piece has zero thickness, side faces on every boundary segment against
/// another type and closed faces on the clip edge. A Manhattan piece takes
/// the layer's height (HeightTable::layer). A split tile, with
/// magic-compat ON, the h and t of its source left type
/// (FlatType::z_type_of), stored on its triangles and side faces; with it
/// OFF its own layer's (Q-MISC-79).
void emit_layer(const FlatType& flat, LayerId layer, const HeightTable& heights, const db::Rect& clip, Chunk& chunk);

/// The next model and chunk version: one process-wide counter, increasing
/// from 1 and never repeating, so a chunk's (id, version) names its content
/// across builds and rebuilds (the renderer's upload cache keys on it).
/// Thread-safe (MeshWorker threads build concurrently).
[[nodiscard]] std::uint64_t next_version() noexcept;

/// build_from_layout with a chosen chunk size (the tests use small ones).
[[nodiscard]] std::optional<MeshModel> build_layout(const LayerStack& stack, const db::CellDef& cell,
                                                    const ClipArea& clip, BuildContext& ctx, std::size_t max_tiles);

/// rebuild_chunks with a chosen chunk size for the new chunks.
[[nodiscard]] std::optional<MeshModel> rebuild_layout(const MeshModel& previous, const LayerStack& stack,
                                                      const db::CellDef& cell, std::span<const db::Rect> dirty,
                                                      BuildContext& ctx, std::size_t max_tiles);

}  // namespace hefesto::view3d::detail
