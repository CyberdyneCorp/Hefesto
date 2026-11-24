// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of view3d/pick: an instance's geometry in root
// coordinates (pick.cpp, highlight.cpp). Owner: INSPECT.

#pragma once

#include "hefesto/db/geometry.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/pick/pick_id.hpp"

namespace hefesto::view3d::detail {

/// The layer of the instance `ref` names (which must exist).
[[nodiscard]] LayerId layer_of(const Chunk& chunk, const PickRef& ref) noexcept;

/// The instance's face in root coordinates: the rectangle, the bounding box
/// of a triangle, or the (zero-width) bounding box of a side segment.
[[nodiscard]] db::Rect face_rect(const Chunk& chunk, const PickRef& ref) noexcept;

/// An area of positive extent inside the material the instance shows: the
/// rectangle; a triangle's bounding box; for an axis-aligned side, the
/// strip one unit wide on its inner side (opposite to its outward normal);
/// for a diagonal side, its bounding box.
[[nodiscard]] db::Rect probe_rect(const Chunk& chunk, const PickRef& ref) noexcept;

}  // namespace hefesto::view3d::detail
