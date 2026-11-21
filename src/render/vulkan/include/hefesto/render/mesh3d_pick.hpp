// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pick ids Mesh3dRenderer writes to the R32_UINT id image
// (hefesto-3d-visualization "Interactive 3D inspection" (picking);
// openspec/changes/m10-3d-view/design.md D10, tasks.md 5.4).
//
// An id is 1 + the instance's ordinal in the model: chunks in model order,
// and in each chunk its rects, then its sides, then its triangles, each in
// vector order. 0 (view3d::kNoPick) is the background. Ids are valid for
// the model that was uploaded when the frame was drawn; decode them against
// that model. A rectangle instance draws both its top and its bottom face
// under one id.
//
// The renderer bakes each instance's chunk-local index (rects first, then
// sides, then triangles) into its instance data and adds the chunk's base
// (1 + the instances of the chunks before it) per draw, so an unchanged
// chunk keeps its GPU buffer when an earlier chunk changes size.
//
// Pure functions, no Vulkan. Owner: RENDER3D.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/pick/pick.hpp"

namespace hefesto::render {

enum class PickKind : std::uint8_t { rect, side, triangle };

/// What a pick id refers to in its model.
struct PickRef {
  std::size_t chunk = 0;  // index into MeshModel::chunks
  PickKind kind = PickKind::rect;
  std::size_t index = 0;  // index into the chunk's rects, sides or triangles

  friend bool operator==(const PickRef&, const PickRef&) = default;
};

/// The number of instances (rects, sides and triangles) of a chunk.
[[nodiscard]] std::uint64_t pick_instance_count(const view3d::Chunk& chunk) noexcept;

/// The chunk-local index of an instance (rects, then sides, then triangles).
[[nodiscard]] std::uint64_t pick_local_index(const view3d::Chunk& chunk, PickKind kind, std::size_t index) noexcept;

/// The id of `ref` in `mesh`; kNoPick when `ref` names no instance or the
/// model has more instances than ids.
[[nodiscard]] view3d::PickId encode_pick(const view3d::MeshModel& mesh, const PickRef& ref) noexcept;

/// The instance of `id` in `mesh`; nullopt for kNoPick and for ids past the
/// model's last instance.
[[nodiscard]] std::optional<PickRef> decode_pick(const view3d::MeshModel& mesh, view3d::PickId id) noexcept;

}  // namespace hefesto::render
