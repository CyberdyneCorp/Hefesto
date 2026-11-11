// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MinimapThumbnail: the minimap's thumbnail with per-area updates. The cell
// grid, the tints and the compositing order are those of
// minimap_thumbnail() (canvas_overlay.cpp), which a full rebuild reproduces
// fill for fill (tests/unit/shell/minimap_test.cpp). Owner: CARRY.

#include "hefesto/shell/minimap.hpp"

#include <algorithm>
#include <cmath>

namespace hefesto::shell {

namespace {

/// How a layer shows in the thumbnail: filled, along its rectangles' edges
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

/// Equal at 8 bits per channel (what the screen shows).
bool same(const Rgba& x, const Rgba& y) {
  const auto q = [](float v) { return std::lround(v * 255.0F); };
  return (x.a > 0.0F) == (y.a > 0.0F) && q(x.r) == q(y.r) && q(x.g) == q(y.g) && q(x.b) == q(y.b);
}

MinimapCells intersect(const MinimapCells& a, const MinimapCells& b) {
  return MinimapCells{std::max(a.col0, b.col0), std::min(a.col1, b.col1), std::max(a.row0, b.row0),
                      std::min(a.row1, b.row1)};
}

}  // namespace

MinimapThumbnail::Tint MinimapThumbnail::style_tint(const display::ResolvedStyle& style, float alpha) {
  return Tint{std::clamp(style.r, 0.0F, 1.0F), std::clamp(style.g, 0.0F, 1.0F), std::clamp(style.b, 0.0F, 1.0F),
              alpha};
}

MinimapThumbnail::Tint MinimapThumbnail::layer_tint(const display::SceneLayer& layer) {
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

void MinimapThumbnail::rebuild(const display::LayoutScene& scene, const MinimapFrame& frame) {
  frame_ = frame;
  built_ = false;
  cells_.clear();
  last_composited_ = 0;
  const auto g = minimap_geometry(frame.bounds, render::WorldWindow{}, frame.width, frame.height, frame.padding);
  if (!g || !(frame.cell_px > 0.0) || !(g->content.w > 0.0) || !(g->content.h > 0.0)) {
    return;
  }
  content_ = g->content;
  scale_ = content_.w / std::max(static_cast<double>(frame.bounds.width()), 1.0);
  cols_ = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(content_.w / frame.cell_px)));
  rows_ = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(content_.h / frame.cell_px)));
  cells_.assign(cols_ * rows_, Rgba{frame.background.r, frame.background.g, frame.background.b, 0.0F});
  built_ = true;
  composite(scene, MinimapCells{0, cols_, 0, rows_});
}

