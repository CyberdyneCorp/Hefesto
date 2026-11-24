// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Vulkan Mesh3dRenderer (view-3d "Viewing transform and shading",
// "Rendered geometry"; openspec/changes/m10-3d-view/design.md D6, D8;
// tasks.md 5.3, 5.4).
//
// - upload: one host-visible buffer per chunk holding its rects, sides and
//   triangles, each grouped by layer, with the chunk-local pick index baked
//   in (mesh3d_pick.hpp). A chunk is uploaded again only when its (id,
//   version) changed: MESH takes versions from one process-wide counter, so
//   the pair names the chunk's content across builds and rebuilds. A chunk
//   of version 0 (a model made by hand) is always uploaded; chunks no
//   longer in the model are dropped.
// - draw: instanced rectangle (top and bottom faces), side-segment and
//   triangle pipelines; z from h, t and s_z in the vertex shader (a
//   triangle and an own_height side face use their own h and t, everything
//   else its layer's MeshModel::layer_heights entry); back faces culled;
//   depth test LESS_OR_EQUAL with writes, layers in stack (style-table)
//   order, so the last style wins on coplanar faces (Q-MISC-77). CAMERA's
//   matrix is applied to the model mirrored in z, so the viewer sits on
//   the side of the higher layers in projection and depth alike, in both
//   magic-compat modes (R1, Q-MISC-81; Q-MISC-84): the top view shows
//   higher layers in front, tilted views look from above. The board's flat
//   per-face shading in both modes (Q-MISC-84, mesh3d_shading.hpp); the
//   clear color is DrawParams::background or the transparent style's.
//   DrawParams::xy_scale multiplies x and y of every instance. Chunk positions go to the GPU relative to a
//   per-chunk matrix computed in double, so far-out chips keep sub-unit
//   precision.
// - level >= 1: the color is drawn into 4x multisampled color and depth
//   images and resolved into the target's color image; the single-sample
//   pass of the same frame still writes the pick ids and the depth image.
//   A device without 4x support draws every level single-sampled.
// - Transparency: layers with alpha < 1 (DrawParams) are drawn after the
//   opaque ones, chunks sorted back to front, depth-tested without depth
//   writes, blended, and write no pick id.
// - read_pick: one pixel of the pick image of the last frame.
//
// The public interface names no Vulkan type. Owner: RENDER3D.

#pragma once

#include <memory>

#include "hefesto/render/mesh3d_renderer.hpp"
#include "hefesto/render/mesh3d_target.hpp"

namespace hefesto::render {

class VulkanMesh3dRenderer final : public Mesh3dRenderer {
 public:
  /// Builds the pipelines for `target` (its device and render pass). Draws
  /// must pass this target. Throws VulkanError when a Vulkan call fails.
  explicit VulkanMesh3dRenderer(VulkanOffscreenTarget& target);
  ~VulkanMesh3dRenderer() override;

  void upload(const view3d::MeshModel& mesh) override;
  /// Draws into the target's back buffer, waits and swaps (target.shown()
  /// is then this frame). Throws std::invalid_argument for another target.
  void draw(const view3d::ViewState& view, const DrawParams& params, OffscreenTarget& target) override;
  [[nodiscard]] view3d::PickId read_pick(int x, int y) override;

  /// Chunk uploads done so far (each one re-created a chunk buffer).
  [[nodiscard]] std::uint64_t chunk_uploads() const noexcept;
  /// Whether level >= 1 draws multisampled on this device.
  [[nodiscard]] bool multisampling() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::render
