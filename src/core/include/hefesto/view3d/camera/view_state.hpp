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
//   current size (Q-MISC-74), orthographic or perspective; z' grows toward
//   the viewer in both projections (the renderer maps it to its depth
//   range);
// - apply: a Gesture (D7), the abstraction the desktop and the iPadOS app
//   (M14) map their input to;
// - preset: the absolute `view` angles of top, isometric, front and side;
// - frame_view: the translation and scale_xy that frame a 3D box under the
//   current angles (Q-MISC-84), as `scroll`/`zoom` values;
// - exit_mask_factor / change_output_style: the Q-MISC-78 rule.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); CAMERA fills in
// src/core/src/view3d/camera/ (design D3, section 4) and owns the
// declarations below the contract (apply with a home view, the Q-MISC-78
// helpers).

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
/// layout-layer mode passes inverse_factor(f), as Magic (W3Dmain.c:890).
/// A factor that is not finite and positive leaves the view unchanged
/// (Magic would divide by zero).
void rescale(ViewState& view, float f);

/// Magic's `1.0 / (float)f`: the division in double, rounded to the
/// `float` parameter of w3dRescale.
[[nodiscard]] float inverse_factor(float f);

/// The viewing matrix for a drawable of `width` x `height` pixels, column
/// major. Orthographic: A * S * Rx * Ry * Rz * T, where A keeps internal
/// units 1:1 on the screen and fits the limiting dimension: diag(H/W, 1, 1)
/// when W >= H, as Magic, and diag(1, W/H, 1) when H > W (Q-MISC-74: the
/// current size; a non-positive size is treated as square). Perspective:
/// the same transform seen from z = +d, d = 1 / tan(fov / 2), so the z = 0
/// plane frames as in the orthographic view.
[[nodiscard]] Matrix4 matrix(const ViewState& view, int width, int height, const ProjectionParams& projection);

/// Applies `gesture` to `view`, changing exactly the values the matching
/// `view`/`scroll`/`zoom` command would:
/// - orbit: `view dy 0 dx rel` (view_x += dy, view_z += dx);
/// - pan: `scroll dx dy 0 rel` (trans += d / scale_xy);
/// - zoom: `zoom factor 1 rel`, or `zoom factor factor rel` with zoom_z; a
///   factor that is not finite and positive changes nothing (Magic's
///   "zoom values/factors must be positive and nonzero");
/// - preset: `view ax ay az` with the preset's angles;
/// - reset: `defaults`, which needs the cell: this overload leaves the view
///   unchanged; callers that support reset pass `home`.
void apply(ViewState& view, const Gesture& gesture);

/// As apply(view, gesture), with reset setting `view` to `home` (the
/// caller's default_view(bbox, mode, f)).
void apply(ViewState& view, const Gesture& gesture, const ViewState& home);

/// The absolute angles of `preset`: top (0, 0, 0), isometric (-54.7356, 0,
/// 45), front (-90, 0, 0), side (-90, 0, -90). The renderer draws the model
/// mirrored in z (Q-MISC-84), so every preset looks from above or from the
/// side with the higher layers up: the isometric one from (-1, -1, 1), the
/// front one from -y, the side one from +x.
[[nodiscard]] Angles preset(Preset which);

/// The 3D box a frame should show: x and y in root coordinates as drawn
/// (already multiplied by DrawParams::xy_scale in mask-layer mode), z as
/// heights in the stack's draw units (LayerStack::draw_height: the lowest h
/// and the highest h + t of the layers shown).
struct Extent3 {
  double x0 = 0.0;
  double y0 = 0.0;
  double x1 = 0.0;
  double y1 = 0.0;
  double z0 = 0.0;
  double z1 = 0.0;

  friend bool operator==(const Extent3&, const Extent3&) = default;
};

/// The share of the limiting dimension frame_view fills.
inline constexpr double kFrameFill = 0.9;

/// Frames `box` (Q-MISC-84, view-3d "Default view"): `view` with its angles,
/// scale_z and prescale_z kept, and trans_x, trans_y, trans_z and scale_xy
/// set so that the box, as the renderer draws it (the displayed scene: the
/// model mirrored in z, a height h at z = +h * scale_z, so trans_z shifts
/// along the height), projects centred and fills kFrameFill of the
/// limiting dimension of a `width_px` x `height_px` drawable in the
/// orthographic projection (the perspective one frames its z = 0 plane the
/// same). The result is what `scroll trans_x trans_y trans_z` and
/// `zoom scale_xy scale_z` (absolute) set, so a caller applies it through
/// those commands. A box that projects to a point keeps scale_xy; a
/// non-finite box, or a result with a non-finite translation or scale (a
/// huge scale_z or box overflowing float), returns `view` unchanged.
[[nodiscard]] ViewState frame_view(const ViewState& view, const Extent3& box, int width_px, int height_px);

/// Q-MISC-78 (compat): the f that `cif` back to layout-layer mode divides
/// by: the current output style's f with magic-compat ON (as Magic,
/// W3Dmain.c:890), the f applied on entering mask-layer mode with it OFF.
[[nodiscard]] float exit_mask_factor(bool magic_compat, float applied_factor, float current_factor);

/// Q-MISC-78: a change of the current output style from `f_old` to
/// `f_new` while the window is in mask-layer mode. ON: nothing (Magic
/// keeps the view and later divides by f_new). OFF: rescales the view by
/// f_new / f_old so the picture keeps its size. Answers the applied factor
/// the window records: f_old with ON (the view still carries it), f_new
/// with OFF.
float change_output_style(ViewState& view, bool magic_compat, float f_old, float f_new);

}  // namespace hefesto::view3d
