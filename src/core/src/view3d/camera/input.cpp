// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pointer and touch input mapped to camera Gestures (design D7).
//
// Owner: CAMERA (design D3).

#include "hefesto/view3d/camera/input.hpp"

#include <algorithm>

namespace hefesto::view3d {

Gesture orbit_drag(float dx_px, float dy_px, float degrees_per_pixel) {
  Gesture g;
  g.kind = Gesture::Kind::orbit;
  g.dx = dx_px * degrees_per_pixel;
  g.dy = dy_px * degrees_per_pixel;
  return g;
}

Gesture pan_drag(float dx_px, float dy_px, int width, int height) {
  // matrix() fits the limiting dimension: one view unit is min(W, H) / 2
  // pixels on both axes. Screen y grows downward, view y upward.
  const auto limiting = static_cast<float>(std::max(1, std::min(width, height)));
  Gesture g;
  g.kind = Gesture::Kind::pan;
  g.dx = 2.0F * dx_px / limiting;
  g.dy = -2.0F * dy_px / limiting;
  return g;
}

Gesture zoom_pinch(float scale, bool zoom_z) {
  Gesture g;
  g.kind = Gesture::Kind::zoom;
  g.factor = scale;
  g.zoom_z = zoom_z;
  return g;
}

}  // namespace hefesto::view3d
