// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/layout_scene.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace hefesto::render {

namespace {

constexpr std::size_t kMaxBatchLayers = std::numeric_limits<std::uint8_t>::max() + 1U;

RectInstance line(const db::Point& a, const db::Point& b) {
  return RectInstance{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}

class BatchBuilder {
 public:
  /// Starts a batch layer; false (and `truncated`) when none is left.
  bool begin(const display::ResolvedStyle& style, BatchKind kind, bool fill) {
    if (out_.styles.size() >= kMaxBatchLayers) {
      out_.truncated = true;
      return false;
    }
    layer_ = static_cast<std::uint8_t>(out_.styles.size());
    out_.styles.push_back(to_layer_style(style, fill));
    out_.kinds.push_back(kind);
    out_.style_index.push_back(style.index);
    return true;
  }
  void add(const RectInstance& r) { rects_.push_back(LayeredRect{r, layer_}); }
  void add_outline(const db::Rect& r) {
    add(line({r.x0, r.y0}, {r.x1, r.y0}));
    add(line({r.x1, r.y0}, {r.x1, r.y1}));
    add(line({r.x0, r.y1}, {r.x1, r.y1}));
    add(line({r.x0, r.y0}, {r.x0, r.y1}));
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

void add_layer(BatchBuilder& b, const display::SceneLayer& layer) {
  if (has_fill(layer.style.fill) && !layer.rects.empty() && b.begin(layer.style, BatchKind::fill, true)) {
    for (const auto& r : layer.rects) {
      b.add(RectInstance{r.x0, r.y0, r.x1, r.y1});
    }
  }
  if (!layer.outline.empty() && b.begin(layer.style, BatchKind::outline, false)) {
    for (const auto& s : layer.outline) {
      b.add(line(s.a, s.b));
    }
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
    if (g.kind == kind && g.style == style) {
      return g;
    }
  }
  groups.push_back(LineGroup{style, kind, {}});
  return groups.back();
}

void add_labels(BatchBuilder& b, const std::vector<display::SceneLabel>& labels) {
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
    if (b.begin(g.style, g.kind, false)) {
      for (const auto& r : g.lines) {
        b.add(r);
      }
    }
  }
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

LayoutBatches to_batches(const display::LayoutScene& scene) {
  BatchBuilder b;
  for (const auto& layer : scene.layers) {
    add_layer(b, layer);
  }
  add_labels(b, scene.labels);
  // One batch layer per style for all cell boxes.
  if (!scene.cells.empty() && b.begin(scene.cells.front().style, BatchKind::cell_box, false)) {
    for (const auto& cell : scene.cells) {
      b.add_outline(cell.bbox);
    }
  }
  return b.finish();
}

}  // namespace hefesto::render
