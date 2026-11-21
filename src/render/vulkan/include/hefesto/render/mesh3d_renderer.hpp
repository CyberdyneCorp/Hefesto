// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mesh3dRenderer (view-3d "Viewing transform and shading", "Platform
// mechanism of the 3D view"; hefesto-rendering, 3D use;
// openspec/changes/m10-3d-view/design.md D8, D11; Q-MISC-77 fix).
//
// The only way the desktop (and, in M14, the iPadOS app) reaches the 3D
// renderer; it names no Vulkan type, so a Metal backend implements the same
// interface. The Vulkan implementation renders offscreen on Raven's device
// into an OffscreenTarget (an sRGB color image shown through a UNORM view,
// D32_SFLOAT depth, an R32_UINT pick-id image; double-buffered, resized from
// the viewport's resize callback) and the viewport shows the color image in
// texture mode. Instanced rectangle, side-segment and triangle pipelines;
// z from h, t and s_z in the shader; back-face culling; depth test
// LESS_OR_EQUAL in style-table order (the last style wins on coplanar
// faces, Q-MISC-77); ambient 0.4 and one white diffuse light at (50, 50,
// 50); MSAA 4x at level >= 1. Redraws on demand only.
//
// Owner: SCAFFOLD (this header, M10 task 2.3, interface only); RENDER3D
// implements it in src/render/vulkan/src/mesh3d_*.cpp (design D3, section 5).

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::render {

/// The offscreen images one frame draws into. Created and resized by the
/// backend; the shell passes it back on every draw and shows its color
/// image (backend-specific handle behind the interface).
class OffscreenTarget {
 public:
  OffscreenTarget() = default;
  OffscreenTarget(const OffscreenTarget&) = delete;
  OffscreenTarget& operator=(const OffscreenTarget&) = delete;
  virtual ~OffscreenTarget() = default;

  /// The size in pixels.
  [[nodiscard]] virtual int width() const noexcept = 0;
  [[nodiscard]] virtual int height() const noexcept = 0;
  /// Resizes every image (the viewport's resize callback).
  virtual void resize(int width, int height) = 0;
};

/// Everything a frame needs besides the view state and the mesh.
struct DrawParams {
  view3d::ProjectionParams projection;
  int level = 1;                                 // `level`: MSAA at level >= 1
  const view3d::LayerStack* stack = nullptr;     // h, t per layer (shader z)
  std::vector<bool> hidden;                      // per stack layer
  std::vector<float> alpha;                      // per stack layer; empty = opaque (transparency pass)
  const view3d::HighlightSet* highlight = nullptr;  // drawn highlighted, the rest dimmed
};

class Mesh3dRenderer {
 public:
  Mesh3dRenderer() = default;
  Mesh3dRenderer(const Mesh3dRenderer&) = delete;
  Mesh3dRenderer& operator=(const Mesh3dRenderer&) = delete;
  virtual ~Mesh3dRenderer() = default;

  /// Uploads the chunks of `mesh` whose version changed since the last
  /// upload; drops chunks no longer present.
  virtual void upload(const view3d::MeshModel& mesh) = 0;
  /// Draws the uploaded mesh into `target`.
  virtual void draw(const view3d::ViewState& view, const DrawParams& params, OffscreenTarget& target) = 0;
  /// The pick id at pixel (x, y) of the last frame (view3d::kNoPick for
  /// the background).
  [[nodiscard]] virtual view3d::PickId read_pick(int x, int y) = 0;
};

}  // namespace hefesto::render
