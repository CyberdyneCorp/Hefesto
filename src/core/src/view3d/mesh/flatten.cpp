// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Flatten and clip of the layout-layer source (view-3d "Rendered
// geometry"; hefesto-3d-visualization "Mesh generation from layout",
// "Region model"; openspec/changes/m10-3d-view/design.md D6). The rendered
// cell and every descendant are searched with the hierarchical tile search
// (connect::search_tree_tiles, Magic's DBTreeSrTiles with every use
// expanded, as W3Dredisplay calls it), each tile is transformed to root
// coordinates and only then clipped against the root-coordinate clip area
// (Q-MISC-70, fix in both modes: Magic clips against the area expressed in
// the subcell's coordinates, W3Dmain.c:253-278, :589). The halo of the
// layout-layer source is 0.
//
// Each tile type gets its own flat plane holding only that type, so a type
// in several display styles is flattened once, two types that overlap in
// different cells both stay, and a contact found on each plane of its
// images paints the same area again (no duplicate faces).
//
// An incremental rebuild flattens several areas into the same planes.
// With a database lock the flatten holds it shared and starts again when
// a writer arrives (flatten_snapshot); the faces are emitted afterwards
// from the planes, which the build owns, without the lock.
//
// Owner: MESH (design D3).

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <tuple>

#include "hefesto/connect/tree.hpp"
#include "internal.hpp"

