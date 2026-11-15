// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Unmerged boundary output (gds-write "Unmerged boundary output", "Tile
// decomposition and enumeration order"; see hefesto/gds/write/polygons.hpp).
// Ports Magic 8.3.694 (fc5daa9e) calma/CalmaWrite.c calmaWritePaintFunc,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). The products are 64-bit and range-checked by
// the ElementWriter (Q-MASK-222); Magic multiplies in int.
// Owner: GDSW.

#include <array>

#include "hefesto/gds/write/polygons.hpp"

namespace hefesto::gds {

namespace {

// The XY points of a solid tile (side) scaled by `scale`.
std::array<OutputPoint, 5> tile_points(const db::TileView& tile, std::int64_t scale, std::size_t& count) {
  const std::int64_t x0 = std::int64_t{tile.area.x0} * scale;
  const std::int64_t y0 = std::int64_t{tile.area.y0} * scale;
  const std::int64_t x1 = std::int64_t{tile.area.x1} * scale;
  const std::int64_t y1 = std::int64_t{tile.area.y1} * scale;
  if (!tile.split) {
    count = 5;
    return {OutputPoint{x0, y0}, OutputPoint{x1, y0}, OutputPoint{x1, y1}, OutputPoint{x0, y1}, OutputPoint{x0, y0}};
  }
  count = 4;
  // k = 2 * side + dir; dir = 1 for the `\` diagonal (TT_DIRECTION).
  const int k = (tile.right_side ? 2 : 0) + (tile.diagonal == db::Diagonal::nw_se ? 1 : 0);
  switch (k) {
    case 0:
      return {OutputPoint{x0, y0}, OutputPoint{x0, y1}, OutputPoint{x1, y1}, OutputPoint{x0, y0}, {}};
    case 1:
      return {OutputPoint{x0, y1}, OutputPoint{x0, y0}, OutputPoint{x1, y0}, OutputPoint{x0, y1}, {}};
    case 2:
      return {OutputPoint{x0, y0}, OutputPoint{x1, y0}, OutputPoint{x1, y1}, OutputPoint{x0, y0}, {}};
    default:
      return {OutputPoint{x0, y1}, OutputPoint{x1, y1}, OutputPoint{x1, y0}, OutputPoint{x0, y1}, {}};
  }
}

}  // namespace

diag::Status emit_tile_boundaries(const mask::MaskPlane& plane, const mask::GdsLayer& layer, ElementWriter& out) {
  tech::TypeMask solid;
  solid.set(mask::kMaskSolid);
  const std::int64_t scale = out.scale().paint;
  diag::Status status = diag::Done{};
  (void)plane.plane().enumerate(db::whole_plane(), solid, [&](const db::TileView& tile) {
    std::size_t count = 0;
    const std::array<OutputPoint, 5> points = tile_points(tile, scale, count);
    status = out.boundary(layer, std::span<const OutputPoint>{points.data(), count});
    return status.ok();
  });
  return status;
}

}  // namespace hefesto::gds
