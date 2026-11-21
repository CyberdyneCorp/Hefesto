// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ViewState and the camera (view-3d "Default view", "Viewing transform and
// shading"; hefesto-3d-visualization "Interactive 3D inspection" (camera);
// openspec/changes/m10-3d-view/design.md D7; Q-MISC-74 fix, Q-MISC-78
// compat).
//
// Magic's state is the state: ViewState holds exactly W3DclientRec's view
// values (graphics/wind3d.h) as `float`, so `view`, `scroll` and `zoom`
// print what the user sees. Every Hefesto camera operation (orbit, pan,
// zoom, presets, reset) is an operation on it:
// - default_view: the "Default view" values for the cell's bbox
//   (W3Dmain.c:1330-1378: integer halving by arithmetic shift, scale_z 25,
//   prescale_z 0.0001, angles 0), rescaled by f in mask-layer mode;
// - rescale: Magic's w3dRescale by f (Q-MISC-78: the caller passes the
//   current style's f with magic-compat ON, the applied f with it OFF);
// - matrix: v' = A * S * Rx * Ry * Rz * T * v with A from the drawable's
//   current size (Q-MISC-74), orthographic or perspective;
// - apply: a Gesture (D7), the abstraction the desktop and the iPadOS app
//   (M14) map their input to;
// - preset: the absolute `view` angles of top, isometric, front and side.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); CAMERA fills in
// src/core/src/view3d/camera/ (design D3, section 4).

#pragma once

#include <array>
#include <cstdint>

#include "hefesto/db/geometry.hpp"
#include "hefesto/view3d/mode.hpp"

namespace hefesto::view3d {

/// W3DclientRec's view values, in Magic's `float`.
struct ViewState {
  float view_x = 0.0F;  // angle around x (degrees)
  float view_y = 0.0F;  // angle around y
  float view_z = 0.0F;  // angle around z
  float trans_x = 0.0F;
  float trans_y = 0.0F;
  float trans_z = 0.0F;
  float scale_xy = 1.0F;
  float prescale_z = 1.0F;
  float scale_z = 1.0F;

  friend bool operator==(const ViewState&, const ViewState&) = default;
};

/// Absolute `view` angles (degrees).
struct Angles {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;

  friend bool operator==(const Angles&, const Angles&) = default;
};

enum class Projection : std::uint8_t { orthographic, perspective };

/// The projection of a frame. Perspective is a Hefesto view option, not
/// part of ViewState and not a Magic command.
struct ProjectionParams {
  Projection kind = Projection::orthographic;
  float field_of_view_deg = 45.0F;  // perspective only
};

/// A column-major 4x4 matrix.
using Matrix4 = std::array<float, 16>;

enum class Preset : std::uint8_t { top, isometric, front, side };

/// An abstract camera event, in view units (D7).
struct Gesture {
  enum class Kind : std::uint8_t { orbit, pan, zoom, preset, reset };
  Kind kind = Kind::reset;
  float dx = 0.0F;       // orbit: degrees around z; pan: screen units
  float dy = 0.0F;       // orbit: degrees around x; pan: screen units
  float factor = 1.0F;   // zoom: multiplier of scale_xy
  bool zoom_z = false;   // zoom: also multiply scale_z (modifier held)
  Preset preset = Preset::top;
};

/// The "Default view" of a cell with bounding box `bbox` in `mode`; `f` is
/// the current output style's scale factor (used in mask-layer mode).
[[nodiscard]] ViewState default_view(const db::Rect& bbox, Mode mode, float f);

/// Magic's w3dRescale: scales the view by `f` in Magic's `float`
/// arithmetic. Entering mask-layer mode passes f; `cif` back to
/// layout-layer mode passes 1/f, as Magic (W3Dmain.c:890).
void rescale(ViewState& view, float f);

/// The viewing matrix for a drawable of `width` x `height` pixels.
[[nodiscard]] Matrix4 matrix(const ViewState& view, int width, int height, const ProjectionParams& projection);

/// Applies `gesture` to `view`, changing exactly the values the matching
/// `view`/`scroll`/`zoom` command would.
void apply(ViewState& view, const Gesture& gesture);

/// The absolute angles of `preset`: top (0, 0, 0), isometric (-54.7356, 0,
/// 45), front (-90, 0, 0), side (-90, 0, -90).
[[nodiscard]] Angles preset(Preset which);

}  // namespace hefesto::view3d
