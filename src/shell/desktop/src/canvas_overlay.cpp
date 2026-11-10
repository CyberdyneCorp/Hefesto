// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/canvas_overlay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace hefesto::shell {

namespace {

constexpr double kFixedOne = 65536.0;

/// The smallest 1/2/5 × 10^k not below `raw`.
double nice_step(double raw) {
  if (!(raw > 0.0) || !std::isfinite(raw)) {
    return 1.0;
  }
  const double decade = std::pow(10.0, std::floor(std::log10(raw)));
  for (const double m : {1.0, 2.0, 5.0, 10.0}) {
    if (m * decade >= raw) {
      return m * decade;
    }
  }
  return 10.0 * decade;
}

std::string tick_label(double value, double step) {
  // Enough decimals to tell neighbouring ticks apart.
  const int decimals = step >= 1.0 ? 0 : static_cast<int>(std::ceil(-std::log10(step) - 1e-9));
  char buffer[48];
  std::snprintf(buffer, sizeof buffer, "%.*f", std::clamp(decimals, 0, 6), std::abs(value) < step * 1e-6 ? 0.0 : value);
  return buffer;
}

double fit_scale(const db::Rect& bounds, double width, double height, double padding) {
  const double w = std::max(static_cast<double>(bounds.width()), 1.0);
  const double h = std::max(static_cast<double>(bounds.height()), 1.0);
  return std::min((width - 2.0 * padding) / w, (height - 2.0 * padding) / h);
}

PixelRect clip(const PixelRect& r, double width, double height) {
  const double x0 = std::clamp(r.x, 0.0, width);
  const double y0 = std::clamp(r.y, 0.0, height);
  const double x1 = std::clamp(r.x + r.w, 0.0, width);
  const double y1 = std::clamp(r.y + r.h, 0.0, height);
  return PixelRect{x0, y0, x1 - x0, y1 - y0};
}

}  // namespace

std::vector<RulerTick> ruler_ticks(double world_start, double world_end, double length_px, const UnitScale& scale,
                                   double min_spacing_px) {
  std::vector<RulerTick> ticks;
  const double d0 = scale.to_display(world_start);
  const double d1 = scale.to_display(world_end);
  const double span = d1 - d0;
  if (!(length_px > 0.0) || span == 0.0 || !std::isfinite(span)) {
    return ticks;
  }
  const double step = nice_step(std::abs(span) * min_spacing_px / length_px);
  const double lo = std::min(d0, d1);
  const double hi = std::max(d0, d1);
  for (double v = std::ceil(lo / step) * step; v <= hi && ticks.size() < 200; v += step) {
    ticks.push_back(RulerTick{(v - d0) / span * length_px, tick_label(v, step)});
  }
  return ticks;
}

namespace {

/// "0", "0.0", "-0.00": a zero however it was printed.
bool is_zero_label(std::string_view digits) {
  return !digits.empty() && digits.find_first_not_of("0.") == std::string_view::npos;
}

}  // namespace

std::string compact_ruler_label(std::string_view label) {
  const bool negative = label.starts_with('-');
  std::string_view digits = negative ? label.substr(1) : label;
  if (is_zero_label(digits)) {
    return "0";  // not ".0" or "-.0"
  }
  std::string out;
  const bool integer = !digits.empty() && digits.find_first_not_of("0123456789") == std::string_view::npos;
  if (integer && digits.size() > 3) {
    // Thousands: "1500" -> "1.5k", trailing zeros of the fraction dropped.
    std::string whole(digits.substr(0, digits.size() - 3));
    std::string frac(digits.substr(digits.size() - 3));
    while (!frac.empty() && frac.back() == '0') frac.pop_back();
    out = whole + (frac.empty() ? "" : "." + frac) + "k";
  } else if (digits.starts_with("0.")) {
    out = std::string(digits.substr(1));
  } else {
    out = std::string(digits);
  }
  return negative ? "-" + out : out;
}

std::size_t ruler_label_max_chars(double ruler_px, double char_px) {
  if (!(char_px > 0.0)) return 1;
  const double usable = ruler_px - 2.0;  // a pixel of air on each side
  return std::max<std::size_t>(1, static_cast<std::size_t>(std::floor(usable / char_px + 1e-6)));
}

