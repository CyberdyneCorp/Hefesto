// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// view3d::Session: the M10 session state of one open project,
// editor::EditorState::view3d (openspec/changes/m10-3d-view/design.md D4):
// the at-most-one 3D window (view-3d "Only one 3D window allowed.") and its
// WindowState. Each member names the workstream that fills it in.
//
// Per window (Magic's W3DclientRec plus Hefesto's state): the rendered
// cell, the mode (per window, Q-MISC-76 fix), the f applied on entering
// mask-layer mode (Q-MISC-78 compat), level, ViewState (D7), the cut box,
// the two visibility sets (one per mode, Q-MISC-71 fix), the current mesh
// handle, and the toolkit path the window was opened with.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); WIND3D fills in
// src/core/src/view3d/client/ (design D3, section 8).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/mode.hpp"

namespace hefesto::view3d {

/// The toolkit path of `specialopen wind3d` (view-3d "Opening the 3D
/// window") and of `render3d` (design D9).
inline constexpr const char* kDefaultToolkitPath = ".magic3d";
inline constexpr const char* kRender3dToolkitPath = ".render.magic";

/// The two visibility sets (Q-MISC-71): `see` and the screen's show/hide
/// edit the current mode's set; `defaults` resets both; a change of the
/// current output style resets the mask set. Hidden sets, so the default
/// (everything visible, as Magic's DBAllTypeBits) is the empty set.
struct VisibilitySets {
  tech::TypeMask layout_hidden;    // tile types hidden in layout-layer mode
  std::vector<bool> mask_hidden;   // per mask layer of the current output style; empty = none hidden

  friend bool operator==(const VisibilitySets&, const VisibilitySets&) = default;
};

/// One open 3D window.
struct WindowState {
  // WIND3D: the rendered cell and the window's identity.
  std::string cell;
  std::string toolkit_path = kDefaultToolkitPath;
  // WIND3D: the mode (per window, Q-MISC-76) and the f recorded on entering
  // mask-layer mode (Q-MISC-78; 1 in layout-layer mode).
  Mode mode = Mode::layout_layer;
  float applied_factor = 1.0F;
  // WIND3D: `level` (0..3 after clamping; 1 on open).
  int level = 1;
  // CAMERA: Magic's view values.
  ViewState view;
  // WIND3D: the cut box (`cutbox`), nullopt = no clipping.
  std::optional<db::Rect> cut_box;
  // WIND3D / INSPECT: the per-mode visibility sets.
  VisibilitySets visibility;
  // MESH: the current mesh model (nullptr before the first build).
  MeshHandle mesh;

  /// The f that `cif` back to layout-layer mode divides by (Q-MISC-78
  /// branch point): `current_factor` (the current output style's f) with
  /// magic-compat ON, `applied_factor` with it OFF.
  [[nodiscard]] float exit_mask_factor(bool magic_compat, float current_factor) const;
};

struct Session {
  // WIND3D: the open 3D window, if any (at most one).
  std::optional<WindowState> window;

  [[nodiscard]] bool has_window() const noexcept { return window.has_value(); }
};

}  // namespace hefesto::view3d
