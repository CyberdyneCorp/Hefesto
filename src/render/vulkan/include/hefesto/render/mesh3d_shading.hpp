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
// - Background: the color of the transparent style (Magic's w3dClear,
//   W3Dmain.c:449-466).
// - Lighting, magic-compat ON (Magic's GL state, W3Dmain.c:471-523): every
//   transform is in the projection matrix and Magic never sets a normal, so
//   each face has the normal (0, 0, 1) and the positional white light at
//   (50, 50, 50) is in model coordinates (root units, z = -h * s_z ...):
//   I = 0.2 (GL's global ambient) + 0.4 (light 0) + max(0, n . l), per
//   vertex. OFF: the light Magic's comment calls directional, from
//   (1, 1, 1), on each face's own normal, with the spec's ambient 0.4.
//   The lit color is clamp(c * I, 0, 1) (Magic's GL_COLOR_MATERIAL).
//
// Owner: RENDER3D.

#pragma once

#include <array>
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

enum class Lighting : std::uint8_t {
  magic,        // magic-compat ON
  directional,  // magic-compat OFF
};

/// The paint of every layer of `stack`. Without a style set every layer is
/// drawable and white.
[[nodiscard]] std::vector<LayerPaint> layer_paints(const view3d::LayerStack& stack, const display::StyleSet* styles);

/// The background (display values); black without a style set.
[[nodiscard]] std::array<float, 3> background_color(const display::StyleSet* styles);

/// The light intensity at model point (x, y, z) of a face with outward
/// normal (nx, ny, nz) (unit length, or zero).
[[nodiscard]] float light_intensity(Lighting lighting, const std::array<double, 3>& model,
                                    const std::array<float, 3>& normal);

/// clamp(c * intensity, 0, 1) per channel (display values).
[[nodiscard]] std::array<float, 3> lit_color(const std::array<float, 3>& rgb, float intensity);

/// The sRGB transfer function and its inverse (the color image's sRGB view
/// encodes what the shader writes).
[[nodiscard]] float srgb_to_linear(float c) noexcept;
[[nodiscard]] float linear_to_srgb(float c) noexcept;

}  // namespace hefesto::render