std::vector<std::string> ruler_label_lines(std::string_view label, std::size_t max_chars) {
  max_chars = std::max<std::size_t>(max_chars, 1);
  if (label.size() <= max_chars) {
    return {std::string(label)};
  }
  std::vector<std::string> lines;
  if (label.starts_with('-')) {
    lines.emplace_back("-");  // the sign alone, the number below it whole when it fits
    label.remove_prefix(1);
  }
  for (std::size_t i = 0; i < label.size(); i += max_chars) {
    lines.emplace_back(label.substr(i, max_chars));
  }
  return lines;
}

std::optional<MinimapGeometry> minimap_geometry(const db::Rect& bounds, const render::WorldWindow& visible,
                                                double width, double height, double padding) {
  if (bounds.is_null()) {
    return std::nullopt;
  }
  const double s = fit_scale(bounds, width, height, padding);
  const double cw = static_cast<double>(bounds.width()) * s;
  const double ch = static_cast<double>(bounds.height()) * s;
  const double ox = (width - cw) / 2.0;
  const double oy = (height - ch) / 2.0;
  const auto px = [&](double wx) { return ox + (wx - bounds.x0) * s; };
  const auto py = [&](double wy) { return oy + ch - (wy - bounds.y0) * s; };  // y up in the world
  MinimapGeometry g;
  g.content = PixelRect{ox, oy, cw, ch};
  const PixelRect view{px(visible.x0), py(visible.y1), (visible.x1 - visible.x0) * s, (visible.y1 - visible.y0) * s};
  g.view = clip(view, width, height);
  return g;
}

