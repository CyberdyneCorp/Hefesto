// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The polygon strip decomposer shared by the `polygon` command and CIF/GDS
// polygon input (cmd-paint-erase "Polygon command", Q-CMDEDIT-19;
// mask-input-styles). Ported from Magic VLSI 8.3.694 (fc5daa9e)
// cif/CIFrdutils.c (CIFMakeManhattanPath, CIFEdgeDirection, is_clockwise,
// path_intersect, seg_intersect, test_insideness) and cif/CIFrdpoly.c
// (CIFPolyToRects, cifOrient, cifCross), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md). Magic's linked
// CIFPath becomes an index-linked node pool; the pieces come out in the
// order Magic paints them (the triangles split off while the path is made
// Manhattan, then the strips in reverse order of discovery). Owner: EDIT.

#include "hefesto/db/polygon.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/diag_codes.hpp"

namespace hefesto::db {

namespace {

constexpr int kEnd = -1;

// Magic's CIFPath as a pool of nodes linked by index.
class Path {
 public:
  explicit Path(std::span<const Point> vertices) {
    for (const Point& p : vertices) {
      nodes_.push_back(Node{p, kEnd});
      if (nodes_.size() > 1) {
        nodes_[nodes_.size() - 2].next = static_cast<int>(nodes_.size() - 1);
      }
    }
  }

  [[nodiscard]] int head() const noexcept { return nodes_.empty() ? kEnd : 0; }
  [[nodiscard]] int next(int i) const { return nodes_.at(static_cast<std::size_t>(i)).next; }
  [[nodiscard]] const Point& at(int i) const { return nodes_.at(static_cast<std::size_t>(i)).p; }
  Point& at(int i) { return nodes_.at(static_cast<std::size_t>(i)).p; }

  /// Inserts `p` after node `i`; returns the new node.
  int insert_after(int i, Point p) {
    nodes_.push_back(Node{p, next(i)});
    const int added = static_cast<int>(nodes_.size() - 1);
    nodes_.at(static_cast<std::size_t>(i)).next = added;
    return added;
  }

  /// Closes the path: appends the first point if the last differs.
  void close() {
    int tail = head();
    while (next(tail) != kEnd) {
      tail = next(tail);
    }
    if (at(tail) != at(head())) {
      insert_after(tail, at(head()));
    }
  }

