// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The corner tests shared by bridge and bridge-lim (capability
// mask-output-operators "bridge", "bridge-lim"; design D6, Q-MASK-3).
// Private to src/core/src/mask/ops/.
//
// Owner: OPS.

#pragma once

#include <optional>

#include "mask/ops/kernel.hpp"

namespace hefesto::mask::detail {

/// The neighbours a corner test reads: TR (top-most right), RBot (right
/// neighbour containing the bottom edge), RT (right-most top), LB
/// (left-most bottom) and BRight (bottom neighbour under the right end).
/// Missing entries are the plane's boundary.
struct TileCorners {
  std::optional<db::TileView> tr, rbot, rt, lb, bright;
};
[[nodiscard]] TileCorners tile_corners(const db::Plane& plane, const db::TileView& t);

/// The NE outer corner of kind `k`: leftType(TR) = k and bottomType(RT) = k.
[[nodiscard]] bool ne_corner(const TileCorners& c, tech::TileType k);
/// The SE outer corner of kind `k`: leftType(RBot) = k and topType of the
/// bottom neighbour tested (BRight when `bottom_right`, else LB) = k.
[[nodiscard]] bool se_corner(const TileCorners& c, tech::TileType k, bool bottom_right);

}  // namespace hefesto::mask::detail