namespace {

/// What one layer adds to a thumbnail cell: its color and opacity.
struct Tint {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 0.0F;  // 0: the layer is left out
};

/// How a layer shows in the thumbnail: filled, or only its rectangles' edges
/// (outline-only styles: wells), or not at all.
enum class ThumbShape : std::uint8_t { fill, edges, none };

ThumbShape thumb_shape(const display::ResolvedStyle& style) {
  switch (style.fill) {
    case display::FillKind::solid:
    case display::FillKind::stipple:
    case display::FillKind::cross: return ThumbShape::fill;
    case display::FillKind::outline: return ThumbShape::edges;
    case display::FillKind::grid: break;
  }
  return ThumbShape::none;
}

Tint style_tint(const display::ResolvedStyle& style, float alpha) {
  return Tint{std::clamp(style.r, 0.0F, 1.0F), std::clamp(style.g, 0.0F, 1.0F), std::clamp(style.b, 0.0F, 1.0F),
              alpha};
}

Tint layer_tint(const display::SceneLayer& layer) {
  if (layer.style.fill == display::FillKind::outline) {
    return style_tint(layer.style, layer.pale ? 0.35F : 0.7F);  // the outline color, opaque enough to see
  }
  if (layer.style.fill == display::FillKind::cross) {
    // A contact's cross: the cell takes the cross color at half strength.
    return style_tint(display::opengl_supercolor(layer.style), layer.pale ? 0.25F : 0.5F);
  }
  if (layer.style.fill != display::FillKind::solid && layer.style.fill != display::FillKind::stipple) {
    return {};
  }
  // The color the renderer blends (Q-UI-35), so the thumbnail matches the canvas.
  const display::ResolvedStyle st = display::opengl_supercolor(layer.style);
  float a = std::clamp(st.a, 0.0F, 1.0F);
  if (st.fill == display::FillKind::stipple) a *= 0.5F;
  if (layer.pale) a *= 0.5F;
  return Tint{std::clamp(st.r, 0.0F, 1.0F), std::clamp(st.g, 0.0F, 1.0F), std::clamp(st.b, 0.0F, 1.0F), a};
}

/// The thumbnail's cells, row-major from the top-left, composited in place.
class ThumbnailGrid {
 public:
  ThumbnailGrid(const db::Rect& bounds, const PixelRect& content, double cell_px, const Rgba& background)
      : bounds_(bounds),
        cell_(cell_px),
        scale_(content.w / std::max(static_cast<double>(bounds.width()), 1.0)),
        cols_(std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(content.w / cell_px)))),
        rows_(std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(content.h / cell_px)))),
        cells_(cols_ * rows_, Rgba{background.r, background.g, background.b, 0.0F}) {}

  void paint(const db::Rect& r, const Tint& tint) {
    const auto [c0, c1] = span(static_cast<double>(r.x0 - bounds_.x0), static_cast<double>(r.x1 - bounds_.x0), cols_);
    const auto [r0, r1] = span(static_cast<double>(bounds_.y1 - r.y1), static_cast<double>(bounds_.y1 - r.y0), rows_);
    for (std::size_t row = r0; row < r1; ++row) {
      for (std::size_t col = c0; col < c1; ++col) blend(cells_[row * cols_ + col], tint);
    }
  }

  /// The cells along the four sides of `r` (outline-only styles, cell boxes).
  void edges(const db::Rect& r, const Tint& tint) {
    paint(db::Rect{r.x0, r.y0, r.x1, r.y0}, tint);
    paint(db::Rect{r.x0, r.y1, r.x1, r.y1}, tint);
    paint(db::Rect{r.x0, r.y0, r.x0, r.y1}, tint);
    paint(db::Rect{r.x1, r.y0, r.x1, r.y1}, tint);
  }

  /// Runs of one color on each row, offset by the content's corner.
  [[nodiscard]] std::vector<MinimapFill> fills(const PixelRect& content) const {
    std::vector<MinimapFill> out;
    for (std::size_t row = 0; row < rows_; ++row) {
      std::size_t col = 0;
      while (col < cols_) {
        const Rgba& c = cells_[row * cols_ + col];
        std::size_t end = col + 1;
        while (end < cols_ && same(cells_[row * cols_ + end], c)) ++end;
        if (c.a > 0.0F) out.push_back(MinimapFill{cell_rect(content, row, col, end), Rgba{c.r, c.g, c.b, 1.0F}});
        col = end;
      }
    }
    return out;
  }

 private:
  /// The cells [first, last) whose centers world offsets [lo, hi) cover
  /// along an axis of `count` cells; a rectangle that covers no center
  /// (thinner than a cell) takes the cell it starts in, so it still shows.
  [[nodiscard]] std::pair<std::size_t, std::size_t> span(double lo, double hi, std::size_t count) const {
    const double max = static_cast<double>(count);
    const double a = lo * scale_ / cell_;
    const double b = hi * scale_ / cell_;
    double first = std::ceil(a - 0.5);
    double last = std::floor(b - 0.5) + 1.0;
    if (last <= first) {
      first = std::floor(a);
      last = first + 1.0;
    }
    first = std::clamp(first, 0.0, max - 1.0);
    last = std::clamp(last, first + 1.0, max);
    return {static_cast<std::size_t>(first), static_cast<std::size_t>(last)};
  }

  static void blend(Rgba& c, const Tint& t) {
    c.r += (t.r - c.r) * t.a;
    c.g += (t.g - c.g) * t.a;
    c.b += (t.b - c.b) * t.a;
    c.a = 1.0F;  // painted
  }

  /// Equal at 8 bits per channel (what the screen shows).
  static bool same(const Rgba& x, const Rgba& y) {
    const auto q = [](float v) { return std::lround(v * 255.0F); };
    return (x.a > 0.0F) == (y.a > 0.0F) && q(x.r) == q(y.r) && q(x.g) == q(y.g) && q(x.b) == q(y.b);
  }

  [[nodiscard]] PixelRect cell_rect(const PixelRect& content, std::size_t row, std::size_t col,
                                    std::size_t end) const {
    const double x = content.x + static_cast<double>(col) * cell_;
    const double y = content.y + static_cast<double>(row) * cell_;
    const double right = std::min(content.x + static_cast<double>(end) * cell_, content.x + content.w);
    const double bottom = std::min(y + cell_, content.y + content.h);
    return PixelRect{x, y, right - x, bottom - y};
  }

  db::Rect bounds_;
  double cell_;
  double scale_;
  std::size_t cols_;
  std::size_t rows_;
  std::vector<Rgba> cells_;  // a == 0: nothing painted
};

}  // namespace