 private:
  struct Node {
    Point p;
    int next = kEnd;
  };
  std::vector<Node> nodes_;
};

std::int64_t wide(Coord v) noexcept {
  return v;
}

// is_clockwise: the side of the leftmost point's successor.
bool is_clockwise(const Path& path) {
  int midx = kEnd;
  int last = kEnd;
  std::int64_t minx = kInfinity;
  const auto find_min = [&](int from) {
    for (int p = from; path.next(p) != kEnd; p = path.next(p)) {
      if (path.at(path.next(p)).x < minx) {
        minx = path.at(path.next(p)).x;
        midx = path.next(p);
        last = p;
      }
    }
  };
  find_min(path.head());
  if (midx == kEnd) {
    return true;
  }
  if (path.at(last).x == path.at(midx).x) {
    int p = path.head();
    while (p != kEnd && path.at(p).x == minx) {
      p = path.next(p);
    }
    if (p == kEnd) {
      return true;
    }
    minx = kInfinity;
    find_min(p);
  }
  if (path.next(midx) == kEnd) {
    midx = path.head();
  }
  const Point p1 = path.at(last);
  const Point p2 = path.at(midx);
  const Point p3 = path.at(path.next(midx));
  const std::int64_t sdir = (wide(p2.x) - p1.x) * (wide(p3.y) - p1.y) - (wide(p2.y) - p1.y) * (wide(p3.x) - p1.x);
  return sdir < 0;
}

enum class EdgeDir : std::uint8_t { ur, dr, ul, dl, other };

EdgeDir edge_direction(Point first, Point last) noexcept {
  if (first.x < last.x) {
    return first.y < last.y ? EdgeDir::ur : first.y > last.y ? EdgeDir::dr : EdgeDir::other;
  }
  if (first.x > last.x) {
    return first.y < last.y ? EdgeDir::ul : first.y > last.y ? EdgeDir::dl : EdgeDir::other;
  }
  return EdgeDir::other;
}

Rect canonical_of(Point a, Point b) noexcept {
  return canonical(Rect{a.x, a.y, b.x, b.y});
}

// test_insideness: `t` strictly inside the triangle's bounding rectangle.
bool test_insideness(const Path& path, int start, Point t) {
  const Rect r = canonical_of(path.at(start), path.at(path.next(path.next(start))));
  return t.x > r.x0 && t.x < r.x1 && t.y > r.y0 && t.y < r.y1;
}

Coord interpolate(Coord base, std::int64_t num_a, std::int64_t num_b, std::int64_t den) {
  return static_cast<Coord>(wide(base) + num_a * num_b / den);
}

bool strictly_between(Coord v, Coord a, Coord b) noexcept {
  return (v > a && v < b) || (v < a && v > b);
}

// seg_intersect: where segment bf-bs crosses the first (Manhattan) leg of the
// triangle at `start`.
bool seg_intersect(const Path& path, int start, Point bf, Point bs, Point& out) {
  const Point a0 = path.at(start);
  const Point a1 = path.at(path.next(start));
  const Point a2 = path.at(path.next(path.next(start)));
  if (a0.x == a1.x) {
    const Coord adx = a0.x + (a2.x > a0.x ? 1 : -1);
    if ((bf.x > adx && bs.x > adx) || (bf.x < adx && bs.x < adx) || bs.x == bf.x) {
      return false;
    }
    out = Point{a0.x, interpolate(bf.y, wide(bs.y) - bf.y, wide(a0.x) - bf.x, wide(bs.x) - bf.x)};
    return strictly_between(out.y, a0.y, a1.y);
  }
  const Coord ady = a0.y + (a2.y > a0.y ? 1 : -1);
  if ((bf.y > ady && bs.y > ady) || (bf.y < ady && bs.y < ady) || bs.y == bf.y) {
    return false;
  }
  out = Point{interpolate(bf.x, wide(bs.x) - bf.x, wide(a0.y) - bf.y, wide(bs.y) - bf.y), a0.y};
  return strictly_between(out.x, a0.x, a1.x);
}

// path_intersect's loop state; `newdist` and `diagonal` persist across
// edges, as in Magic.
struct CrossState {
  std::int64_t tdist = kInfinity;
  std::int64_t newdist = kInfinity;
  bool diagonal = false;
  bool crosses = false;
  Point tmp;
  Point best;
  int crossed = kEnd;
};

// The insideness test of path_intersect for edge `p`: a Manhattan edge
// whose start lies inside the triangle limits it. Returns false when the
// edge must be skipped.
bool inside_limit(const Path& path, int start, int p, CrossState& c) {
  const Point e0 = path.at(p);
  const Point e1 = path.at(path.next(p));
  const std::int64_t tmpdist = std::abs(c.newdist);
  if (e0.x != e1.x && e0.y != e1.y) {
    return c.diagonal;
  }
  const bool vertical = path.at(start).x == path.at(path.next(start)).x;
  c.newdist = vertical ? wide(e0.y) - path.at(start).y : wide(e0.x) - path.at(start).x;
  if (std::abs(c.newdist) < tmpdist) {
    c.tmp = vertical ? Point{path.at(start).x, e0.y} : Point{e0.x, path.at(start).y};
    c.diagonal = false;
  }
  return true;
}

void consider_edge(const Path& path, int start, int p, CrossState& c) {
  if (seg_intersect(path, start, path.at(p), path.at(path.next(p)), c.tmp)) {
    c.newdist = (wide(path.at(start).x) - c.tmp.x) + (wide(path.at(start).y) - c.tmp.y);
    c.diagonal = true;
  }
  if (test_insideness(path, start, path.at(p))) {
    if (!inside_limit(path, start, p, c)) {
      return;
    }
  } else if (!c.diagonal) {
    return;
  }
  c.newdist = std::abs(c.newdist);
  if (!c.crosses || c.newdist < c.tdist) {
    c.crosses = true;
    c.best = c.tmp;
    c.tdist = c.newdist;
    c.crossed = c.diagonal ? p : kEnd;
  }
}

// path_intersect: whether another edge limits the triangle at `start`; the
// limiting point goes to `out` (and into the crossed diagonal edge).
bool path_intersect(Path& path, int start, Point& out) {
  CrossState c;
  for (int p = path.head(); path.next(p) != kEnd; p = path.next(p)) {
    if (p != start && p != path.next(start)) {
      consider_edge(path, start, p, c);
    }
  }
  if (c.crosses) {
    out = c.best;
    if (c.crossed != kEnd) {
      path.insert_after(c.crossed, c.best);
    }
  }
  return c.crosses;
}

// Splits the triangle off the diagonal edge after `p` (one step of
// CIFMakeManhattanPath).
void split_diagonal(Path& path, int p, bool clockwise, std::vector<PolygonPiece>& out) {
  const int next = path.next(p);
  const Point first = clockwise ? path.at(next) : path.at(p);
  const Point last = clockwise ? path.at(p) : path.at(next);
  const EdgeDir dir = edge_direction(first, last);
  const bool corner_first_x = dir == EdgeDir::dl || dir == EdgeDir::ur;
  const int base = path.insert_after(p, corner_first_x ? Point{first.x, last.y} : Point{last.x, first.y});
  Point clip;
  if (path_intersect(path, p, clip)) {
    path.at(base) = clip;
    const Point a = path.at(p);
    const Point b = path.at(next);
    const Point mid = a.x == clip.x
                          ? Point{interpolate(a.x, wide(clip.y) - a.y, wide(b.x) - a.x, wide(b.y) - a.y), clip.y}
                          : Point{clip.x, interpolate(a.y, wide(clip.x) - a.x, wide(b.y) - a.y, wide(b.x) - a.x)};
    path.insert_after(base, mid);
  }
  const Rect tr = canonical_of(path.at(p), path.at(path.next(path.next(p))));
  if (tr.x1 - tr.x0 > 0 && tr.y1 - tr.y0 > 0) {
    const bool right = !(dir == EdgeDir::ur || dir == EdgeDir::ul);  // TT_SIDE
    const bool backslash = !(dir == EdgeDir::ur || dir == EdgeDir::dl);  // TT_DIRECTION
    out.push_back(PolygonPiece{tr, TriangleSide{backslash ? Diagonal::nw_se : Diagonal::ne_sw, right}});
  }
}

void make_manhattan(Path& path, std::vector<PolygonPiece>& out) {
  const bool clockwise = is_clockwise(path);
  for (int p = path.head(); path.next(p) != kEnd; p = path.next(p)) {
    const Point a = path.at(p);
    const Point b = path.at(path.next(p));
    if (a.x != b.x && a.y != b.y) {
      split_diagonal(path, p, clockwise, out);
    }
  }
}

enum class Orient : std::int8_t { falling = -1, horizontal = 0, rising = 1 };

struct Edge {
  Point from;
  Point to;
  Orient dir = Orient::horizontal;
};

bool crosses(const Edge& e, Coord ybot, Coord ytop) noexcept {
  if (e.dir == Orient::rising) {
    return e.from.y <= ybot && e.to.y >= ytop;
  }
  if (e.dir == Orient::falling) {
    return e.to.y <= ybot && e.from.y >= ytop;
  }
  return false;
}

// The strips of one minimum y-range, left to right by wrap number.
void strips(const std::vector<Edge>& edges, Coord ybot, Coord ytop, std::vector<Rect>& found) {
  int wrap = 0;
  Coord xbot = 0;
  for (const Edge& e : edges) {
    if (wrap == 0) {
      xbot = e.from.x;
    }
    if (!crosses(e, ybot, ytop)) {
      continue;
    }
    wrap += e.dir == Orient::rising ? 1 : -1;
    if (wrap == 0 && xbot != e.from.x) {
      found.push_back(Rect{xbot, ybot, e.from.x, ytop});
    }
  }
}

// cifOrient: the edges of the path with their direction; false when an
// edge is still not Manhattan.
bool manhattan_edges(const Path& path, std::vector<Edge>& edges) {
  for (int p = path.head(); path.next(p) != kEnd; p = path.next(p)) {
    const Point a = path.at(p);
    const Point b = path.at(path.next(p));
    if (a.x != b.x && a.y != b.y) {
      return false;
    }
    Orient dir = Orient::horizontal;
    if (a.x == b.x && a.y != b.y) {
      dir = a.y < b.y ? Orient::rising : Orient::falling;
    }
    edges.push_back(Edge{a, b, dir});
  }
  return true;
}

// CIFPolyToRects after the path is Manhattan. A path that is still not
// Manhattan, or has fewer than four edges, gives no strips (Magic reports a
// CIF read error and keeps the triangles already painted).
void poly_to_rects(const Path& path, std::vector<PolygonPiece>& out) {
  std::vector<Edge> edges;
  if (!manhattan_edges(path, edges) || edges.size() < 4) {
    return;
  }
  std::vector<Coord> ys;
  for (const Edge& e : edges) {
    ys.push_back(e.from.y);
  }
  std::stable_sort(ys.begin(), ys.end());
  std::stable_sort(edges.begin(), edges.end(), [](const Edge& l, const Edge& r) { return l.from.x < r.from.x; });
  std::vector<Rect> found;
  for (std::size_t curr = 1; curr < ys.size(); ++curr) {
    const Coord ybot = ys[curr - 1];
    while (curr < ys.size() && ys[curr] == ybot) {
      ++curr;
    }
    if (curr >= ys.size()) {
      break;
    }
    strips(edges, ybot, ys[curr], found);
  }
  for (auto it = found.rbegin(); it != found.rend(); ++it) {
    out.push_back(PolygonPiece{*it, std::nullopt});
  }
}

}  // namespace

diag::Outcome<std::vector<PolygonPiece>> decompose_polygon(std::span<const Point> vertices) {
  if (vertices.size() < 3) {
    return diag::make(codes::kBadArgument, "a polygon needs at least three vertices");
  }
  Path path(vertices);
  path.close();
  std::vector<PolygonPiece> pieces;
  make_manhattan(path, pieces);
  poly_to_rects(path, pieces);
  return pieces;
}

}  // namespace hefesto::db
