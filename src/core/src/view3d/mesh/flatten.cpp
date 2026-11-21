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
// Owner: MESH (design D3).

#include <array>

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
void paint_tile(db::Plane& plane, const connect::TreeTile& found, const db::Rect& clip) {
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
}

}  // namespace

bool flatten_layout(const LayerStack& stack, const db::CellDef& cell, const db::Rect& clip,
                    const mask::Progress& progress, FlatLayout& out) {
  out = FlatLayout{};
  std::array<std::ptrdiff_t, tech::kMaxTileTypes> slot{};
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
      out.types.push_back(FlatType{type, db::Plane{}});
      types.set(type);
    }
    out.of_layer.push_back(slot[type]);
  }
  if (types.empty() || clip.is_null()) {
    return !progress.cancelled();
  }
  return connect::search_tree_tiles(cell, clip, types, {}, stack.magic_compat(),
                                    [&](const connect::TreeTile& found) {
                                      if (progress.cancelled()) {
                                        return false;
                                      }
                                      const auto index = static_cast<std::size_t>(slot[found.tile.type]);
                                      paint_tile(out.types[index].plane, found, clip);
                                      return true;
                                    });
}

}  // namespace hefesto::view3d::detail
