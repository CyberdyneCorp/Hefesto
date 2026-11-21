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
// handle, the toolkit path the window was opened with and the output style
// it last saw.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); WIND3D fills in
// src/core/src/view3d/client/ (design D3, section 8).

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/mode.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::mask {
class OutputStyle;
}

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

/// Which output style a window last saw (Q-MISC-71, 78): the style as read
/// (mask::OutputStyleSet::base, replaced only by a tech reload) and its
/// index. Selecting another style or reloading the technology changes it;
/// a grid rescale and reselecting the current style (Magic's CIFLoadStyle
/// returns early) do not.
struct OutputStyleKey {
  std::shared_ptr<const mask::OutputStyle> base;  // nullptr: no current output style
  std::size_t index = 0;

  friend bool operator==(const OutputStyleKey& a, const OutputStyleKey& b) noexcept {
    return a.base == b.base && a.index == b.index;
  }
};

/// What `render name height thick [style]` wrote into one mask layer.
/// Magic writes cl_height, cl_thick and cl_renderStyle of CIFCurStyle in
/// place; selecting another style re-reads the section, so the values last
/// until the current output style changes.
struct RenderOverride {
  float height = 0.0F;
  float thickness = 0.0F;
  std::optional<std::int32_t> style_position;  // cl_renderStyle as given; nullopt: unchanged
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
  // WIND3D: `level` (at least 0 after clamping; 1 on open).
  int level = 1;
  // CAMERA: Magic's view values.
  ViewState view;
  // WIND3D: the cut box (`cutbox`), nullopt = no clipping.
  std::optional<db::Rect> cut_box;
  // WIND3D / INSPECT: the per-mode visibility sets.
  VisibilitySets visibility;
  // MESH: the current mesh model (nullptr before the first build).
  MeshHandle mesh;
  // WIND3D: the output style this window last saw (Q-MISC-71, 78).
  OutputStyleKey style_key;

  /// The f that `cif` back to layout-layer mode divides by (Q-MISC-78
  /// branch point): `current_factor` (the current output style's f) with
  /// magic-compat ON, `applied_factor` with it OFF.
  [[nodiscard]] float exit_mask_factor(bool magic_compat, float current_factor) const;
};

/// Where the 3D window is drawn (design D8, D9). The front end that shows
/// the window (the desktop's 3D view; M14's Metal view) implements it over
/// render::Mesh3dRenderer (view3d::build_scene gives the stack and mesh to
/// upload) and installs it in Session::display. The client calls redraw()
/// when the window opens and after every successful change (view-3d "3D
/// view commands": "every successful change triggers a full redraw"), and
/// closed() when it closes. Not owned by the session.
class WindowDisplay {
 public:
  WindowDisplay() = default;
  WindowDisplay(const WindowDisplay&) = delete;
  WindowDisplay& operator=(const WindowDisplay&) = delete;
  virtual ~WindowDisplay() = default;

  virtual void redraw(const editor::EditorState& editor, const WindowState& window) = 0;
  virtual void closed(const WindowState& window) = 0;
};

struct Session {
  // WIND3D: the open 3D window, if any (at most one).
  std::optional<WindowState> window;
  // WIND3D: the toolkit paths of closed 3D windows. Their window commands
  // stay defined in the interpreter and answer "invalid command name", as
  // Magic's deleted command does (the use-after-free of `.magic3d` after
  // `closewindow` is fixed in both modes); with magic-compat ON a new
  // window on one of them is refused with "Could not create a new Tk
  // window" (Q-MISC-80).
  std::set<std::string> closed_paths;
  // WIND3D: the `render name height thick [style]` values, per mask layer
  // index of the output style `render_key` names.
  OutputStyleKey render_key;
  std::map<std::size_t, RenderOverride> render_overrides;
  // WIND3D / SHELL: the front end's display of the window (nullptr: none).
  WindowDisplay* display = nullptr;

  [[nodiscard]] bool has_window() const noexcept { return window.has_value(); }
};

}  // namespace hefesto::view3d
