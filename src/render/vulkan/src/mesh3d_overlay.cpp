// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// overlay_faces (mesh3d_renderer.hpp): the faces of the net overlay
// (DrawParams::net_overlay, task 10.4). Vulkan-free: the renderer calls it
// once per overlay change, tests call it directly.
//
// Each z range's prisms are swept left to right over the distinct x of
// their edges: the active prisms' y intervals (kept sorted) are merged once
// per slab, so the work is the number of slabs times the active intervals,
// never all prisms per slab. At most kMaxOverlayPrisms prisms are read and
// kMaxOverlayFaces faces emitted (the standing crafted-input limit rule of
// 2026-11-25); OverlayFaces::truncated says when a limit cut the overlay.
//
// Owner: RENDER3D (round 6).

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include "hefesto/render/mesh3d_renderer.hpp"

namespace hefesto::render {

namespace {

using Intervals = std::vector<std::pair<float, float>>;

/// One z range's prisms as y intervals per column of x (`ys` sorted,
/// merged, abutting ones joined); consecutive slabs with the same intervals
/// share a column.
struct Column {
  float x0 = 0.0F;
  float x1 = 0.0F;
  Intervals ys;
};

/// What the face pass may still spend: interval visits of the sweep and
/// faces emitted. Running out of either stops the pass (`truncated`); what
/// was emitted before stays.
struct Budget {
  std::size_t work = kMaxOverlayWork;
  std::size_t faces = kMaxOverlayFaces;
  bool truncated = false;

  bool spend_work(std::size_t n) {
    if (n > work) {
      truncated = true;
      return false;
    }
    work -= n;
    return true;
  }

  bool spend_faces(std::size_t n) {
    if (n > faces) {
      truncated = true;
      return false;
    }
    faces -= n;
    return true;
  }
};

/// The union of the sorted intervals of `active`, abutting ones joined.
Intervals merged(const std::multiset<std::pair<float, float>>& active) {
  Intervals out;
  for (const auto& [lo, hi] : active) {
    if (!out.empty() && lo <= out.back().second) {
      out.back().second = std::max(out.back().second, hi);
    } else {
      out.emplace_back(lo, hi);
    }
  }
  return out;
}

/// The pieces of [lo, hi) not covered by `b` (sorted, merged).
void subtract(float lo, float hi, const Intervals& b, Intervals& out) {
  float cur = lo;
  for (const auto& [blo, bhi] : b) {
    if (bhi <= cur || blo >= hi) {
      continue;
    }
    if (blo > cur) {
      out.emplace_back(cur, blo);
    }
    cur = std::max(cur, bhi);
  }
  if (cur < hi) {
    out.emplace_back(cur, hi);
  }
}

Intervals minus(const Intervals& a, const Intervals& b) {
  Intervals out;
  for (const auto& [lo, hi] : a) subtract(lo, hi, b, out);
  return out;
}

/// The sweep's events of one z range: the prisms sorted by their left and
/// by their right edge.
struct Sweep {
  const std::vector<OverlayPrism>* group;
  std::vector<std::size_t> by_x0;
  std::vector<std::size_t> by_x1;
  std::size_t next_open = 0;
  std::size_t next_close = 0;
  std::multiset<std::pair<float, float>> active;

  explicit Sweep(const std::vector<OverlayPrism>& g) : group(&g), by_x0(g.size()), by_x1(g.size()) {
    for (std::size_t i = 0; i < g.size(); ++i) by_x0[i] = by_x1[i] = i;
    std::sort(by_x0.begin(), by_x0.end(), [&g](std::size_t a, std::size_t b) { return g[a].x0 < g[b].x0; });
    std::sort(by_x1.begin(), by_x1.end(), [&g](std::size_t a, std::size_t b) { return g[a].x1 < g[b].x1; });
  }