std::vector<MinimapFill> minimap_thumbnail(const display::LayoutScene& scene, const db::Rect& bounds,
                                           const Rgba& background, double width, double height, double cell_px,
                                           double padding) {
  const auto g = minimap_geometry(bounds, render::WorldWindow{}, width, height, padding);
  if (!g || !(cell_px > 0.0) || !(g->content.w > 0.0) || !(g->content.h > 0.0)) {
    return {};
  }
  ThumbnailGrid grid(bounds, g->content, cell_px, background);
  for (const display::SceneLayer& layer : scene.layers) {
    const Tint tint = layer_tint(layer);
    const ThumbShape shape = thumb_shape(layer.style);
    if (!(tint.a > 0.0F) || shape == ThumbShape::none) continue;
    for (const db::Rect& r : layer.rects) {
      if (shape == ThumbShape::edges) {
        grid.edges(r, tint);
      } else {
        grid.paint(r, tint);
      }
    }
  }
  // Labels as points of their style, unexpanded cells as their box outline,
  // over the paint, in Magic's redisplay order.
  for (const display::SceneLabel& label : scene.labels) grid.paint(label.rect, style_tint(label.style, 0.9F));
  for (const display::SceneCell& cell : scene.cells) grid.edges(cell.bbox, style_tint(cell.style, 0.8F));
  return grid.fills(g->content);
}

db::Point minimap_to_world(const db::Rect& bounds, double width, double height, double px, double py,
                           double padding) {
  const double s = fit_scale(bounds, width, height, padding);
  const double cw = static_cast<double>(bounds.width()) * s;
  const double ch = static_cast<double>(bounds.height()) * s;
  const double ox = (width - cw) / 2.0;
  const double oy = (height - ch) / 2.0;
  const double wx = bounds.x0 + (px - ox) / s;
  const double wy = bounds.y0 + (oy + ch - py) / s;
  return db::Point{static_cast<db::Coord>(std::lround(wx)), static_cast<db::Coord>(std::lround(wy))};
}

std::string zoom_percent_text(double pixels_per_unit, double fit_pixels_per_unit) {
  if (!(fit_pixels_per_unit > 0.0) || !(pixels_per_unit > 0.0)) {
    return "100%";
  }
  const double percent = pixels_per_unit / fit_pixels_per_unit * 100.0;
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, percent >= 10.0 ? "%.0f%%" : "%.1f%%", percent);
  return buffer;
}

OverlayLayout overlay_layout(double width, double height) {
  constexpr double kZoomLeft = 36.0;  // the mockup's left: 36px (ruler 20 + 16)
  constexpr double kGap = 6.0;
  constexpr double kLabelWidth = 64.0;
  const double button = kZoomButtonSize;
  const double inset = kCanvasInset;
  const double top = height - inset - button;
  OverlayLayout l;
  l.zoom_out = PixelRect{kZoomLeft, top, button, button};
  l.zoom_label = PixelRect{kZoomLeft + button + kGap, top, kLabelWidth, button};
  l.zoom_in = PixelRect{kZoomLeft + button + kGap + kLabelWidth + kGap, top, button, button};
  l.minimap = PixelRect{width - inset - kMinimapWidth, height - inset - kMinimapHeight, kMinimapWidth,
                        kMinimapHeight};
  l.ruler_top = PixelRect{0.0, 0.0, width, kRulerSize};
  l.ruler_left = PixelRect{0.0, 0.0, kRulerSize, height};
  return l;
}

OverlayHit hit_test(const OverlayLayout& l, double x, double y) noexcept {
  if (l.zoom_out.contains(x, y)) return OverlayHit::zoom_out;
  if (l.zoom_in.contains(x, y)) return OverlayHit::zoom_in;
  if (l.zoom_label.contains(x, y)) return OverlayHit::zoom_label;
  if (l.minimap.contains(x, y)) return OverlayHit::minimap;
  return OverlayHit::none;
}

void camera_from_mapping(const view::ViewMapping& m, render::Camera& camera) {
  if (m.scale <= 0) {
    return;
  }
  const double scale = static_cast<double>(m.scale);
  const double cx_px = (m.screen.x0 + m.screen.x1 + 1) / 2.0;
  const double cy_px = (m.screen.y0 + m.screen.y1 + 1) / 2.0;
  const double cx = m.surface.x0 + (cx_px * kFixedOne - static_cast<double>(m.origin_x)) / scale;
  const double cy = m.surface.y0 + (cy_px * kFixedOne - static_cast<double>(m.origin_y)) / scale;
  const int w = camera.viewport_width();
  const int h = camera.viewport_height();
  camera = render::Camera(cx, cy, scale / kFixedOne);
  camera.set_viewport(w, h);
}

