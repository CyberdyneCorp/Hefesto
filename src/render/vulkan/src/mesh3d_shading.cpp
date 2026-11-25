// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The colors and lighting of the 3D view (mesh3d_shading.hpp; design.md D8).
// Pick ids live in the core (view3d/pick/pick_id.cpp). Owner: RENDER3D.

#include <algorithm>
#include <cmath>
#include <numbers>

#include "hefesto/display/scene.hpp"
#include "hefesto/render/mesh3d_shading.hpp"

namespace hefesto::render {

// --- colors and lighting -------------------------------------------------------

std::vector<LayerPaint> layer_paints(const view3d::LayerStack& stack, const display::StyleSet* styles) {
  std::vector<LayerPaint> out(stack.size());
  if (styles == nullptr) {
    return out;
  }
  for (std::size_t i = 0; i < out.size(); ++i) {
    const display::ResolvedStyle s = display::resolve_style(*styles, stack.layers()[i].style);
    out[i].rgb = {s.r, s.g, s.b};
    out[i].drawable = s.fill == display::FillKind::solid || s.fill == display::FillKind::stipple;
  }
  return out;
}

std::array<float, 3> background_color(const display::StyleSet* styles) {
  if (styles == nullptr) {
    return {0.0F, 0.0F, 0.0F};
  }
  const display::ResolvedStyle s = display::resolve_style(*styles, display::kStyleTransparent);
  return {s.r, s.g, s.b};
}

std::array<float, 3> board_light(float view_z_deg) {
  // normalize(-0.4, -0.2, 1) at the isometric azimuth (45 degrees), turned
  // with the camera around z.
  constexpr double kX = -0.4;
  constexpr double kY = -0.2;
  constexpr double kZ = 1.0;
  constexpr double kIsometricAzimuth = 45.0;
  const double len = std::sqrt((kX * kX) + (kY * kY) + (kZ * kZ));
  const double turn = (kIsometricAzimuth - static_cast<double>(std::isfinite(view_z_deg) ? view_z_deg : 45.0F)) *
                      std::numbers::pi / 180.0;
  const double c = std::cos(turn);
  const double s = std::sin(turn);
  return {static_cast<float>(((kX * c) - (kY * s)) / len), static_cast<float>(((kX * s) + (kY * c)) / len),
          static_cast<float>(kZ / len)};
}

float light_intensity(Lighting /*lighting*/, const std::array<float, 3>& light, const std::array<float, 3>& normal) {
  const float diffuse = std::max((normal[0] * light[0]) + (normal[1] * light[1]) + (normal[2] * light[2]), 0.0F);
  return std::min(kBoardAmbient + ((1.0F - kBoardAmbient) * diffuse / light[2]), 1.0F);
}

std::array<float, 3> lit_color(const std::array<float, 3>& rgb, float intensity) {
  return {std::clamp(rgb[0] * intensity, 0.0F, 1.0F), std::clamp(rgb[1] * intensity, 0.0F, 1.0F),
          std::clamp(rgb[2] * intensity, 0.0F, 1.0F)};
}

float srgb_to_linear(float c) noexcept {
  return c <= 0.04045F ? c / 12.92F : std::pow((c + 0.055F) / 1.055F, 2.4F);
}

float linear_to_srgb(float c) noexcept {
  return c <= 0.0031308F ? c * 12.92F : (1.055F * std::pow(c, 1.0F / 2.4F)) - 0.055F;
}

}  // namespace hefesto::render
