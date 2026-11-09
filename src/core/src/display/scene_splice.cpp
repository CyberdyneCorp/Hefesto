// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Partial scene rebuilds (capability ui-windows-and-views "Change-driven
// redisplay"): a retained LayoutScene takes the content of one changed area
// from a scene built for that area only, the way Magic redraws only the
// areas DBWAreaChanged recorded.
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>

#include "hefesto/display/scene.hpp"

namespace hefesto::display {

namespace {

/// The parts of `r` outside `cut` (half-open areas; empty parts dropped).
void subtract(const db::Rect& r, const db::Rect& cut, std::vector<db::Rect>& out) {
  if (!db::overlap(r, cut)) {
    out.push_back(r);
    return;
  }
  const db::Coord y0 = std::max(r.y0, cut.y0);
  const db::Coord y1 = std::min(r.y1, cut.y1);
  const db::Rect parts[4] = {
      {r.x0, r.y0, r.x1, cut.y0},  // below
      {r.x0, cut.y1, r.x1, r.y1},  // above
      {r.x0, y0, cut.x0, y1},      // left, within the cut's rows
      {cut.x1, y0, r.x1, y1},      // right
  };
  for (const db::Rect& p : parts) {
    if (!p.is_null()) {
      out.push_back(p);
    }
  }
}

/// A horizontal or vertical segment as an interval [lo, hi] at `at`.
struct Span {
  bool horizontal = true;
  db::Coord at = 0;
  db::Coord lo = 0;
  db::Coord hi = 0;

