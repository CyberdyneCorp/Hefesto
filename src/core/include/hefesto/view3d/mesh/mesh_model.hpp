// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MeshModel (view-3d "Rendered geometry"; hefesto-3d-visualization "Mesh
// generation from layout"; openspec/changes/m10-3d-view/design.md D4, D6):
// the immutable mesh of one build, shared as std::shared_ptr<const
// MeshModel> by the renderer, picking, cross-sections and export.
//
// Geometry is instanced, never expanded into triangles: a top/bottom face
// is a RectInstance, a side face a SideInstance, split tiles go to a small
// per-chunk triangle buffer. Coordinates are float32 relative to the
// chunk's double origin (R5: sub-unit precision on a 2^31-unit chip); z is
// not stored: the shader applies z_top = -h * s_z, z_bot = z_top - t * s_z
// from the layer's LayerStack entry, so `zoom` in z never rebuilds. A
// split tile is the exception: Magic draws both its triangles with the h
// and t of the source tile's left type (ExtGetZAxis), whatever layer
// draws them, so its triangles carry their own h and t (still unscaled
// by s_z), and so do its side faces, flagged own_height, when that left
// type is not the layer's own type.
// The h and t of every other instance are those of its layer in this
// mesh, MeshModel::layer_heights: the stack's draw_height, except a
// contact spanned by the J1 option "Span connected layers".
// With magic-compat OFF a split tile is drawn with its own type's h and t
// (J3, 2026-11-21), so only magic-compat ON produces own_height faces and
// triangles whose h and t differ from their layer's.
// Instances are emitted in a fixed order (layer order, then Morton chunk
// order, then the plane's tile order), so two builds are byte-equal.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); MESH fills in
// src/core/src/view3d/mesh/ (design D3, section 6).

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/view3d/mode.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

/// A top and bottom face: an axis-aligned rectangle, chunk-relative.
struct RectInstance {
  float x0 = 0.0F;
  float y0 = 0.0F;
  float x1 = 0.0F;
  float y1 = 0.0F;
  LayerId layer = 0;

  friend bool operator==(const RectInstance&, const RectInstance&) = default;
};

/// Flags of a side face.
namespace side_flag {
inline constexpr std::uint8_t outward_left = 0x01;  // the outward normal is to the left of (x0,y0)->(x1,y1)
inline constexpr std::uint8_t clip_edge = 0x02;     // a closed face on the clip (cut box) boundary
inline constexpr std::uint8_t diagonal = 0x04;      // the diagonal face of a split tile
inline constexpr std::uint8_t own_height = 0x08;    // a split tile's face drawn with its left type's h and t (height/thickness)
}  // namespace side_flag

/// A side face: one boundary segment, chunk-relative.
struct SideInstance {
  float x0 = 0.0F;
  float y0 = 0.0F;
  float x1 = 0.0F;
  float y1 = 0.0F;
  LayerId layer = 0;
  std::uint8_t flags = 0;  // side_flag bits
  float height = 0.0F;     // with side_flag::own_height: the h and t it is drawn with
  float thickness = 0.0F;

  friend bool operator==(const SideInstance&, const SideInstance&) = default;
};

/// A split-tile triangle (top and bottom face), chunk-relative, drawn in
/// its layer's style with its own h and t (the source left type's).
struct Triangle {
  float x[3] = {0.0F, 0.0F, 0.0F};
  float y[3] = {0.0F, 0.0F, 0.0F};
  LayerId layer = 0;
  float height = 0.0F;
  float thickness = 0.0F;

  friend bool operator==(const Triangle&, const Triangle&) = default;
};

/// The unit of rebuild, culling and progressive display.
struct Chunk {
  std::uint32_t id = 0;       // Morton index of the chunk
  std::uint64_t version = 0;  // bumped when the chunk is rebuilt; the renderer uploads changed chunks only
  double origin_x = 0.0;      // root coordinates of the chunk's origin
  double origin_y = 0.0;
  db::Rect area;              // the chunk's area in root coordinates
  std::vector<RectInstance> rects;
  std::vector<SideInstance> sides;
  std::vector<Triangle> triangles;
};

struct MeshModel {
  Mode mode = Mode::layout_layer;
  db::Rect region;            // the clipped region the mesh covers (root coordinates)
  std::uint64_t version = 0;  // of the whole model
  std::vector<Chunk> chunks;  // in Morton order
  /// Per LayerId: the h and t the layer's rectangles and plain side faces
  /// are drawn with (the stack's draw_height, or the spanned values of a
  /// contact under the J1 option). The renderer, picking and export read
  /// these, not the stack.
  std::vector<DrawHeight> layer_heights;

  [[nodiscard]] bool empty() const noexcept { return chunks.empty(); }
};

/// The shared, immutable handle the window, renderer and export hold.
using MeshHandle = std::shared_ptr<const MeshModel>;

}  // namespace hefesto::view3d
