// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The renderer's input: a toolkit- and GPU-free description of what a
// layout window shows, built in the core from a cell, the technology and
// the display styles (capability hefesto-rendering "Style-file-faithful 2D
// drawing"; ui-windows-and-views "Redisplay order for layout windows",
// "Label display", "Subcell display", "DRC error display";
// hefesto-design-system "Layer colors come from the PDK, not the theme").
//
// The Vulkan renderer (src/render/vulkan, hefesto/render/layout_scene.hpp)
// converts a LayoutScene into GPU batches; the desktop shell builds the layer
// palette from layer_palette(). Neither the core nor this header knows Vulkan
// or Raven. View-dependent rules (pixel thresholds such as "skip outlines of
// rectangles under 4 pixels", label text sizes) are applied by the renderer.
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::display {

/// A display style resolved to concrete drawing parameters.
struct ResolvedStyle {
  std::size_t index = 0;  // style-table index
  float r = 1.0F;         // straight RGB, 0..1, from the color map (sRGB)
  float g = 1.0F;
  float b = 1.0F;
  float a = 1.0F;         // OpenGL class: mask/127 (Q-UI-35); 1 otherwise
  FillKind fill = FillKind::solid;
  Stipple stipple{};      // valid when fill == stipple
  std::int32_t outline = 0;  // 8-bit line pattern; 0 = no outline

  friend bool operator==(const ResolvedStyle&, const ResolvedStyle&) = default;
};

/// Resolves table index `index` of `set` (color, opacity, stipple). An index
/// outside the table resolves to opaque white with no outline.
[[nodiscard]] ResolvedStyle resolve_style(const StyleSet& set, std::size_t index);

/// The color Magic's OpenGL driver actually sends for a translucent style:
/// c' = 2c - 0.8 per channel when a < 1, so that the blend over the
/// background gives the intended color (Q-UI-35). Opaque styles keep c.
[[nodiscard]] ResolvedStyle opengl_supercolor(const ResolvedStyle& style) noexcept;

struct SceneTriangle {
  db::Rect bounds;
  db::Diagonal diagonal = db::Diagonal::nw_se;
  bool right_side = false;
};

/// A line segment in root coordinates (outlines, port lines). Horizontal or
/// vertical except where stated.
struct SceneSegment {
  db::Point a;
  db::Point b;
  friend bool operator==(const SceneSegment&, const SceneSegment&) = default;
};

/// Everything drawn with one style, in root-cell coordinates.
struct SceneLayer {
  ResolvedStyle style;
  bool pale = false;                   // the pale variant of a layout style
  std::vector<db::Rect> rects;         // fill (and, for `cross`, the diagonals)
  std::vector<SceneTriangle> triangles;
  /// "Outline and fill rendering semantics": boundary between the style's
  /// material and other material only; internal edges between abutting
  /// tiles of the same style are not included. Empty when outline == 0.
  std::vector<SceneSegment> outline;
  /// Position in the redisplay order: 2 × (layout style − first layout
  /// style) + 1 for the pale variant (splice_scene merges by it).
  std::size_t order = 0;
};

struct SceneLabel {
  std::string text;
  tech::TileType type = tech::kSpace;
  db::Rect rect;                    // root coordinates
  db::Position justification = 0;   // transformed into root coordinates
  bool port = false;
  bool pale = false;
  ResolvedStyle style;              // labels (12), ports (13) or the pale 27 / 28
  /// "For each port, a line in the port-connection style (14) along each
  /// side of the label rectangle flagged in its port directions".
  std::vector<SceneSegment> port_lines;
  ResolvedStyle port_line_style;
  /// Font labels only: the glyph contours (nonzero fill) in root
  /// coordinates, and the rendered bounding box.
  bool font_label = false;
  std::int32_t size = 0;            // 1/8 units
  std::vector<std::vector<PointF>> outline_polygons;
  db::Rect font_bbox;
};

/// An unexpanded subcell: its outline and captions ("Subcell display").
struct SceneCell {
  db::Rect bbox;           // root coordinates (whole array)
  std::string name;        // the definition name (drawn at 2/3 height)
  std::string id;          // the instance id (drawn at 1/3 height)
  int depth = 0;
  ResolvedStyle style;     // cell bounding boxes and names (15)
};

/// The whole window content in Magic's redisplay order: layers first (in
/// style order, the pale variant of a style right after its normal one),
/// then labels, then subcell boxes.
struct LayoutScene {
  std::vector<SceneLayer> layers;
  std::vector<SceneLabel> labels;
  std::vector<SceneCell> cells;
  ResolvedStyle background;  // window background (23)
  db::Rect bounds;           // of everything above; null when empty
};

/// Expansion test for one instance (Magic keeps expansion per use, per window).
using ExpandedFn = std::function<bool(const db::CellUse& use)>;

/// Where build_scene reads paint: enumerates the tiles of `def`'s plane
/// `plane` overlapping `area` whose type is in `types`. The default reads
/// db::Plane::enumerate; a substitute serves tests and future overlays.
using PaintSource = std::function<bool(const db::CellDef& def, tech::PlaneId plane, const db::Rect& area,
                                       const tech::TypeMask& types, const db::TileVisitor& visit)>;

/// "Redisplay order for layout windows" / font-labels "Font labels in
/// bounding boxes and searches": a label is drawn for an area when its
/// rectangle touches the area or, for a font label, its rendered bounding
/// box does (the display search).
[[nodiscard]] bool label_in_display_area(const SceneLabel& label, const db::Rect& area) noexcept;

struct SceneOptions {
  /// Root coordinates to include: tiles touching the area grown by one unit,
  /// labels per label_in_display_area() with the area grown by
  /// `label_extent`, instances whose bounding box (labels included) touches it.
  db::Rect area = db::whole_plane();
  db::Coord label_extent = 0;                      // layout units
  tech::TypeMask visible = tech::TypeMask::all();  // `see` state (space is never hidden)
  ExpandedFn expanded;                             // empty = nothing but the root expanded
  bool show_labels = true;                         // `see labels`
  bool show_cells = true;                          // `see subcells`
  bool all_same = false;                           // `see allSame`: no pale styles
  /// Edit context. `edit_root` false: the window root is not the edit cell's
  /// root, so everything is pale. Otherwise paint is normal only in
  /// `edit_cell` (nullptr: the root itself) under `edit_to_root`.
  bool edit_root = true;
  const db::CellDef* edit_cell = nullptr;
  db::Transform edit_to_root;
  bool magic_compat = true;
  PaintSource paint_source;                        // empty = db::Plane::enumerate
};

/// Builds the scene for `root`. Pure: no I/O, no GPU, deterministic.
[[nodiscard]] diag::Outcome<LayoutScene> build_scene(const db::CellDef& root, const tech::Technology& tech,
                                                     const StyleSet& styles, const FontRegistry& fonts,
                                                     const SceneOptions& options);

/// "Change-driven redisplay" for a retained scene: replaces what `scene`
/// shows in `area` (root coordinates) by `partial`, a scene built with the
/// same options except `SceneOptions::area = area`. Fills and outlines are
/// cut at the area's edge (outside from `scene`, inside from `partial`);
/// split tiles, labels (label_in_display_area) and unexpanded instances
/// touching the area come from `partial`. Returns false, leaving `scene`
/// unchanged, when a cut would change what is drawn (a `cross` fill draws a
/// diagonal per rectangle): rebuild the whole scene instead.
[[nodiscard]] bool splice_scene(LayoutScene& scene, LayoutScene partial, const db::Rect& area);

/// The boundary of a set of non-overlapping rectangles: the parts of their
/// edges not shared with another rectangle of the set (used for outlines).
[[nodiscard]] std::vector<SceneSegment> boundary_segments(const std::vector<db::Rect>& rects);

/// "Box display" (ui-windows-and-views): how the box is drawn at a zoom.
enum class BoxShape : std::uint8_t {
  point,    // a single screen point: outline-highlight cross plus a 1-pixel-larger square
  outline,  // a nonzero dimension under 4 pixels: plain outline (a line box stays thick)
  thick,    // 2-pixel-wide sides inside the rectangle
};
struct BoxDrawing {
  BoxShape shape = BoxShape::thick;
  std::size_t style = kStyleBox;  // 1 solid, 2 medium (box outside the edit cell), 6 outline highlights
};
/// `box_px_width` / `box_px_height` are the box's size on screen in pixels;
/// `edit_bbox` the edit cell's bounding box in the same coordinates as `box`
/// (none when the window does not show the edit cell).
[[nodiscard]] BoxDrawing box_drawing(const db::Rect& box, std::int64_t box_px_width, std::int64_t box_px_height,
                                     const std::optional<db::Rect>& edit_bbox);

/// One entry of the layer palette shown by the desktop shell.
struct LayerSwatch {
  tech::TileType type = tech::kSpace;
  std::string name;         // long type name
  std::string plane;        // home plane long name
  bool contact = false;
  std::vector<ResolvedStyle> styles;  // as drawn on the canvas
};

/// The layers of `tech` that have a display style, in technology order.
[[nodiscard]] std::vector<LayerSwatch> layer_palette(const tech::Technology& tech, const StyleSet& styles);

}  // namespace hefesto::display
