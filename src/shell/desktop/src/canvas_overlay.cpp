// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/canvas_overlay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

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

std::string compact_ruler_label(std::string_view label) {
  const bool negative = label.starts_with('-');
  std::string_view digits = negative ? label.substr(1) : label;
  std::string out;
  const bool integer = !digits.empty() && digits.find_first_not_of("0123456789") == std::string_view::npos;
  if (integer && digits.size() > 3 && digits != "0") {
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

std::vector<std::string> ruler_label_lines(std::string_view label, std::size_t max_chars) {
  if (label.size() <= max_chars) {
    return {std::string(label)};
  }
  std::vector<std::string> lines;
  lines.reserve(label.size());
  for (const char c : label) lines.emplace_back(1, c);
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
