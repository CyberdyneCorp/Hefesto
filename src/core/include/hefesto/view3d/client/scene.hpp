// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the 3D window draws (view-3d "Rendered geometry", design D1, D6,
// D9): the layer stack, the mesh and the per-layer visibility of the open
// window, for the front end's WindowDisplay to hand to
// render::Mesh3dRenderer (upload the mesh; draw with the window's view,
// level and hidden flags).
//
// Phase 1 draws the layout-layer source in both modes: in mask-layer mode
// the layout-layer mesh is drawn with xy scaled by the f applied on
// entering the mode (`xy_factor`; the z factors of the f rescale cancel, so
// heights are unchanged) until MASKSRC (P2) replaces it with the mask
// layers, and the mask visibility set applies from then on.
//
// Owner: WIND3D (openspec/changes/m10-3d-view/design.md D3).

#pragma once

#include <vector>

#include "hefesto/view3d/client/session.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::view3d {

struct Scene {
  LayerStack stack;
  MeshHandle mesh;              // nullptr: no rendered cell, or the build was cancelled
  std::vector<bool> hidden;     // per stack layer
  float xy_factor = 1.0F;       // P1 mask-layer placeholder: xy scale of the layout mesh
  db::Rect clip;                // the cut box, or the cell's bounding box grown by 1
  bool clipped = false;         // `clip` is the cut box (faces on it are closed)
};

/// The layout-layer stack of the session: the extract heights of the
/// current extract style rescaled to the internal grid, the display
/// type-style bindings (Q-MISC-77) and the Q-MISC-72 setting.
[[nodiscard]] LayerStack layout_stack(const editor::EditorState& editor, bool magic_compat);

/// The mask-layer stack of the current output style (empty without one),
/// with the values `render name height thick [style]` wrote while that
/// style stayed current (Session::render_overrides). `render name` reads
/// it; MASKSRC (P2) draws from it.
[[nodiscard]] LayerStack mask_stack(const editor::EditorState& editor, bool magic_compat);

/// The scene of `window`: the stack, the mesh of the rendered cell clipped
/// to the cut box or the bounding box grown by 1 (Q-MISC-70), the hidden
/// flags of the window's layout visibility set and the P1 xy factor.
[[nodiscard]] Scene build_scene(const editor::EditorState& editor, const WindowState& window, bool magic_compat,
                                BuildContext& ctx);

}  // namespace hefesto::view3d
