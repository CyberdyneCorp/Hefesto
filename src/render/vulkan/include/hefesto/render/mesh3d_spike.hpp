// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M10 renderer spike (openspec/changes/m10-3d-view tasks.md 5.1, design
// D8, risk R1): a depth-tested test mesh drawn into a VulkanOffscreenTarget,
// to show that the 3D view can render offscreen on Raven's device and be shown
// through the viewport's texture mode. It checks, with one draw:
//   - the depth test (a nearer quad drawn first stays in front),
//   - LESS_OR_EQUAL (a coplanar quad drawn later wins, Q-MISC-77),
//   - back-face culling (a back-facing quad over everything is not drawn),
//   - the pick-id image and the sRGB encoding of the color image.
// The real pipelines replace it in task 5.3 (Mesh3dRenderer).
//
// Owner: RENDER3D (design D3).

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "hefesto/render/mesh3d_target.hpp"

namespace hefesto::render {

/// One vertex of the test mesh: clip-space position (Vulkan: y down, z in
/// [0, 1]) before the transform, linear color, pick id.
struct SpikeVertex {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 1.0F;
  std::uint32_t pick = 0;
};

/// Column-major 4x4 matrix applied to every vertex.
using SpikeTransform = std::array<float, 16>;
inline constexpr SpikeTransform kSpikeIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

/// The background, linear: encodes to 128 in each sRGB channel.
inline constexpr std::array<float, 4> kSpikeBackground{0.2158605F, 0.2158605F, 0.2158605F, 1.0F};

/// The pick ids of the test mesh's quads.
enum class SpikeQuad : std::uint32_t { near_red = 1, far_green = 2, coplanar_blue = 3, back_facing = 4 };

/// The test mesh, in draw order (counter-clockwise front faces):
///   near_red      z 0.25, x [-1, 0.5], y [-1, 0.5]   (drawn first)
///   far_green     z 0.75, x [-0.5, 1], y [-1, 0.5]   (behind red where they overlap)
///   coplanar_blue z 0.25, x [-1, 0],   y [-1, -0.5]  (coplanar with red, drawn later: wins)
///   back_facing   z 0.1,  the whole view, wound clockwise (culled)
/// y in (0.5, 1] shows the background.
[[nodiscard]] std::vector<SpikeVertex> spike_test_mesh();

class Mesh3dSpike {
 public:
  /// Builds the pipeline for `target`'s render pass and uploads the test mesh.
  explicit Mesh3dSpike(VulkanOffscreenTarget& target);
  ~Mesh3dSpike();
  Mesh3dSpike(const Mesh3dSpike&) = delete;
  Mesh3dSpike& operator=(const Mesh3dSpike&) = delete;

  /// Draws the test mesh with `transform` into `target`'s back buffer, waits
  /// and swaps (target.shown() is then this frame). `target` must be the one
  /// the spike was built for.
  void draw(VulkanOffscreenTarget& target, const SpikeTransform& transform = kSpikeIdentity);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::render