void frame_root(const db::CellDef& root, render::Camera& camera) {
  const int w = camera.viewport_width();
  const int h = camera.viewport_height();
  if (w <= 0 || h <= 0) {
    return;
  }
  const db::Rect area = root.flags().not_found ? db::Rect{0, 0, 1, 1} : root.bbox();
  camera_from_mapping(view::ViewMapping::fit(view::ScreenRect{0, 0, w - 1, h - 1}, view::load_frame(area)), camera);
}

view::ViewMapping mapping_from_camera(const render::Camera& camera) {
  const int w = camera.viewport_width();
  const int h = camera.viewport_height();
  const double ppu = camera.pixels_per_unit();
  const double left = camera.world_x(0.0);
  const double bottom = camera.world_y(static_cast<double>(h));
  view::ViewMapping m;
  m.screen = view::ScreenRect{0, 0, w - 1, h - 1};
  m.scale = std::max<std::int64_t>(1, std::llround(ppu * kFixedOne));
  m.surface = db::Rect{static_cast<db::Coord>(std::floor(left)), static_cast<db::Coord>(std::floor(bottom)),
                       static_cast<db::Coord>(std::ceil(camera.world_x(static_cast<double>(w)))),
                       static_cast<db::Coord>(std::ceil(camera.world_y(0.0)))};
  m.origin_x = std::llround((m.surface.x0 - left) * static_cast<double>(m.scale));
  m.origin_y = std::llround((m.surface.y0 - bottom) * static_cast<double>(m.scale));
  return m;
}

PixelRect world_to_pixels(const render::Camera& camera, const db::Rect& rect, double pixel_ratio) {
  const double ratio = pixel_ratio > 0.0 ? pixel_ratio : 1.0;
  const double ppu = camera.pixels_per_unit();
  const double x0 = (rect.x0 - camera.center_x()) * ppu + 0.5 * camera.viewport_width();
  const double y1 = 0.5 * camera.viewport_height() - (rect.y1 - camera.center_y()) * ppu;
  return PixelRect{x0 / ratio, y1 / ratio, static_cast<double>(rect.width()) * ppu / ratio,
                   static_cast<double>(rect.height()) * ppu / ratio};
}

PixelPoint world_point_to_pixels(const render::Camera& camera, double x, double y, double pixel_ratio) {
  const double ratio = pixel_ratio > 0.0 ? pixel_ratio : 1.0;
  const double ppu = camera.pixels_per_unit();
  return PixelPoint{((x - camera.center_x()) * ppu + 0.5 * camera.viewport_width()) / ratio,
                    (0.5 * camera.viewport_height() - (y - camera.center_y()) * ppu) / ratio};
}

PixelPoint label_text_origin(const PixelRect& r, db::Position pos, double text_w, double text_h) {
  constexpr double kTextOffset = 5.0;  // Magic's TEXTOFFSET (graphics/grText.c)
  // Per position (GEO_CENTER .. GEO_NORTHWEST): -1 west / south, 0 center, 1 east / north.
  constexpr std::array<int, 9> kHorizontal = {0, 0, 1, 1, 1, 0, -1, -1, -1};
  constexpr std::array<int, 9> kVertical = {0, 1, 1, 0, -1, -1, -1, 0, 1};
  const auto index = static_cast<std::size_t>(pos >= 0 && pos < 9 ? pos : 0);
  const int h = kHorizontal[index];
  const int v = kVertical[index];
  const double ax = r.x + r.w * (h + 1) / 2.0;
  const double ay = r.y + r.h * (1 - v) / 2.0;  // y down: north is the top edge
  const double x = h < 0 ? ax - kTextOffset - text_w : (h > 0 ? ax + kTextOffset : ax - text_w / 2.0);
  const double y = v > 0 ? ay - kTextOffset - text_h : (v < 0 ? ay + kTextOffset : ay - text_h / 2.0);
  return PixelPoint{x, y};
}

