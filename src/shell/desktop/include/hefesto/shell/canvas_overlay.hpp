// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Geometry of what the shell draws over the layout canvas (Desktop-Layout
// mockup): rulers on the top and left edges, the zoom control (− / % / +)
// at the bottom left, the 168×112 minimap at the bottom right, the box in
// white dashes; and the mapping between the core window's ViewMapping and
// the renderer's Camera (design.md D5). Toolkit-free and unit-tested.
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2, D5 and D6).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/render/camera.hpp"
#include "hefesto/shell/view_models.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::shell {

// --- rulers ------------------------------------------------------------------

struct RulerTick {
  double pixel = 0.0;   // along the ruler, from its start
  std::string label;    // in display units
};

/// Labelled ticks for a ruler showing world coordinates `world_start` at
/// pixel 0 to `world_end` at pixel `length_px` (world_end may be smaller,
/// for the vertical ruler where y grows upwards). Steps are 1, 2 or 5 × 10^k
/// display units, at least `min_spacing_px` apart.
[[nodiscard]] std::vector<RulerTick> ruler_ticks(double world_start, double world_end, double length_px,
                                                 const UnitScale& scale, double min_spacing_px = 120.0);

/// The left ruler's label in its shortest form: no leading zero ("0.5" ->
/// ".5", "-0.5" -> "-.5") and thousands as "k" ("1000" -> "1k", "-1500" ->
/// "-1.5k"); other labels are returned unchanged.
[[nodiscard]] std::string compact_ruler_label(std::string_view label);

/// The lines a ruler label is drawn on in a ruler `max_chars` characters
/// wide: the label itself when it fits, else one character per line
/// (stacked top to bottom), so nothing spills onto the canvas.
[[nodiscard]] std::vector<std::string> ruler_label_lines(std::string_view label, std::size_t max_chars);

// --- minimap -------------------------------------------------------------------

struct PixelRect {
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;

  [[nodiscard]] bool contains(double px, double py) const noexcept {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

struct PixelPoint {
  double x = 0.0;
  double y = 0.0;
};

struct MinimapGeometry {
  PixelRect content;  // the cell's bounding box, fitted and centered
  PixelRect view;     // the visible world window, clipped to the minimap
};

/// Inside a minimap of `width` × `height` pixels with `padding`; nullopt when
/// `bounds` is null.
[[nodiscard]] std::optional<MinimapGeometry> minimap_geometry(const db::Rect& bounds,
                                                              const render::WorldWindow& visible, double width,
                                                              double height, double padding = 12.0);

/// The world point at a minimap pixel (click to center the view there).
[[nodiscard]] db::Point minimap_to_world(const db::Rect& bounds, double width, double height, double px, double py,
                                         double padding = 12.0);

// --- zoom control and hit testing ----------------------------------------------

/// "400%": the zoom relative to the zoom that fits the cell (100%).
[[nodiscard]] std::string zoom_percent_text(double pixels_per_unit, double fit_pixels_per_unit);

enum class OverlayHit : std::uint8_t { none, zoom_out, zoom_in, zoom_label, minimap };

struct OverlayLayout {
  PixelRect zoom_out;
  PixelRect zoom_label;
  PixelRect zoom_in;
  PixelRect minimap;
  PixelRect ruler_top;
  PixelRect ruler_left;
};

/// Places the overlay elements in a canvas of `width` × `height` logical
/// pixels with the mockup's insets (rulers 20 px, controls 16 px from the
/// edges, zoom control 36 px from the left).
[[nodiscard]] OverlayLayout overlay_layout(double width, double height);
[[nodiscard]] OverlayHit hit_test(const OverlayLayout& layout, double x, double y) noexcept;

// --- view sync (design.md D5) -----------------------------------------------------

/// Sets the camera's center and zoom from a core window's mapping
/// (scale in 1/65536 pixel per unit, screen y growing upwards). The
/// camera's viewport size is left as it is.
void camera_from_mapping(const view::ViewMapping& mapping, render::Camera& camera);

/// The mapping that shows what `camera` shows, in framebuffer pixels.
[[nodiscard]] view::ViewMapping mapping_from_camera(const render::Camera& camera);

/// The world rectangle of the box in canvas pixels (y down), for the white
/// dashed outline.
[[nodiscard]] PixelRect world_to_pixels(const render::Camera& camera, const db::Rect& rect, double pixel_ratio);

/// A layout point in canvas pixels (y down).
[[nodiscard]] PixelPoint world_point_to_pixels(const render::Camera& camera, double x, double y, double pixel_ratio);

// --- labels (Magic's DBWdisplay / GrPutText placement) ---------------------------

/// Top-left corner of a label's text of `text_w` × `text_h` pixels for a
/// label rectangle `r` (canvas pixels) and justification `pos`
/// (db::kCenter .. db::kNorthWest): anchored at the side or corner named by
/// `pos` and offset by Magic's TEXTOFFSET (5 px) away from it.
[[nodiscard]] PixelPoint label_text_origin(const PixelRect& r, db::Position pos, double text_w, double text_h);

struct PixelSpan {
  double x0 = 0.0;
  double x1 = 0.0;
};

/// The spans of the scan line at `y` that lie inside `polygons` under the
/// nonzero winding rule (how font-label glyph contours are filled).
[[nodiscard]] std::vector<PixelSpan> nonzero_spans(const std::vector<std::vector<PixelPoint>>& polygons, double y);

}  // namespace hefesto::shell
