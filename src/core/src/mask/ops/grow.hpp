// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-tile grow constructions shared by grow, shrink and bloat-or
// (capability mask-output-operators "grow and shrink (default, square
// metric)", "Euclidean grow and shrink"; design D6). Private to
// src/core/src/mask/ops/.
//
// Owner: OPS.

#pragma once

#include "mask/ops/kernel.hpp"

namespace hefesto::mask::detail {

/// cifGrowFunc: paints (or with `erase` erases) tile `t` (a tile of a mask
/// plane, scale 1) grown by `d` into `into`; sides at the plane's edge stay.
void grow_tile_square(MaskPlane& into, const db::TileView& t, db::Coord d, bool erase, bool magic_compat);

/// cifGrowEuclideanFunc: the Euclidean construction of tile `t` of `from`,
/// its rectangle scaled by `scale` (finite sides only), grown by `d`.
void grow_tile_euclidean(MaskPlane& into, const db::Plane& from, const db::TileView& t, db::Coord d, bool erase,
                         std::int32_t grid_limit, bool magic_compat, std::int32_t scale = 1);

}  // namespace hefesto::mask::detail
