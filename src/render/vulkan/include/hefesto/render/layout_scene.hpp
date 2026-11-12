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
// Owner: VIEW (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hefesto/display/scene.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::render {

/// What a batch layer draws (for tests and the shell's legend).
enum class BatchKind : std::uint8_t {
  fill, outline, label, port_line, cell_box, diagonals, triangles, glyphs,
  highlight,  // a highlight style's fills, outlines, lines, crosses and triangles
};

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

/// How highlight lines are sized on screen.
struct HighlightOptions {
  /// Framebuffer pixels per logical pixel (2 on a Retina display).
  double pixel_ratio = 1.0;
};

/// The line width of highlight outlines in framebuffer pixels: one logical
/// pixel (`pixel_ratio` rounded, at least 1, at most 63).
[[nodiscard]] std::uint8_t highlight_line_width(const HighlightOptions& options) noexcept;

/// Converts a resolved display style to a GPU layer style: fill stipple
/// included when the style's fill is `stipple`; translucent fills get the
/// OpenGL color compensation of Q-UI-35 (display::opengl_supercolor).
[[nodiscard]] LayerStyle to_layer_style(const display::ResolvedStyle& style, bool fill);

/// Builds the batches in redisplay order: for each scene layer its fill
/// (solid or stipple rectangles, or the diagonals of a `cross` style), its
/// triangles, then its outline; then label rectangles, port lines and glyphs;
/// then cell boxes; then, when given, the highlights over everything
/// (ui-feedback-and-highlights "Highlight layer and clients": the box,
/// feedback, elements, crosshair and the selection in white solid outlines,
/// in client order). Highlight text items are the front end's (like label
/// text); a cross is drawn as its point, since its 5-pixel arms depend on the
/// zoom.
[[nodiscard]] LayoutBatches to_batches(const display::LayoutScene& scene,
                                       const display::HighlightScene* highlights = nullptr,
                                       const HighlightOptions& options = {});

/// The batches of a highlight scene alone (its own layer numbering), for a
/// front end that draws highlights as a separate pass over a retained layout.
/// Lines are one logical pixel wide (`options.pixel_ratio` framebuffer
/// pixels), so that the selection outline does not thin out on a Retina
/// display; the box drawn thick (display::box_drawing: an outline item in the
/// box style or, outside the edit cell, the medium-highlights style) has
/// sides two logical pixels wide inside the box, as Magic's DBWDrawBox draws
/// its 2-pixel sides. Each of the box's four sides is its own batch layer.
[[nodiscard]] LayoutBatches highlight_batches(const display::HighlightScene& highlights,
                                              const HighlightOptions& options = {});

}  // namespace hefesto::render