std::pair<std::size_t, std::size_t> MinimapThumbnail::span(double lo, double hi, std::size_t count) const {
  // The cells [first, last) whose centers world offsets [lo, hi) cover; a
  // rectangle that covers no center (thinner than a cell) takes the cell it
  // starts in, so it still shows.
  const double max = static_cast<double>(count);
  const double a = lo * scale_ / frame_.cell_px;
  const double b = hi * scale_ / frame_.cell_px;
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

MinimapCells MinimapThumbnail::rect_cells(const db::Rect& r) const {
  const db::Rect& b = frame_.bounds;
  const auto [c0, c1] = span(static_cast<double>(r.x0 - b.x0), static_cast<double>(r.x1 - b.x0), cols_);
  const auto [r0, r1] = span(static_cast<double>(b.y1 - r.y1), static_cast<double>(b.y1 - r.y0), rows_);
  return MinimapCells{c0, c1, r0, r1};
}

void MinimapThumbnail::blend(Rgba& c, const Tint& t) {
  c.r += (t.r - c.r) * t.a;
  c.g += (t.g - c.g) * t.a;
  c.b += (t.b - c.b) * t.a;
  c.a = 1.0F;  // painted
}

void MinimapThumbnail::paint(const db::Rect& r, const Tint& tint, const MinimapCells& clip) {
  const MinimapCells cells = intersect(rect_cells(r), clip);
  for (std::size_t row = cells.row0; row < cells.row1; ++row) {
    for (std::size_t col = cells.col0; col < cells.col1; ++col) blend(cells_[row * cols_ + col], tint);
  }
}

void MinimapThumbnail::edges(const db::Rect& r, const Tint& tint, const MinimapCells& clip) {
  paint(db::Rect{r.x0, r.y0, r.x1, r.y0}, tint, clip);
  paint(db::Rect{r.x0, r.y1, r.x1, r.y1}, tint, clip);
  paint(db::Rect{r.x0, r.y0, r.x0, r.y1}, tint, clip);
  paint(db::Rect{r.x1, r.y0, r.x1, r.y1}, tint, clip);
}

void MinimapThumbnail::composite(const display::LayoutScene& scene, const MinimapCells& clip) {
  last_composited_ = (clip.col1 - clip.col0) * (clip.row1 - clip.row0);
  for (const display::SceneLayer& layer : scene.layers) {
    const Tint tint = layer_tint(layer);
    const ThumbShape shape = thumb_shape(layer.style);
    if (!(tint.a > 0.0F) || shape == ThumbShape::none) continue;
    for (const db::Rect& r : layer.rects) {
      if (shape == ThumbShape::edges) {
        edges(r, tint, clip);
      } else {
        paint(r, tint, clip);
      }
    }
  }
  // Labels as points of their style, unexpanded cells as their box outline,
  // over the paint, in Magic's redisplay order.
  for (const display::SceneLabel& label : scene.labels) paint(label.rect, style_tint(label.style, 0.9F), clip);
  for (const display::SceneCell& cell : scene.cells) edges(cell.bbox, style_tint(cell.style, 0.8F), clip);
}

MinimapCells MinimapThumbnail::dirty_cells(const db::Rect& area) const {
  if (!built_ || area.x1 < area.x0 || area.y1 < area.y0) return {};  // a point or line area is an edit too
  const db::Rect& b = frame_.bounds;
  if (area.x1 < b.x0 || area.x0 > b.x1 || area.y1 < b.y0 || area.y0 > b.y1) return {};
  // Every cell whose closed world extent touches the area: floor of the
  // low edge, and the cell after the high edge when it lies on a boundary.
  const double unit = scale_ / frame_.cell_px;  // cells per world unit
  const auto cells = [unit](double lo, double hi, std::size_t count) {
    const double max = static_cast<double>(count);
    const double first = std::clamp(std::floor(lo * unit) - (std::floor(lo * unit) == lo * unit ? 1.0 : 0.0), 0.0,
                                    max - 1.0);
    const double last = std::clamp(std::floor(hi * unit) + 1.0, first + 1.0, max);
    return std::pair{static_cast<std::size_t>(first), static_cast<std::size_t>(last)};
  };
  const auto [c0, c1] = cells(static_cast<double>(area.x0 - b.x0), static_cast<double>(area.x1 - b.x0), cols_);
  const auto [r0, r1] = cells(static_cast<double>(b.y1 - area.y1), static_cast<double>(b.y1 - area.y0), rows_);
  return MinimapCells{c0, c1, r0, r1};
}

std::optional<db::Rect> MinimapThumbnail::dirty_area(const db::Rect& area) const {
  const MinimapCells cells = dirty_cells(area);
  if (cells.empty()) return std::nullopt;
  const db::Rect& b = frame_.bounds;
  const double units = frame_.cell_px / scale_;  // world units per cell
  const auto at = [](double v) { return static_cast<db::Coord>(v); };
  return db::Rect{at(std::floor(static_cast<double>(b.x0) + static_cast<double>(cells.col0) * units)),
                  at(std::floor(static_cast<double>(b.y1) - static_cast<double>(cells.row1) * units)),
                  at(std::ceil(static_cast<double>(b.x0) + static_cast<double>(cells.col1) * units)),
                  at(std::ceil(static_cast<double>(b.y1) - static_cast<double>(cells.row0) * units))};
}

void MinimapThumbnail::update(const display::LayoutScene& partial, const db::Rect& area) {
  const MinimapCells dirty = dirty_cells(area);
  last_composited_ = 0;
  if (dirty.empty()) return;
  const Rgba unpainted{frame_.background.r, frame_.background.g, frame_.background.b, 0.0F};
  for (std::size_t row = dirty.row0; row < dirty.row1; ++row) {
    std::fill_n(cells_.begin() + static_cast<std::ptrdiff_t>(row * cols_ + dirty.col0), dirty.col1 - dirty.col0,
                unpainted);
  }
  composite(partial, dirty);
}

std::vector<MinimapFill> MinimapThumbnail::fills() const {
  std::vector<MinimapFill> out;
  if (!built_) return out;
  const double cell = frame_.cell_px;
  for (std::size_t row = 0; row < rows_; ++row) {
    std::size_t col = 0;
    while (col < cols_) {
      const Rgba& c = cells_[row * cols_ + col];
      std::size_t end = col + 1;
      while (end < cols_ && same(cells_[row * cols_ + end], c)) ++end;
      if (c.a > 0.0F) {
        const double x = content_.x + static_cast<double>(col) * cell;
        const double y = content_.y + static_cast<double>(row) * cell;
        const double right = std::min(content_.x + static_cast<double>(end) * cell, content_.x + content_.w);
        const double bottom = std::min(y + cell, content_.y + content_.h);
        out.push_back(MinimapFill{PixelRect{x, y, right - x, bottom - y}, Rgba{c.r, c.g, c.b, 1.0F}});
      }
      col = end;
    }
  }
  return out;
}

}  // namespace hefesto::shell
