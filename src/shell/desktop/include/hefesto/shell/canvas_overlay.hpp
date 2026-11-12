// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Geometry of what the shell draws over the layout canvas (Desktop-Layout
// mockup): rulers on the top and left edges, the zoom control (− / % / +)
// at the bottom left, the 168×112 minimap at the bottom right, the box as
// Magic draws it, bitmap label text sizes; and the mapping between the core window's ViewMapping and
// the renderer's Camera (design.md D5). Toolkit-free and unit-tested.
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2, D5 and D6).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/display/scene.hpp"
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
/// ".5", "-0.5" -> "-.5"), zero as "0" ("0.0" -> "0") and thousands as "k"
/// ("1000" -> "1k", "-1500" -> "-1.5k"); other labels are returned unchanged.
[[nodiscard]] std::string compact_ruler_label(std::string_view label);

/// Size of the left ruler's labels: the mockup's JetBrains Mono 10, which
/// fits three characters ("-.5", "2.7") in the 20 px ruler.
inline constexpr double kLeftRulerLabelSize = 10.0;
/// Size of the top ruler's labels: the mockup's JetBrains Mono 10.
inline constexpr double kTopRulerLabelSize = 10.0;

/// How many characters of `char_px` fit across a ruler `ruler_px` wide with
/// a pixel of air on each side (at least 1).
[[nodiscard]] std::size_t ruler_label_max_chars(double ruler_px, double char_px);

/// The lines a ruler label is drawn on in a ruler `max_chars` characters
/// wide: the label itself when it fits, else its sign on a line of its own
/// and the rest in lines of `max_chars` ("-500" in 3 -> "-", "500"), so
/// nothing spills onto the canvas.
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

/// One filled rectangle of the minimap's layout thumbnail, in minimap pixels.
struct MinimapFill {
  PixelRect rect;
  Rgba color;  // opaque
};

/// A thumbnail of `scene`'s paint for the minimap ("Minimap" of the
/// Desktop-Layout mockup): the content rectangle of minimap_geometry() is cut
/// into cells of `cell_px` pixels; each cell takes the PDK colors of the
/// solid and stippled layers that cover it, composited in redisplay order
/// over `background` at the styles' opacity (stipples and pale layers at
/// half of it); cross styles (contacts) at half strength, outline-only
/// styles (wells) along their rectangles' edges, then the label rectangles
/// and the boxes of unexpanded cells (M1 gap closed in M2, design.md D10).
/// Adjacent cells of one color on a row are merged into one fill. Empty for an empty
/// scene or null `bounds`.
[[nodiscard]] std::vector<MinimapFill> minimap_thumbnail(const display::LayoutScene& scene, const db::Rect& bounds,
                                                         const Rgba& background, double width, double height,
                                                         double cell_px = 2.0, double padding = 12.0);

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

// --- DRC tag on the found error (DRC-Browser board, note 6) ---------------------

/// The tag's distance from the tile it labels (the board: 14 px right, 12 px below).
inline constexpr double kDrcTagGapX = 14.0;
inline constexpr double kDrcTagGapY = 12.0;

/// Top-left corner of a `tag_w` × `tag_h` tag for the error tile `tile`
/// (canvas pixels, y down) in a `canvas_w` × `canvas_h` canvas: below and
/// right of the tile as on the board; left of it when it would leave the
/// canvas on the right, above it when it would leave at the bottom; then
/// kept inside the canvas (rulers and insets included).
[[nodiscard]] PixelPoint drc_tag_origin(const PixelRect& tile, double tag_w, double tag_h, double canvas_w,
                                        double canvas_h) noexcept;

// --- view sync (design.md D5) -----------------------------------------------------

/// Sets the camera's center and zoom from a core window's mapping
/// (scale in 1/65536 pixel per unit, screen y growing upwards). The
/// camera's viewport size is left as it is.
void camera_from_mapping(const view::ViewMapping& mapping, render::Camera& camera);

/// The mapping that shows what `camera` shows, in framebuffer pixels.
[[nodiscard]] view::ViewMapping mapping_from_camera(const render::Camera& camera);

/// Frames `root` in the camera as the core frames a cell loaded into a
/// window (LayoutWindow::set_root, Magic's DBWloadWindow): load_frame() of
/// its bounding box, (0,0)-(1,1) for a cell not found on disk, fitted to the
/// camera's viewport. Does nothing while the viewport has no size.
void frame_root(const db::CellDef& root, render::Camera& camera);

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

/// Which caption of a cell box (dbwBBoxFunc, selRedisplayCellFunc).
enum class CellCaption : std::uint8_t { name, id };

/// The centre of a cell box caption in canvas pixels (y down) for the box
/// `r` in a window `win_h` pixels tall: the definition name on the line 1/3
/// down from the top of `r`, the instance id on the line 2/3 down from the
/// top of `r` clipped to the window (Magic computes the id's line after
/// GeoClip to the screen area). std::nullopt when `r` is under
/// `min_height` pixels tall (Magic's "BBB" tiny-box test).
[[nodiscard]] std::optional<PixelPoint> cell_caption_center(const PixelRect& r, double win_h, CellCaption which,
                                                            double min_height);

/// Magic's four bitmap label fonts (X font pixel sizes small, medium, large,
/// xlarge: graphics/grTkCommon.h), in logical pixels.
inline constexpr std::array<double, 4> kBitmapLabelSizes{10.0, 14.0, 18.0, 24.0};
/// Height of a capital B relative to the font size (Inter's cap height).
inline constexpr double kCapHeightRatio = 0.727;

/// The text size of bitmap (unfonted) labels at `pixels_per_unit` (logical
/// pixels per internal unit), ui-windows-and-views "Label display": the
/// largest of the four sizes whose `B` height is below pixels-per-unit, the
/// smallest when none is; nullopt (rectangle only, no text) when 3 ×
/// pixels-per-unit does not exceed that size's `B` height.
[[nodiscard]] std::optional<double> bitmap_label_size(double pixels_per_unit);

// --- box (ui-windows-and-views "Box display", Magic's DBWDrawBox) -----------------

/// The filled rectangles (canvas logical pixels, y down) that draw the box
/// `r` of shape `shape` on a canvas of `width` × `height` logical pixels:
/// a point is a cross of ±5 pixels plus a 1-pixel-larger square, a thin box
/// a 1-pixel outline, otherwise four sides 2 pixels wide inside the
/// rectangle (only the sides of nonzero length). A side outside the canvas
/// is not drawn. Magic's pixel counts are logical pixels: they scale with
/// the HiDPI factor (ui-windows-and-views "Box display").
[[nodiscard]] std::vector<PixelRect> box_overlay(const PixelRect& r, display::BoxShape shape, double width,
                                                 double height);

struct PixelSpan {
  double x0 = 0.0;
  double x1 = 0.0;
};

/// The spans of the scan line at `y` that lie inside `polygons` under the
/// nonzero winding rule (how font-label glyph contours are filled).
[[nodiscard]] std::vector<PixelSpan> nonzero_spans(const std::vector<std::vector<PixelPoint>>& polygons, double y);

}  // namespace hefesto::shell