  /// Makes `active` the prisms covering the slab that starts at `x`: those
  /// that end at or before `x` leave, those that start there come in.
  void advance(float x) {
    const std::vector<OverlayPrism>& g = *group;
    for (; next_close < by_x1.size() && g[by_x1[next_close]].x1 <= x; ++next_close) {
      const OverlayPrism& p = g[by_x1[next_close]];
      active.erase(active.find({p.y0, p.y1}));
    }
    for (; next_open < by_x0.size() && g[by_x0[next_open]].x0 <= x; ++next_open) {
      const OverlayPrism& p = g[by_x0[next_open]];
      active.emplace(p.y0, p.y1);
    }
  }
};

/// The distinct x of the group's edges, sorted.
std::vector<float> edges_x(const std::vector<OverlayPrism>& group) {
  std::vector<float> xs;
  xs.reserve(group.size() * 2U);
  for (const OverlayPrism& p : group) {
    xs.push_back(p.x0);
    xs.push_back(p.x1);
  }
  std::sort(xs.begin(), xs.end());
  xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
  return xs;
}

std::vector<Column> columns_of(const std::vector<OverlayPrism>& group, Budget& budget) {
  const std::vector<float> xs = edges_x(group);
  Sweep sweep(group);
  std::vector<Column> out;
  for (std::size_t k = 0; k + 1 < xs.size(); ++k) {
    sweep.advance(xs[k]);
    if (!budget.spend_work(sweep.active.size() + 1U)) {
      break;
    }
    Intervals ys = merged(sweep.active);
    if (!out.empty() && out.back().x1 == xs[k] && out.back().ys == ys) {
      out.back().x1 = xs[k + 1];
    } else if (!ys.empty()) {
      out.push_back(Column{xs[k], xs[k + 1], std::move(ys)});
    }
  }
  return out;
}

/// The boundary sides of `col` on its -x and +x edges: the parts of its
/// intervals not continued by the column that abuts it there (none when
/// the neighbour is not adjacent). False when the budget ran out.
bool emit_x_sides(const Column& col, const Column* left, const Column* right, float zb, float zt, Budget& budget,
                  OverlayFaces& out) {
  static const Intervals kNone;
  const Intervals open_left = minus(col.ys, left != nullptr && left->x1 == col.x0 ? left->ys : kNone);
  const Intervals open_right = minus(col.ys, right != nullptr && right->x0 == col.x1 ? right->ys : kNone);
  if (!budget.spend_faces(open_left.size() + open_right.size())) {
    return false;
  }
  for (const auto& [lo, hi] : open_left) {
    out.sides.push_back(OverlayFaces::Side{col.x0, hi, col.x0, lo, zb, zt});  // -x
  }
  for (const auto& [lo, hi] : open_right) {
    out.sides.push_back(OverlayFaces::Side{col.x1, lo, col.x1, hi, zb, zt});  // +x
  }
  return true;
}

/// The tops and boundary side faces of one z range's columns, each side
/// with the outside on the right of (x0, y0) -> (x1, y1).
void emit_columns(const std::vector<Column>& cols, float zb, float zt, Budget& budget, OverlayFaces& out) {
  for (std::size_t c = 0; c < cols.size(); ++c) {
    const Column& col = cols[c];
    if (!budget.spend_faces(col.ys.size() * 3U)) {  // each interval: a top, its -y and +y sides
      return;
    }
    for (const auto& [lo, hi] : col.ys) {
      out.tops.push_back(OverlayPrism{col.x0, lo, col.x1, hi, zb, zt});
      out.sides.push_back(OverlayFaces::Side{col.x0, lo, col.x1, lo, zb, zt});  // -y
      out.sides.push_back(OverlayFaces::Side{col.x1, hi, col.x0, hi, zb, zt});  // +y
    }
    const Column* left = c > 0 ? &cols[c - 1] : nullptr;
    const Column* right = c + 1 < cols.size() ? &cols[c + 1] : nullptr;
    if (!emit_x_sides(col, left, right, zb, zt, budget, out)) {
      return;
    }
  }
}

/// A prism with its corners and z range ordered; nullopt when a field is
/// not finite or its rectangle is empty.
std::optional<OverlayPrism> normalized(const OverlayPrism& o) {
  const bool finite = std::isfinite(o.x0) && std::isfinite(o.y0) && std::isfinite(o.x1) && std::isfinite(o.y1) &&
                      std::isfinite(o.z_bottom) && std::isfinite(o.z_top);
  if (!finite || o.x0 == o.x1 || o.y0 == o.y1) {
    return std::nullopt;
  }
  return OverlayPrism{std::min(o.x0, o.x1), std::min(o.y0, o.y1), std::max(o.x0, o.x1),
                      std::max(o.y0, o.y1), std::min(o.z_bottom, o.z_top), std::max(o.z_bottom, o.z_top)};
}

}  // namespace

OverlayFaces overlay_faces(std::span<const OverlayPrism> prisms) {
  OverlayFaces out;
  if (prisms.size() > kMaxOverlayPrisms) {
    prisms = prisms.first(kMaxOverlayPrisms);
    out.truncated = true;
  }
  std::map<std::pair<float, float>, std::vector<OverlayPrism>> groups;  // by z range
  for (const OverlayPrism& o : prisms) {
    if (const auto n = normalized(o)) groups[{n->z_bottom, n->z_top}].push_back(*n);
  }
  Budget budget;
  for (const auto& [z, group] : groups) {
    emit_columns(columns_of(group, budget), z.first, z.second, budget, out);
  }
  out.truncated = out.truncated || budget.truncated;
  return out;
}

}  // namespace hefesto::render
