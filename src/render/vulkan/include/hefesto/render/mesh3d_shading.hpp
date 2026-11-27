// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Colors and lighting of the 3D view (view-3d "Viewing transform and
// shading", "Rendered geometry"; openspec/changes/m10-3d-view/design.md D8).
// The CPU side of what the Mesh3dRenderer shaders compute
// (shaders/mesh3d_common.glsl), so tests and other backends share one
// definition.
//
// - Color: each stack layer's display style color as the color map gives
//   it (display::resolve_style), opaque. Magic's OpenGL driver sends a
//   translucent style (mask < 127) as the "supercolor" 2c - 0.8 blended
//   with alpha mask / 127, so that a blend over the background shows c
//   (grTOGL1.c:116-133); in 3D, with the depth test (Q-MISC-77), the face
//   is drawn in c itself.
// - Only solid and stippled styles are drawn (W3Dmain.c:406, :439);
//   stippled ones solid (design D8).
// - Background: DrawParams::background when set (the desktop tab passes its
//   theme token, Q-MISC-84), else the color of the transparent style
//   (Magic's w3dClear, W3Dmain.c:449-466).
// - Lighting: the board look in both magic-compat modes (Q-MISC-84, V1,
//   approved 2026-11-24): flat per-face directional shading on each face's
//   own outward normal in the displayed scene (the model mirrored in z, z
//   growing with the height, R1/Q-MISC-81). The light keeps its direction
//   relative to the camera's azimuth (view_z), so the two side orientations
//   a view shows always get distinct factors: I = a + (1 - a) * max(n . l,
//   0) / l_z, at most 1, with a = 0.55: a top face is drawn in its style
//   color (I = 1), side faces darker, a bottom face at a. Magic's GL state
//   (normal (0, 0, 1) on every face, positional light at (50, 50, 50),
//   per-vertex) is no longer drawn: Magic's pixels are informative
//   (view-3d "Viewing transform and shading"), and no test or golden face
//   log needs it.
// - Translucent styles are drawn opaque in their plain colour, in both
//   modes (R3, approved 2026-11-22).
//   The lit color is clamp(c * I, 0, 1); I <= 1, so a face is never lighter
//   than its style color.
//
// Owner: RENDER3D.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "hefesto/display/styles.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::render {

/// How a stack layer is painted.
struct LayerPaint {
  std::array<float, 3> rgb{1.0F, 1.0F, 1.0F};  // display values (sRGB-encoded), 0..1
  bool drawable = true;                        // a solid or stippled style

  friend bool operator==(const LayerPaint&, const LayerPaint&) = default;
};

/// The lighting of a frame (DrawParams::lighting). Only the board look
/// remains (Q-MISC-84); the enum keeps the parameter for later looks.
enum class Lighting : std::uint8_t {
  board,  // flat per-face directional shading, both magic-compat modes
};

/// The board lighting's ambient term: the factor of a face turned away from
/// the light (a bottom face; a side face away from it).
inline constexpr float kBoardAmbient = 0.55F;

/// The net overlay's colour (DrawParams::overlay_color by default): the
/// board's highlight accent #FF8A3D, display values.
inline constexpr std::array<float, 3> kOverlayColor{1.0F, 138.0F / 255.0F, 61.0F / 255.0F};

/// The net overlay's opacity: semi-opaque, blended over what the frame drew.
inline constexpr float kOverlayAlpha = 0.6F;

/// The coplanar tie-break (task 17.5, Q-MISC-77). Two faces at the same
/// height but with different vertices (two layers, two chunks) do not get
/// bit-equal depths: the rasterizer snaps each vertex to its sub-pixel grid,
/// so each face's depth plane is off by up to its depth slope times the
/// snap (measured: up to 1.3e-5 in a tilted mask-unit view, about 200 units
/// in the last place), and LESS_OR_EQUAL alone let the earlier layer show
/// through. Every face of the n-th drawn layer in stack order (n from 1; the
/// substrate slab 0) is therefore drawn with Vulkan's depth bias toward the
/// viewer: n * kCoplanarBiasSlope times the face's depth slope per pixel
/// plus n * kCoplanarBiasConstant units of depth resolution. The bias
/// scales with the slope, as the error does, so a later layer wins at every
/// pixel and orbit angle, in the colour and the pick image alike; faces of
/// different heights keep their physical order unless their separation is
/// below about n * kCoplanarBiasSlope pixels' worth of depth.
inline constexpr float kCoplanarBiasSlope = 1.0F / 16.0F;
inline constexpr float kCoplanarBiasConstant = 2.0F;

/// The depth bias factors (constant, slope) of the n-th drawn layer: both
/// negative (toward the viewer); zero for ordinal 0.
[[nodiscard]] constexpr std::array<float, 2> coplanar_bias(std::size_t ordinal) noexcept {
  const auto n = static_cast<float>(ordinal);
  return {-n * kCoplanarBiasConstant, -n * kCoplanarBiasSlope};
}

/// The paint of every layer of `stack`. Without a style set every layer is
/// drawable and white.
[[nodiscard]] std::vector<LayerPaint> layer_paints(const view3d::LayerStack& stack, const display::StyleSet* styles);

/// The transparent style's color (display values), Magic's background;
/// black without a style set. DrawParams::background overrides it.
[[nodiscard]] std::array<float, 3> background_color(const display::StyleSet* styles);

/// The board light's direction (unit, toward the light) in the displayed
/// scene for a camera at azimuth `view_z_deg` (ViewState::view_z): from
/// above, over the viewer's left shoulder. In the isometric preset it is
/// about (-0.37, -0.18, 0.91), so the -x side faces (screen left) get about
/// 0.73 and the -y side faces (screen right) 0.64.
[[nodiscard]] std::array<float, 3> board_light(float view_z_deg);

/// The intensity of a face with outward normal `normal` in the displayed
/// scene (unit length, or zero) under the light `light` (board_light):
/// kBoardAmbient + (1 - kBoardAmbient) * max(n . l, 0) / l_z, at most 1. A
/// face whose normal is +z gets exactly 1. Flat: one value per face.
[[nodiscard]] float light_intensity(Lighting lighting, const std::array<float, 3>& light,
                                    const std::array<float, 3>& normal);

/// clamp(c * intensity, 0, 1) per channel (display values).
[[nodiscard]] std::array<float, 3> lit_color(const std::array<float, 3>& rgb, float intensity);

/// The sRGB transfer function and its inverse (the color image's sRGB view
/// encodes what the shader writes).
[[nodiscard]] float srgb_to_linear(float c) noexcept;
[[nodiscard]] float linear_to_srgb(float c) noexcept;

}  // namespace hefesto::render