std::optional<double> bitmap_label_size(double pixels_per_unit) {
  // DBWdisplay.c: from xlarge down, the first size whose B is below the
  // scale; else small. No text when 3 × the scale does not exceed it.
  const auto cap = [](double size) { return std::round(size * kCapHeightRatio); };
  std::size_t i = kBitmapLabelSizes.size();
  while (i > 0 && !(pixels_per_unit > cap(kBitmapLabelSizes[i - 1]))) --i;
  const double size = kBitmapLabelSizes[i > 0 ? i - 1 : 0];
  if (3.0 * pixels_per_unit <= cap(size)) return std::nullopt;
  return size;
}

namespace {

/// Adds `r` unless it lies entirely outside the canvas.
void add_side(std::vector<PixelRect>& out, const PixelRect& r, double width, double height) {
  if (r.x + r.w < 0.0 || r.y + r.h < 0.0 || r.x > width || r.y > height) return;
  out.push_back(r);
}

std::vector<PixelRect> point_box(const PixelRect& r, double px) {
  const double cross = 5.0 * px;  // GR_CROSSSIZE
  std::vector<PixelRect> out{{r.x - cross, r.y, 2.0 * cross + px, px}, {r.x, r.y - cross, px, 2.0 * cross + px}};
  // GEO_EXPAND by 1: a square one pixel larger on every side.
  const PixelRect sq{r.x - px, r.y - px, 3.0 * px, 3.0 * px};
  out.push_back({sq.x, sq.y, sq.w, px});
  out.push_back({sq.x, sq.y + sq.h - px, sq.w, px});
  out.push_back({sq.x, sq.y, px, sq.h});
  out.push_back({sq.x + sq.w - px, sq.y, px, sq.h});
  return out;
}

}  // namespace

std::vector<PixelRect> box_overlay(const PixelRect& r, display::BoxShape shape, double width, double height) {
  constexpr double px = 1.0;  // one Magic pixel: one logical pixel
  if (shape == display::BoxShape::point) return point_box(r, px);
  const double t = shape == display::BoxShape::thick ? 2.0 * px : px;
  std::vector<PixelRect> out;
  const bool has_w = r.w > 0.0;
  const bool has_h = r.h > 0.0;
  if (has_h || shape == display::BoxShape::outline) {
    add_side(out, {r.x, r.y, t, r.h}, width, height);              // west
    add_side(out, {r.x + r.w - t, r.y, t, r.h}, width, height);    // east
  }
  if (has_w || shape == display::BoxShape::outline) {
    add_side(out, {r.x, r.y + r.h - t, r.w, t}, width, height);    // south (y down)
    add_side(out, {r.x, r.y, r.w, t}, width, height);              // north
  }
  return out;
}

namespace {

struct Crossing {
  double x;
  int winding;  // +1 edge going down, -1 going up
};

/// Where the edges of `polygons` cross the scan line at `y`, sorted by x.
std::vector<Crossing> scanline_crossings(const std::vector<std::vector<PixelPoint>>& polygons, double y) {
  std::vector<Crossing> crossings;
  for (const auto& poly : polygons) {
    for (std::size_t i = 0; i < poly.size(); ++i) {
      const PixelPoint& a = poly[i];
      const PixelPoint& b = poly[(i + 1) % poly.size()];
      if ((a.y <= y) == (b.y <= y)) continue;  // horizontal or not crossing
      crossings.push_back(Crossing{a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y), b.y > a.y ? 1 : -1});
    }
  }
  std::sort(crossings.begin(), crossings.end(), [](const Crossing& l, const Crossing& r) { return l.x < r.x; });
  return crossings;
}

}  // namespace

std::vector<PixelSpan> nonzero_spans(const std::vector<std::vector<PixelPoint>>& polygons, double y) {
  const std::vector<Crossing> crossings = scanline_crossings(polygons, y);
  std::vector<PixelSpan> spans;
  int winding = 0;
  for (std::size_t i = 0; i + 1 < crossings.size(); ++i) {
    winding += crossings[i].winding;
    if (winding != 0 && crossings[i + 1].x > crossings[i].x) {
      spans.push_back(PixelSpan{crossings[i].x, crossings[i + 1].x});
    }
  }
  return spans;
}

}  // namespace hefesto::shell
