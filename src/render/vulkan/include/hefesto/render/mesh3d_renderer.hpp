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
// faces, Q-MISC-77), the scene drawn mirrored in z so the viewer is on the
// side of the higher layers in projection and depth alike (R1, Q-MISC-81;
// Q-MISC-84); the board's flat per-face shading in both magic-compat modes
// (Q-MISC-84, mesh3d_shading.hpp); MSAA 4x at level >= 1. Redraws on
// demand only. Round 5 (SECTION): an optional vertical clipping plane for
// cross-sections (DrawParams::section_plane) and the board's substrate
// slab (DrawParams::substrate), both off unless the caller sets them.
// Round 6: on coplanar faces the later layer in stack order wins at every
// pixel (a per-layer, slope-scaled depth bias, coplanar_bias, task 17.5), and an
// optional net overlay (DrawParams::net_overlay, task 10.4) is drawn over
// the mask geometry.
//
// Owner: SCAFFOLD (this header, M10 task 2.3, interface only); RENDER3D
// implements it in src/render/vulkan/src/mesh3d_*.cpp (design D3, section 5):
// render::VulkanMesh3dRenderer (mesh3d_vulkan_renderer.hpp), with the pick
// ids of mesh3d_pick.hpp and the colors and lighting of mesh3d_shading.hpp.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "hefesto/display/styles.hpp"
#include "hefesto/render/mesh3d_shading.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"
#include "hefesto/view3d/stack/substrate.hpp"

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

/// A vertical clipping plane x = position or y = position (M10 round 5,
/// cross-sections; design D10 "an optional clipping plane in the 3D
/// view"): fragments on the removed side are discarded in every pass
/// (opaque, transparent, pick and depth, and the substrate slab), so the
/// pick and depth images agree with the picture. The cut faces are not
/// capped: the inside of a prism the plane cuts is open, and what lies
/// behind it shows through. `position` is in root coordinates, as the
/// mesh and view3d::Segment (DrawParams::xy_scale applies to it as to the
/// mesh); points on the plane are kept.
struct SectionPlane {
  enum class Axis : std::uint8_t { x, y };
  Axis axis = Axis::x;
  double position = 0.0;
  /// Keep x (or y) >= position; false: keep x (or y) <= position.
  bool keep_greater = true;

  friend bool operator==(const SectionPlane&, const SectionPlane&) = default;
};

/// One prism of the net overlay (task 10.4, design D10: a layout net drawn
/// over mask-layer geometry; view3d::NetOverlayPrism has the same fields).
/// x and y are in the mesh's units, as the instances of the uploaded
/// MeshModel (DrawParams::xy_scale applies to them as to the mesh);
/// z_bottom and z_top are stack draw heights, as h and h + t of a layer
/// (LayerStack::draw_height): the renderer scales them by s_z and mirrors z
/// as for the layers. Corners in either order; z_top below z_bottom is
/// swapped; a prism with a non-finite field is skipped.
struct OverlayPrism {
  float x0, y0, x1, y1;
  float z_bottom, z_top;

  friend bool operator==(const OverlayPrism&, const OverlayPrism&) = default;
};

/// Everything a frame needs besides the view state and the mesh.
/// Layers past the end of `hidden` are visible, past the end of `alpha`
/// opaque. A layer with alpha < 1 is drawn in the transparency pass (after
/// the opaque layers, depth-tested without depth writes, chunks back to
/// front) and writes no pick id, so a click picks what is behind it; alpha
/// <= 0 hides the layer.
struct DrawParams {
  view3d::ProjectionParams projection;
  int level = 1;                                 // `level`: MSAA 4x at level >= 1
  const view3d::LayerStack* stack = nullptr;     // h, t of layers missing from MeshModel::layer_heights
  const display::StyleSet* styles = nullptr;     // layer colors and the default background (mesh3d_shading.hpp)
  /// The clear color, display (sRGB-encoded) values 0..1; unset: the
  /// transparent style's color, background_color(styles). The desktop tab
  /// passes its theme token (Q-MISC-84).
  std::optional<std::array<float, 3>> background;
  /// The shading (Q-MISC-84: the board look in both magic-compat modes; the
  /// stack's magic_compat no longer chooses it).
  Lighting lighting = Lighting::board;
  std::vector<bool> hidden;                      // per stack layer
  std::vector<float> alpha;                      // per stack layer; empty = opaque (transparency pass)
  const view3d::HighlightSet* highlight = nullptr;  // drawn highlighted, the rest dimmed
  /// Multiplies x and y of every instance (root coordinates) before the
  /// view transform; z is unchanged. Mask-layer mode in P1 draws the layout
  /// mesh with view3d::Scene::xy_factor (task 9.1). A value that is not a
  /// positive finite number draws at 1.
  float xy_scale = 1.0F;
  /// The cross-section's clipping plane; unset: nothing is clipped.
  std::optional<SectionPlane> section_plane;
  /// The board's substrate slab (view3d::substrate_slab; Q-MISC-84/85,
  /// task 16.11), drawn in the opaque pass with depth writes and pick id
  /// view3d::kNoPick (a click on it picks nothing), shaded per face like
  /// the layers, never dimmed by a highlight. Unset (the default, script
  /// and hefesto-cli windows): no slab. Only the desktop tab sets it.
  std::optional<view3d::SubstrateSlab> substrate;
  /// The net overlay (task 10.4): prisms drawn after the opaque and ghost
  /// passes over everything the frame drew (no depth test, no depth
  /// write), in `overlay_color` with board shading per face, blended at
  /// kOverlayAlpha; no pick id (a click picks the mask geometry under it);
  /// clipped by `section_plane`; never dimmed by `highlight`. The caller
  /// keeps the prisms alive for the draw call. Empty (the default): nothing
  /// drawn and no overlay pipeline created. Each prism blends once where it
  /// is seen (back faces culled); prisms that overlap on the screen blend
  /// once each.
  std::span<const OverlayPrism> net_overlay;
  /// The overlay's colour, display (sRGB-encoded) values 0..1.
  std::array<float, 3> overlay_color = kOverlayColor;
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
  /// The pick id at pixel (x, y) of the last frame, x from the left and y
  /// from the top (view3d::kNoPick for the background, outside the frame or
  /// before the first frame).
  [[nodiscard]] virtual view3d::PickId read_pick(int x, int y) = 0;
};

}  // namespace hefesto::render