  [[nodiscard]] SceneSegment segment(db::Coord a, db::Coord b) const {
    return horizontal ? SceneSegment{{a, at}, {b, at}} : SceneSegment{{at, a}, {at, b}};
  }
};

Span span_of(const SceneSegment& s) {
  if (s.a.y == s.b.y) {
    return Span{true, s.a.y, std::min(s.a.x, s.b.x), std::max(s.a.x, s.b.x)};
  }
  return Span{false, s.a.x, std::min(s.a.y, s.b.y), std::max(s.a.y, s.b.y)};
}

/// Splits a segment by the closed rectangle `cut`: the part inside goes to
/// `inside` (when given), the parts outside to `outside` (when given).
void split_segment(const SceneSegment& seg, const db::Rect& cut, std::vector<SceneSegment>* inside,
                   std::vector<SceneSegment>* outside) {
  const Span s = span_of(seg);
  const db::Coord across_lo = s.horizontal ? cut.y0 : cut.x0;
  const db::Coord across_hi = s.horizontal ? cut.y1 : cut.x1;
  const db::Coord along_lo = s.horizontal ? cut.x0 : cut.y0;
  const db::Coord along_hi = s.horizontal ? cut.x1 : cut.y1;
  const bool crosses = s.at >= across_lo && s.at <= across_hi && s.lo <= along_hi && s.hi >= along_lo;
  if (!crosses) {
    if (outside != nullptr) {
      outside->push_back(seg);
    }
    return;
  }
  const db::Coord in_lo = std::max(s.lo, along_lo);
  const db::Coord in_hi = std::min(s.hi, along_hi);
  if (inside != nullptr && in_lo < in_hi) {
    inside->push_back(s.segment(in_lo, in_hi));
  }
  if (outside != nullptr && s.lo < along_lo) {
    outside->push_back(s.segment(s.lo, along_lo));
  }
  if (outside != nullptr && s.hi > along_hi) {
    outside->push_back(s.segment(along_hi, s.hi));
  }
}

db::Rect clipped(const db::Rect& r, const db::Rect& to) {
  return db::Rect{std::max(r.x0, to.x0), std::max(r.y0, to.y0), std::min(r.x1, to.x1), std::min(r.y1, to.y1)};
}

/// The layer outside `area` from `old` plus the layer inside from `fresh`
/// (either may be absent).
SceneLayer merge_layer(const SceneLayer* old, const SceneLayer* fresh, const db::Rect& area) {
  SceneLayer out = old != nullptr ? SceneLayer{old->style, old->pale, {}, {}, {}, old->order}
                                  : SceneLayer{fresh->style, fresh->pale, {}, {}, {}, fresh->order};
  if (old != nullptr) {
    for (const db::Rect& r : old->rects) {
      subtract(r, area, out.rects);
    }
    for (const SceneSegment& s : old->outline) {
      split_segment(s, area, nullptr, &out.outline);
    }
    std::copy_if(old->triangles.begin(), old->triangles.end(), std::back_inserter(out.triangles),
                 [&area](const SceneTriangle& t) { return !db::touch(t.bounds, area); });
  }
  if (fresh != nullptr) {
    for (const db::Rect& r : fresh->rects) {
      const db::Rect c = clipped(r, area);
      if (!c.is_null()) {
        out.rects.push_back(c);
      }
    }
    for (const SceneSegment& s : fresh->outline) {
      split_segment(s, area, &out.outline, nullptr);
    }
    std::copy_if(fresh->triangles.begin(), fresh->triangles.end(), std::back_inserter(out.triangles),
                 [&area](const SceneTriangle& t) { return db::touch(t.bounds, area); });
  }
  return out;
}

/// A `cross` fill draws diagonals per rectangle, so cutting one would draw
/// different diagonals than a full rebuild.
bool cuts_cross_fill(const std::vector<SceneLayer>& layers, const db::Rect& area) {
  return std::any_of(layers.begin(), layers.end(), [&area](const SceneLayer& l) {
    return l.style.fill == FillKind::cross &&
           std::any_of(l.rects.begin(), l.rects.end(), [&area](const db::Rect& r) { return db::touch(r, area); });
  });
}

/// The builder's bounds rule: every drawn rectangle, triangle, label (at
/// least one unit) and font-label text box, and every instance box.
db::Rect scene_bounds(const LayoutScene& scene) {
  db::Rect b;
  for (const SceneLayer& l : scene.layers) {
    for (const db::Rect& r : l.rects) {
      b = db::include(b, r);
    }
    for (const SceneTriangle& t : l.triangles) {
      b = db::include(b, t.bounds);
    }
  }
  for (const SceneLabel& l : scene.labels) {
    if (l.font_label) {
      b = db::include(b, l.font_bbox);
    }
    b = db::include(b, db::Rect{l.rect.x0, l.rect.y0, std::max(l.rect.x1, l.rect.x0 + 1),
                                std::max(l.rect.y1, l.rect.y0 + 1)});
  }
  for (const SceneCell& c : scene.cells) {
    b = db::include(b, c.bbox);
  }
  return b;
}

}  // namespace

bool label_in_display_area(const SceneLabel& label, const db::Rect& area) noexcept {
  return db::touch(label.rect, area) || (label.font_label && db::touch(label.font_bbox, area));
}

bool splice_scene(LayoutScene& scene, LayoutScene partial, const db::Rect& area) {
  if (cuts_cross_fill(scene.layers, area) || cuts_cross_fill(partial.layers, area)) {
    return false;
  }
  std::map<std::size_t, std::pair<const SceneLayer*, const SceneLayer*>> by_order;
  for (const SceneLayer& l : scene.layers) {
    by_order[l.order].first = &l;
  }
  for (const SceneLayer& l : partial.layers) {
    by_order[l.order].second = &l;
  }
  std::vector<SceneLayer> layers;
  for (const auto& [order, pair] : by_order) {
    SceneLayer merged = merge_layer(pair.first, pair.second, area);
    if (!merged.rects.empty() || !merged.triangles.empty()) {
      layers.push_back(std::move(merged));
    }
  }
  scene.layers = std::move(layers);
  std::erase_if(scene.labels, [&area](const SceneLabel& l) { return label_in_display_area(l, area); });
  std::move(partial.labels.begin(), partial.labels.end(), std::back_inserter(scene.labels));
  std::erase_if(scene.cells, [&area](const SceneCell& c) { return db::touch(c.bbox, area); });
  std::copy_if(std::make_move_iterator(partial.cells.begin()), std::make_move_iterator(partial.cells.end()),
               std::back_inserter(scene.cells), [&area](const SceneCell& c) { return db::touch(c.bbox, area); });
  scene.bounds = scene_bounds(scene);
  return true;
}

}  // namespace hefesto::display