namespace hefesto::view3d::detail {

namespace {

db::PaintRow paint_all(tech::TileType type) {
  db::PaintRow row;
  row.fill(type);
  return row;
}

// Paints one found tile, in root coordinates, into its type's plane.
void paint_tile(FlatType& flat, const connect::TreeTile& found, const db::Rect& clip) {
  db::Plane& plane = flat.plane;
  const db::TileView& tile = found.tile;
  const db::Rect area = db::apply(found.to_root, tile.area);
  const db::PaintRow row = paint_all(tile.type);
  if (!tile.split) {
    const db::Rect piece = intersect(intersect(area, clip), db::whole_plane());
    if (!piece.is_null()) {
      static_cast<void>(plane.paint(piece, row).ok());
    }
    return;
  }
  // Kept whole: clipping a triangle on the tile grid would move its
  // diagonal (DBNMPaintPlane); faces.cpp clips the exact polygon.
  if (!db::in_usable_plane(area)) {
    return;
  }
  const db::TriangleSide side = db::transform_triangle(found.to_root, tile.diagonal, tile.right_side);
  static_cast<void>(plane.paint_triangle(area, side.diagonal, side.right_side, row).ok());
  // The left type in the source cell's orientation, as ExtGetZAxis reads it.
  flat.splits.push_back(SplitSource{area, side.diagonal, side.right_side, tile.left_type});
}

[[nodiscard]] auto split_key(const SplitSource& s) noexcept {
  return std::tuple{s.area.x0, s.area.y0, s.area.x1, s.area.y1, s.diagonal, s.right_side};
}

[[nodiscard]] bool contains(const db::Rect& outer, const db::Rect& inner) noexcept {
  return outer.x0 <= inner.x0 && outer.y0 <= inner.y0 && inner.x1 <= outer.x1 && inner.y1 <= outer.y1;
}

}  // namespace

tech::TileType FlatType::z_type_of(const db::TileView& t) const {
  const SplitSource probe{t.area, t.diagonal, t.right_side, tech::kSpace};
  const auto exact = std::lower_bound(splits.begin(), splits.end(), probe, [](const SplitSource& a, const SplitSource& b) {
    return split_key(a) < split_key(b);
  });
  if (exact != splits.end() && split_key(*exact) == split_key(probe)) {
    return exact->z_type;
  }
  for (const SplitSource& s : splits) {
    if (s.diagonal == t.diagonal && s.right_side == t.right_side && contains(s.area, t.area)) {
      return s.z_type;
    }
  }
  return type;
}

namespace {

// One flat plane per distinct tile type of the stack, in stack order; the
// types to search. `slot` maps a type to its plane.
tech::TypeMask prepare_types(const LayerStack& stack, std::array<std::ptrdiff_t, tech::kMaxTileTypes>& slot,
                             FlatLayout& out) {
  out = FlatLayout{};
  slot.fill(-1);
  tech::TypeMask types;
  for (const StackLayer& layer : stack.layers()) {
    if (!layer.type || *layer.type == tech::kSpace || *layer.type >= tech::kMaxTileTypes) {
      out.of_layer.push_back(-1);
      continue;
    }
    const tech::TileType type = *layer.type;
    if (slot[type] < 0) {
      slot[type] = static_cast<std::ptrdiff_t>(out.types.size());
      out.types.push_back(FlatType{type, db::Plane{}, {}});
      types.set(type);
    }
    out.of_layer.push_back(slot[type]);
  }
  return types;
}

// Sorts each plane's split sources and drops the copies a tile found by
// two overlapping areas left.
void finish_splits(FlatLayout& out) {
  for (FlatType& flat : out.types) {
    std::stable_sort(flat.splits.begin(), flat.splits.end(),
                     [](const SplitSource& a, const SplitSource& b) { return split_key(a) < split_key(b); });
    flat.splits.erase(std::unique(flat.splits.begin(), flat.splits.end()), flat.splits.end());
  }
}

}  // namespace

void wait_for_writers(const db::DatabaseLock& lock, const mask::Progress& progress) {
  while (lock.writer_waiting() && !lock.held_exclusively_by_this_thread() && !progress.cancelled()) {
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }
}

bool flatten_layout(const LayerStack& stack, const db::CellDef& cell, std::span<const db::Rect> areas,
                    const std::function<bool()>& stop, FlatLayout& out) {
  std::array<std::ptrdiff_t, tech::kMaxTileTypes> slot{};
  const tech::TypeMask types = prepare_types(stack, slot, out);
  bool done = !stop();
  for (const db::Rect& clip : areas) {
    if (!done || types.empty() || clip.is_null()) {
      continue;
    }
    done = connect::search_tree_tiles(cell, clip, types, {}, stack.magic_compat(),
                                      [&](const connect::TreeTile& found) {
                                        if (stop()) {
                                          return false;
                                        }
                                        const auto index = static_cast<std::size_t>(slot[found.tile.type]);
                                        paint_tile(out.types[index], found, clip);
                                        return true;
                                      });
  }
  finish_splits(out);
  return done && !stop();
}

bool flatten_layout(const LayerStack& stack, const db::CellDef& cell, const db::Rect& clip,
                    const mask::Progress& progress, FlatLayout& out) {
  const std::array<db::Rect, 1> areas{clip};
  return flatten_layout(stack, cell, areas, [&progress] { return progress.cancelled(); }, out);
}

bool flatten_snapshot(const LayerStack& stack, const db::CellDef& cell, std::span<const db::Rect> areas,
                      const BuildContext& ctx, FlatLayout& out) {
  const mask::Progress& progress = ctx.progress;
  if (ctx.lock == nullptr) {
    return flatten_layout(stack, cell, areas, [&progress] { return progress.cancelled(); }, out);
  }
  db::DatabaseLock& lock = *ctx.lock;
  const bool owner = lock.held_exclusively_by_this_thread();
  // A writer arriving during the flatten makes it start again once the
  // command is done, as the DRC worker drops a partial square.
  const auto stop = [&] { return progress.cancelled() || (!owner && lock.writer_waiting()); };
  while (!progress.cancelled()) {
    wait_for_writers(lock, progress);
    const db::DatabaseLock::Shared guard(lock);
    if (flatten_layout(stack, cell, areas, stop, out)) {
      return true;
    }
  }
  return false;
}

}  // namespace hefesto::view3d::detail
