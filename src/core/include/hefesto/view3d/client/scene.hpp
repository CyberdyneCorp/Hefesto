// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the 3D window draws (view-3d "Rendered geometry", design D1, D6,
// D9): the layer stack, the mesh and the per-layer visibility of the open
// window, for the front end's WindowDisplay to hand to
// render::Mesh3dRenderer (upload the mesh; draw with the window's view,
// level and hidden flags).
//
// Layout-layer mode draws the layout-layer source (build_from_layout);
// mask-layer mode (P2, MASKSRC) the mask layers of the current output
// style generated from the rendered cell (build_from_mask), in mask units,
// so the mesh needs no xy scale in either mode (`xy_factor` is 1; the P1
// placeholder drew the layout mesh with xy scaled by f). The mask
// visibility set applies to the mask layers (Q-MISC-71).
//
// window_build is the one entry for any front end (the desktop's 3D tab,
// hefesto-cli's face log): the stack, the MeshWorker inputs and the hidden
// flags of the window in its mode.
//
// Owner: WIND3D (openspec/changes/m10-3d-view/design.md D3); the
// mask-layer parts (window_build, mask_generation_area, hidden_layers):
// MASKSRC (round 5).

#pragma once

#include <memory>
#include <vector>

#include "hefesto/view3d/client/session.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/mesh/worker.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::view3d {

struct Scene {
  LayerStack stack;
  MeshHandle mesh;              // nullptr: no rendered cell, or the build was cancelled
  std::vector<bool> hidden;     // per stack layer
  float xy_factor = 1.0F;       // 1: the mesh is drawn as built (the P1 mask-layer placeholder scaled it by f)
  db::Rect clip;                // the mesh's clip area: the cut box, or the cell's bounding box grown by 1
                                // (layout-layer mode); in mask-layer mode that area in mask units (mask_clip)
  bool clipped = false;         // the clip area is the cut box (faces on it are closed)
};

/// The layout-layer stack of the session: the extract heights of the
/// current extract style rescaled to the internal grid, the display
/// type-style bindings (Q-MISC-77) and the Q-MISC-72 setting.
[[nodiscard]] LayerStack layout_stack(const editor::EditorState& editor, bool magic_compat);

/// The mask-layer stack of the current output style (empty without one),
/// with the values `render name height thick [style]` wrote while that
/// style stayed current (Session::render_overrides). `render name` reads
/// it; MASKSRC (P2) draws from it. A `render` statement whose style does
/// not resolve is reported to `sink` on every build (decision J2).
[[nodiscard]] LayerStack mask_stack(const editor::EditorState& editor, bool magic_compat, diag::DiagnosticSink& sink);

/// The area the mask-layer source generates, in internal units: the cut
/// box; else, with magic-compat ON, the rendered cell's bounding box, as
/// W3DCIFredisplay passes it to CIFGen (W3Dmain.c:1743, so a mask layer
/// that grows past the cell's edge is cut there), and with it OFF that box
/// grown by the style's generation radius, so no mask layer is cut at the
/// cell's edge (standing rule; reported to the user in round 5). An empty
/// rectangle when the cell or the output style is missing.
[[nodiscard]] db::Rect mask_generation_area(const editor::EditorState& editor, const WindowState& window,
                                            bool magic_compat);

/// Per stack layer, whether it is never drawn nor exported whatever the
/// visibility: with magic-compat OFF in mask-layer mode, the temporary
/// layers (`templayer`) of the current output style, which are no masks;
/// otherwise none. Kept apart from the user's hidden flags so that
/// `export3d -all` still leaves them out.
[[nodiscard]] std::vector<bool> excluded_layers(const editor::EditorState& editor, const LayerStack& stack,
                                                bool magic_compat);

/// Per stack layer, whether it is hidden: in layout-layer mode by the
/// layout visibility set (tile types); in mask-layer mode by the mask set
/// (mask layer indexes of the current output style, Q-MISC-71) and, with
/// magic-compat OFF, every temporary layer (`templayer`), which Magic
/// draws (it draws every CIF layer, W3Dmain.c:1766) but which is no mask
/// (standing rule; reported in round 5).
[[nodiscard]] std::vector<bool> hidden_layers(const editor::EditorState& editor, const LayerStack& stack,
                                              const WindowState& window, bool magic_compat);

/// What the 3D window's mesh is built from in its mode.
struct WindowBuild {
  std::shared_ptr<const LayerStack> stack;  // layout_stack or mask_stack
  /// The rendered cell (null when it is gone), the clip area and, in
  /// mask-layer mode, the mask source (MeshWorker builds the mask layers),
  /// the technology, the window's J1 option (WindowState::
  /// span_connected_layers) and the clip centre. The host sets `lock`, the
  /// chunk size and, for the mask-layer generation errors, `sink`.
  BuildInputs inputs;
  std::vector<bool> hidden;  // hidden_layers
};

/// The stack, build inputs and hidden flags of `window`: layout-layer mode
/// clips to the cut box or the bounding box grown by 1 (Q-MISC-70);
/// mask-layer mode generates mask_generation_area and clips to it in mask
/// units. A `render` statement whose style does not resolve is reported to
/// `sink` (decision J2). The front end calls it again when the mode, the
/// output style (a full rebuild), the cut box or the cell change.
[[nodiscard]] WindowBuild window_build(const editor::EditorState& editor, const WindowState& window,
                                       bool magic_compat, diag::DiagnosticSink& sink);

/// The scene of `window`: window_build, built now on the calling thread
/// with ctx's progress, cancellation and sink (build_once; with
/// magic-compat ON a mask-layer build reports its generation errors to
/// ctx.sink once).
[[nodiscard]] Scene build_scene(const editor::EditorState& editor, const WindowState& window, bool magic_compat,
                                BuildContext& ctx);

}  // namespace hefesto::view3d
