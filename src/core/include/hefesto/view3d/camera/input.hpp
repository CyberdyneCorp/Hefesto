// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pointer and touch input mapped to camera Gestures (design D7;
// hefesto-3d-visualization "Interactive 3D inspection" (camera), scenario
// "Touch orbit"). The desktop maps a mouse drag and the wheel, the iPadOS
// app (M14) a one-finger drag and a pinch, to the same Gestures, which
// apply() turns into `view`/`scroll`/`zoom` changes of the ViewState.
//
// Owner: CAMERA (design D3).

#pragma once

#include "hefesto/view3d/camera/view_state.hpp"

namespace hefesto::view3d {

/// Degrees of orbit per pixel of drag.
inline constexpr float kOrbitDegreesPerPixel = 0.5F;

/// A drag of (dx, dy) pixels (x right, y down) that orbits the camera, as
/// if grabbing the layout below its centre: dragging right turns it
/// counter-clockwise around z (seen from above), dragging up tilts its top
/// edge away from the viewer, toward the front view (view_x += dy).
[[nodiscard]] Gesture orbit_drag(float dx_px, float dy_px, float degrees_per_pixel = kOrbitDegreesPerPixel);

/// A drag of (dx, dy) pixels that pans the layout, in the normalized view
/// units of `scroll ... rel` for a drawable of `width` x `height` pixels:
/// in the top-down view the layout follows the pointer exactly.
[[nodiscard]] Gesture pan_drag(float dx_px, float dy_px, int width, int height);

/// A pinch (or wheel step) with scale ratio `scale` (> 1 zooms in);
/// `zoom_z` also scales the height exaggeration.
[[nodiscard]] Gesture zoom_pinch(float scale, bool zoom_z = false);

/// The touch mapping of the iPadOS app (M14): one finger drags orbit.
[[nodiscard]] inline Gesture touch_one_finger_drag(float dx_px, float dy_px) {
  return orbit_drag(dx_px, dy_px);
}

/// The touch mapping of the iPadOS app (M14): a two-finger pinch zooms.
[[nodiscard]] inline Gesture touch_pinch(float scale) {
  return zoom_pinch(scale);
}

}  // namespace hefesto::view3d
