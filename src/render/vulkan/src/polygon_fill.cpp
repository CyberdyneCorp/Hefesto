// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/polygon_fill.hpp"

#include <algorithm>
#include <cmath>

namespace hefesto::render {

namespace {

/// A non-horizontal edge with y0 < y1; `dir` is +1 when the contour runs upward.
struct Edge {
  double x0 = 0.0;
  double y0 = 0.0;
  double x1 = 0.0;
  double y1 = 0.0;
  int dir = 0;

  [[nodiscard]] double x_at(double y) const { return x0 + (x1 - x0) * (y - y0) / (y1 - y0); }
};

bool finite(const display::PointF& p) {
  return std::isfinite(p.x) && std::isfinite(p.y);
}

void add_contour(const std::vector<display::PointF>& contour, std::vector<Edge>& edges) {
  const std::size_t n = contour.size();
  if (n < 3) return;
  for (std::size_t i = 0; i < n; ++i) {
    const display::PointF& a = contour[i];
    const display::PointF& b = contour[(i + 1) % n];
    if (!finite(a) || !finite(b) || a.y == b.y) continue;
    edges.push_back(a.y < b.y ? Edge{a.x, a.y, b.x, b.y, 1} : Edge{b.x, b.y, a.x, a.y, -1});
  }
}

/// Every height where the fill can change: edge ends and edge crossings.
std::vector<double> event_heights(const std::vector<Edge>& edges) {
  std::vector<double> ys;
  ys.reserve(edges.size() * 2);
  for (const Edge& e : edges) {
    ys.push_back(e.y0);
    ys.push_back(e.y1);
  }
  for (std::size_t i = 0; i < edges.size(); ++i) {
    for (std::size_t j = i + 1; j < edges.size(); ++j) {
      const Edge& a = edges[i];
      const Edge& b = edges[j];
      const double lo = std::max(a.y0, b.y0);
      const double hi = std::min(a.y1, b.y1);
      if (lo >= hi) continue;
      // The x distance between the edges is linear in y: a sign change means a crossing.
      const double d_lo = a.x_at(lo) - b.x_at(lo);
      const double d_hi = a.x_at(hi) - b.x_at(hi);
      if ((d_lo < 0.0 && d_hi > 0.0) || (d_lo > 0.0 && d_hi < 0.0)) {
        ys.push_back(lo + (hi - lo) * d_lo / (d_lo - d_hi));
      }
    }
  }
  std::sort(ys.begin(), ys.end());
  ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
  return ys;
}

struct Crossing {
  double mid = 0.0;
  double lo = 0.0;  // x at the slab's bottom
  double hi = 0.0;  // x at the slab's top
  int dir = 0;
};

class TrapezoidSink {
 public:
  TrapezoidSink(double base_x, double base_y, std::vector<PolygonVertex>& out) : bx_(base_x), by_(base_y), out_(out) {}

  void add(const Crossing& left, const Crossing& right, double y_lo, double y_hi) {
    const PolygonVertex a = vertex(left.lo, y_lo);
    const PolygonVertex b = vertex(right.lo, y_lo);
    const PolygonVertex c = vertex(right.hi, y_hi);
    const PolygonVertex d = vertex(left.hi, y_hi);
    out_.insert(out_.end(), {a, b, c, a, c, d});
  }

 private:
  [[nodiscard]] PolygonVertex vertex(double x, double y) const {
    return PolygonVertex{static_cast<float>(x - bx_), static_cast<float>(y - by_)};
  }

  double bx_;
  double by_;
  std::vector<PolygonVertex>& out_;
};

/// The nonzero spans of one slab, emitted as trapezoids.
void fill_slab(const std::vector<const Edge*>& active, double y_lo, double y_hi, TrapezoidSink& sink,
               std::vector<Crossing>& crossings) {
  crossings.clear();
  const double mid = 0.5 * (y_lo + y_hi);
  for (const Edge* e : active) {
    crossings.push_back(Crossing{e->x_at(mid), e->x_at(y_lo), e->x_at(y_hi), e->dir});
  }
  std::sort(crossings.begin(), crossings.end(), [](const Crossing& a, const Crossing& b) { return a.mid < b.mid; });
  int winding = 0;
  std::size_t start = 0;
  for (std::size_t k = 0; k < crossings.size(); ++k) {
    const int before = winding;
    winding += crossings[k].dir;
    if (before == 0 && winding != 0) {
      start = k;
    } else if (before != 0 && winding == 0) {
      sink.add(crossings[start], crossings[k], y_lo, y_hi);
    }
  }
}

}  // namespace

bool fill_nonzero(const std::vector<std::vector<display::PointF>>& contours, double base_x, double base_y,
                  std::vector<PolygonVertex>& out) {
  std::vector<Edge> edges;
  for (const auto& contour : contours) {
    if (edges.size() > kMaxFillEdges) return false;
    add_contour(contour, edges);
  }
  if (edges.size() > kMaxFillEdges) return false;
  std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) { return a.y0 < b.y0; });
  const std::vector<double> ys = event_heights(edges);
  TrapezoidSink sink(base_x, base_y, out);
  std::vector<const Edge*> active;
  std::vector<Crossing> crossings;
  std::size_t next = 0;
  for (std::size_t i = 0; i + 1 < ys.size(); ++i) {
    const double y_lo = ys[i];
    const double y_hi = ys[i + 1];
    while (next < edges.size() && edges[next].y0 <= y_lo) active.push_back(&edges[next++]);
    std::erase_if(active, [y_lo](const Edge* e) { return e->y1 <= y_lo; });
    fill_slab(active, y_lo, y_hi, sink, crossings);
  }
  return true;
}

void append_triangle(const db::Rect& bounds, db::Diagonal diagonal, bool right_side, double base_x, double base_y,
                     std::vector<PolygonVertex>& out) {
  const auto v = [base_x, base_y](db::Coord x, db::Coord y) {
    return PolygonVertex{static_cast<float>(static_cast<double>(x) - base_x),
                         static_cast<float>(static_cast<double>(y) - base_y)};
  };
  const PolygonVertex ll = v(bounds.x0, bounds.y0);
  const PolygonVertex lr = v(bounds.x1, bounds.y0);
  const PolygonVertex ul = v(bounds.x0, bounds.y1);
  const PolygonVertex ur = v(bounds.x1, bounds.y1);
  if (diagonal == db::Diagonal::nw_se) {  // `\` from ul to lr
    out.insert(out.end(), {right_side ? ur : ll, ul, lr});
  } else {  // `/` from ll to ur
    out.insert(out.end(), {right_side ? lr : ul, ll, ur});
  }
}

}  // namespace hefesto::render
