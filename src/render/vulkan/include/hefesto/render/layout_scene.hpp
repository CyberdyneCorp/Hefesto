// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Bridge from the core's toolkit-free display::LayoutScene to the GPU
// batches of the Vulkan renderer (capability hefesto-rendering
// "Style-file-faithful 2D drawing"; display-style-files "Outline and fill
// rendering semantics"; hefesto-design-system "Layer colors come from the
// PDK, not the theme"). Layer order and colors come from the scene, never
// from the UI theme.
//
// What the renderer draws from the scene: solid and stippled fills (OpenGL
// stipple orientation), outlines along material boundaries in the style's
// line pattern, the two diagonals of every rectangle of a `cross` style,
// non-Manhattan (split tile) triangles, label rectangles, port-connection
// lines, unexpanded-cell boxes and the glyphs of font labels (filled with the
// nonzero rule, skipped when the text would be under 5 pixels high). Lines are
// zero-width rectangles, which the vertex shader widens to one pixel.
// Bitmap label text and subcell names are text, drawn by the desktop shell
// in its overlay from LayoutScene::labels / cells (see
// docs/design/m1-ui-conformance.md).
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstddef>
#include <vector>

#include "hefesto/display/scene.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::render {

/// What a batch layer draws (for tests and the shell's legend).
enum class BatchKind : std::uint8_t { fill, outline, label, port_line, cell_box, diagonals, triangles, glyphs };

/// GPU batches in redisplay order plus one LayerStyle per batch layer index.
struct LayoutBatches {
  RectScene rects;
  PolygonScene polygons;  // triangles and glyphs, same layer numbering as `rects`
  std::vector<LayerStyle> styles;
  std::vector<BatchKind> kinds;          // per batch layer index
  std::vector<std::size_t> style_index;  // per batch layer: the display style-table index
  /// More than 256 batch layers were needed; the excess (drawn last) was dropped.
  bool truncated = false;
};

/// Converts a resolved display style to a GPU layer style: fill stipple
/// included when the style's fill is `stipple`; translucent fills get the
/// OpenGL color compensation of Q-UI-35 (display::opengl_supercolor).
[[nodiscard]] LayerStyle to_layer_style(const display::ResolvedStyle& style, bool fill);

/// Builds the batches in redisplay order: for each scene layer its fill
/// (solid or stipple rectangles, or the diagonals of a `cross` style), its
/// triangles, then its outline; then label rectangles, port lines and glyphs;
/// then cell boxes.
[[nodiscard]] LayoutBatches to_batches(const display::LayoutScene& scene);

}  // namespace hefesto::render
