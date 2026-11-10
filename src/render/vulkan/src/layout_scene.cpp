// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/layout_scene.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "hefesto/render/polygon_fill.hpp"

namespace hefesto::render {

namespace {

constexpr std::size_t kMaxBatchLayers = std::numeric_limits<std::uint8_t>::max() + 1U;

RectInstance line(const db::Point& a, const db::Point& b) {
  return RectInstance{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}

std::uint32_t to_u32(std::size_t v) {
  return static_cast<std::uint32_t>(std::min<std::size_t>(v, std::numeric_limits<std::uint32_t>::max()));
}

class BatchBuilder {
 public:
  BatchBuilder(std::int32_t base_x, std::int32_t base_y) {
    out_.polygons.base_x = base_x;
    out_.polygons.base_y = base_y;
  }

  /// Starts a batch layer; false (and `truncated`) when none is left.
  bool begin(const LayerStyle& style, BatchKind kind, std::size_t style_index) {
    if (out_.styles.size() >= kMaxBatchLayers) {
      out_.truncated = true;
      return false;
    }
    layer_ = static_cast<std::uint8_t>(out_.styles.size());
    out_.styles.push_back(style);
    out_.kinds.push_back(kind);
    out_.style_index.push_back(style_index);
    return true;
  }
  bool begin(const display::ResolvedStyle& style, BatchKind kind, bool fill) {
    return begin(to_layer_style(style, fill), kind, style.index);
  }
  void add(const RectInstance& r) { rects_.push_back(LayeredRect{r, layer_}); }
  void add_outline(const db::Rect& r) {
    add(line({r.x0, r.y0}, {r.x1, r.y0}));
    add(line({r.x1, r.y0}, {r.x1, r.y1}));
    add(line({r.x0, r.y1}, {r.x1, r.y1}));
    add(line({r.x0, r.y0}, {r.x0, r.y1}));
  }

  [[nodiscard]] double base_x() const { return out_.polygons.base_x; }
  [[nodiscard]] double base_y() const { return out_.polygons.base_y; }
  /// Vertices of the current layer's polygon batch are appended here.
  [[nodiscard]] std::vector<PolygonVertex>& vertices() { return out_.polygons.vertices; }
  /// Closes the current layer's polygon batch, which starts at vertex `first`.
  void end_polygons(std::size_t first, double min_pixels_per_unit = 0.0) {
    PolygonScene& p = out_.polygons;
    const std::size_t count = p.vertices.size() - first;
    if (count == 0) return;
    p.batches.push_back(LayerBatch{layer_, to_u32(first), to_u32(count)});
    p.min_pixels_per_unit.push_back(min_pixels_per_unit);
  }

  LayoutBatches finish() {
    out_.rects = RectScene::from_layered(rects_);
    return std::move(out_);
  }

 private:
  LayoutBatches out_;
  std::vector<LayeredRect> rects_;
  std::uint8_t layer_ = 0;
};

bool has_fill(display::FillKind fill) {
  return fill == display::FillKind::solid || fill == display::FillKind::stipple;
}

void add_fill(BatchBuilder& b, const display::SceneLayer& layer) {
  if (layer.rects.empty()) return;
  if (has_fill(layer.style.fill) && b.begin(layer.style, BatchKind::fill, true)) {
    for (const auto& r : layer.rects) b.add(RectInstance{r.x0, r.y0, r.x1, r.y1});
  } else if (layer.style.fill == display::FillKind::cross) {
    // grClip.c: a `cross` style draws both diagonals of each rectangle in
    // its line pattern, and no fill.
    LayerStyle style = to_layer_style(layer.style, false);
    style.diagonals = true;
    style.dash = static_cast<std::uint8_t>(layer.style.outline & 0xFF);
    if (b.begin(style, BatchKind::diagonals, layer.style.index)) {
      for (const auto& r : layer.rects) b.add(RectInstance{r.x0, r.y0, r.x1, r.y1});
    }
  }
}

void add_triangles(BatchBuilder& b, const display::SceneLayer& layer) {
  if (layer.triangles.empty() || !has_fill(layer.style.fill) || !b.begin(layer.style, BatchKind::triangles, true)) {
    return;
  }
  const std::size_t first = b.vertices().size();
  for (const auto& t : layer.triangles) {
    append_triangle(t.bounds, t.diagonal, t.right_side, b.base_x(), b.base_y(), b.vertices());
  }
  b.end_polygons(first);
}

void add_layer(BatchBuilder& b, const display::SceneLayer& layer) {
  add_fill(b, layer);
  add_triangles(b, layer);
  if (layer.outline.empty()) return;
  LayerStyle style = to_layer_style(layer.style, false);
  style.dash = static_cast<std::uint8_t>(layer.style.outline & 0xFF);
  if (b.begin(style, BatchKind::outline, layer.style.index)) {
    for (const auto& s : layer.outline) b.add(line(s.a, s.b));
  }
}

/// Lines of one style, grouped so that every label style is one batch layer.
struct LineGroup {
  display::ResolvedStyle style;
  BatchKind kind = BatchKind::label;
  std::vector<RectInstance> lines;
};

LineGroup& group_for(std::vector<LineGroup>& groups, const display::ResolvedStyle& style, BatchKind kind) {
  for (auto& g : groups) {
    if (g.kind == kind && g.style == style) return g;
  }
  groups.push_back(LineGroup{style, kind, {}});
  return groups.back();
}

void add_label_lines(BatchBuilder& b, const std::vector<display::SceneLabel>& labels) {
  std::vector<LineGroup> groups;
  for (const auto& label : labels) {
    auto& outline = group_for(groups, label.style, BatchKind::label).lines;
    const db::Rect& r = label.rect;  // a zero-area rectangle collapses to a dot
    outline.push_back(line({r.x0, r.y0}, {r.x1, r.y0}));
    outline.push_back(line({r.x1, r.y0}, {r.x1, r.y1}));
    outline.push_back(line({r.x0, r.y1}, {r.x1, r.y1}));
    outline.push_back(line({r.x0, r.y0}, {r.x0, r.y1}));
    for (const auto& s : label.port_lines) {
      group_for(groups, label.port_line_style, BatchKind::port_line).lines.push_back(line(s.a, s.b));
    }
  }
  for (const auto& g : groups) {
    if (!b.begin(g.style, g.kind, false)) continue;
    for (const auto& r : g.lines) b.add(r);
  }
}

/// The zoom below which a font label's glyphs are not drawn: its text
/// height ⌊ppu · size / 8⌋ (size in 1/8 units) is 0 (DBWdisplay.c, "Label
/// display") or under 5 pixels (grTOGL3.c, font-labels "Font label
/// rendering"); the attachment rectangle is still drawn.
double glyph_min_pixels_per_unit(std::int32_t size) {
  return size > 0 ? 5.0 * 8.0 / static_cast<double>(size) : std::numeric_limits<double>::infinity();
}

/// Font labels' glyphs, one batch layer per (style, size).
void add_glyphs(BatchBuilder& b, const std::vector<display::SceneLabel>& labels) {
  std::vector<bool> done(labels.size(), false);
  for (std::size_t i = 0; i < labels.size(); ++i) {
    const display::SceneLabel& head = labels[i];
    if (done[i] || !head.font_label || head.outline_polygons.empty()) continue;
    if (!b.begin(head.style, BatchKind::glyphs, false)) return;
    const std::size_t first = b.vertices().size();
    for (std::size_t j = i; j < labels.size(); ++j) {
      const display::SceneLabel& l = labels[j];
      if (done[j] || !l.font_label || l.size != head.size || !(l.style == head.style)) continue;
      done[j] = true;
      (void)fill_nonzero(l.outline_polygons, b.base_x(), b.base_y(), b.vertices());
    }
    b.end_polygons(first, glyph_min_pixels_per_unit(head.size));
  }
}

// --- highlights ---------------------------------------------------------------

/// The batch layers of one highlight run: consecutive items of the same
/// resolved style share a fill, a line and a triangle layer (in that order).
struct HighlightRun {
  std::vector<RectInstance> fills;
  std::vector<RectInstance> lines;
  std::vector<const display::HighlightItem*> triangles;
};

void add_highlight_item(HighlightRun& run, const display::HighlightItem& it) {
  const db::Rect& r = it.rect;
  const bool has_outline = it.resolved.outline != 0;
  switch (it.shape) {
    case display::HighlightShape::box:
      if (has_fill(it.resolved.fill)) run.fills.push_back(RectInstance{r.x0, r.y0, r.x1, r.y1});
      if (has_outline || !has_fill(it.resolved.fill)) {
        for (const auto& s : display::boundary_segments({r})) run.lines.push_back(line(s.a, s.b));
      }
      break;
    case display::HighlightShape::outline:
      run.lines.push_back(line({r.x0, r.y0}, {r.x1, r.y0}));
      run.lines.push_back(line({r.x1, r.y0}, {r.x1, r.y1}));
      run.lines.push_back(line({r.x0, r.y1}, {r.x1, r.y1}));
      run.lines.push_back(line({r.x0, r.y0}, {r.x0, r.y1}));
      break;
    case display::HighlightShape::line:
      run.lines.push_back(line(it.line.a, it.line.b));
      break;
    case display::HighlightShape::cross:
      run.lines.push_back(RectInstance{r.x0, r.y0, r.x0, r.y0});
      break;
    case display::HighlightShape::triangle:
      run.triangles.push_back(&it);
      break;
    case display::HighlightShape::text:
      break;
  }
}

void emit_highlight_run(BatchBuilder& b, const display::ResolvedStyle& style, const HighlightRun& run) {
  if (!run.fills.empty() && b.begin(style, BatchKind::highlight, true)) {
    for (const auto& r : run.fills) b.add(r);
  }
  if (!run.triangles.empty() && b.begin(style, BatchKind::highlight, true)) {
    const std::size_t first = b.vertices().size();
    for (const auto* t : run.triangles) {
      append_triangle(t->rect, t->diagonal, t->right_side, b.base_x(), b.base_y(), b.vertices());
    }
    b.end_polygons(first);
  }
  if (!run.lines.empty()) {
    LayerStyle ls = to_layer_style(style, false);
    ls.dash = static_cast<std::uint8_t>(style.outline & 0xFF);
    if (b.begin(ls, BatchKind::highlight, style.index)) {
      for (const auto& r : run.lines) b.add(r);
    }
  }
}

void add_highlights(BatchBuilder& b, const display::HighlightScene& scene) {
  std::size_t i = 0;
  while (i < scene.items.size()) {
    const display::ResolvedStyle& style = scene.items[i].resolved;
    HighlightRun run;
    for (; i < scene.items.size() && scene.items[i].resolved == style; ++i) {
      add_highlight_item(run, scene.items[i]);
    }
    emit_highlight_run(b, style, run);
  }
}

db::Point highlight_base(const display::HighlightScene& scene) {
  for (const auto& it : scene.items) {
    if (it.shape == display::HighlightShape::triangle) return {it.rect.x0, it.rect.y0};
  }
  return {0, 0};
}

}  // namespace

LayerStyle to_layer_style(const display::ResolvedStyle& style, bool fill) {
  // Q-UI-35: translucent fills use Magic's OpenGL color c' = 2c - 0.8,
  // clamped as GL clamps it, with alpha = mask / 127.
  const display::ResolvedStyle c = fill ? display::opengl_supercolor(style) : style;
  auto clamp = [](float v) { return std::clamp(v, 0.0F, 1.0F); };
  LayerStyle out{clamp(c.r), clamp(c.g), clamp(c.b), c.a, false, {}};
  if (fill && style.fill == display::FillKind::stipple) {
    out.stippled = true;
    out.stipple = style.stipple;
  }
  if (!fill) {
    out.a = 1.0F;  // outlines and text-related lines are opaque
  }
  return out;
}

LayoutBatches to_batches(const display::LayoutScene& scene, const display::HighlightScene* highlights) {
  const db::Rect& bounds = scene.bounds;
  db::Point base{bounds.is_null() ? 0 : bounds.x0, bounds.is_null() ? 0 : bounds.y0};
  if (bounds.is_null() && highlights != nullptr) {
    base = highlight_base(*highlights);
  }
  BatchBuilder b(base.x, base.y);
  for (const auto& layer : scene.layers) {
    add_layer(b, layer);
  }
  add_label_lines(b, scene.labels);
  add_glyphs(b, scene.labels);
  // One batch layer per style for all cell boxes.
  if (!scene.cells.empty() && b.begin(scene.cells.front().style, BatchKind::cell_box, false)) {
    for (const auto& cell : scene.cells) b.add_outline(cell.bbox);
  }
  if (highlights != nullptr) {
    add_highlights(b, *highlights);
  }
  return b.finish();
}

LayoutBatches highlight_batches(const display::HighlightScene& highlights) {
  const db::Point base = highlight_base(highlights);
  BatchBuilder b(base.x, base.y);
  add_highlights(b, highlights);
  return b.finish();
}

}  // namespace hefesto::render
