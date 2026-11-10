// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `*watch` tile-structure overlay (capability ui-wizard-and-debug,
// requirement "Tile structure overlay"): draws the corner-stitched tiles of
// one plane of the edit cell (or the window's root when there is no edit
// cell) in the tile-debug style (19), each labeled at its center, on top of
// the layout of the window it was set in. While a window has an overlay any
// change in it redraws the whole window (HighlightLayer::redraw; the M1 task
// 5.6 "debug tile-overlay full redraw rule").
//
// Hefesto's overlay is a developer overlay (the requirement allows other
// visuals): tiles are labeled with the type's short name (`types`) or with
// their lower-left corner instead of a memory address, and corner stitches
// are not drawn (the tile plane does not expose them).
// (Magic ref: commands/CmdWizard.c:1245-1300, dbwind/DBWdisplay.c:156-574)
//
// Owner: VIEW (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <map>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::view {

/// STYLE_DRAWTILE.
inline constexpr std::size_t kStyleDrawTile = 19;

struct TileOverlay {
  tech::PlaneId plane = 0;
  bool demo = false;   // Magic: corner stitches as arrows instead of text
  bool types = false;  // label tiles with the type's short name
  /// The watched cell (the edit cell, else the window's root) and the
  /// transform from it to the window's root.
  const db::CellDef* def = nullptr;
  db::Transform to_root;
};

/// The overlay of each window that has one (`*watch` with no plane removes it).
using TileOverlays = std::map<WindowId, TileOverlay>;

/// The overlay of `window` over `area` (root coordinates): one outline per
/// tile of the watched plane in style 19 and its label. Empty when the
/// window has no overlay.
[[nodiscard]] display::HighlightScene tile_overlay_scene(const editor::EditorState& editor,
                                                         const LayoutWindow& window, const db::Rect& area);

}  // namespace hefesto::view
