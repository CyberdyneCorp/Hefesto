// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Face generation of one layer in one chunk (view-3d "Rendered geometry";
// openspec/changes/m10-3d-view/design.md D6 "Geometry", "Encoding"), from
// the layer's flat plane (flatten.cpp):
// - a top/bottom rectangle per Manhattan tile piece inside the chunk;
// - a split tile's part, clipped exactly to the chunk (GrClipTriangle), as
//   a triangle fan, with its diagonal side face;
// - side faces on the boundary segments where the neighbour across a tile
//   side has another type (GrBoxOutline's walk, over the flattened type
//   rather than one cell's plane, so abutting pieces of different cells
//   leave no inner faces), clipped to the chunk;
// - closed faces on the clip-area edge (Magic's "edges cut by the clipping
//   area", W3Dmain.c:357-374), flagged clip_edge;
// - no side faces at all for a zero-thickness piece (W3Dmain.c:287).
// With magic-compat ON a split tile is drawn with the h and t of its
// source tile's left type on both triangles (w3dRenderVolume calls
// ExtGetZAxis, which reads TiGetLeftType; view-3d "the left-side type is
// used"): its triangles carry those values, its side faces too when the
// left type is another type (side_flag::own_height), and whether it has
// side faces follows that thickness, not the layer's. A right triangle
// over space is therefore flat and has no side faces, as in Magic. With
// it OFF each triangle takes its own layer's h and t and side faces (J3,
// user decision 2026-11-21).
// Layer heights come from the build's HeightTable (the stack's, or a
// contact spanned by the J1 option).
// A boundary segment on the clip edge is emitted once, as a clip-edge face.
// Segments are stored with increasing coordinates; the outward_left flag
// gives the outward normal.
//
// Owner: MESH (design D3).

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "internal.hpp"

namespace hefesto::view3d::detail {

namespace {

struct Vec2 {
  double x = 0.0;
  double y = 0.0;

  friend bool operator==(const Vec2&, const Vec2&) = default;
};

using Polygon = std::vector<Vec2>;

constexpr std::array<db::Side, 4> kSides = {db::Side::left, db::Side::right, db::Side::bottom, db::Side::top};

[[nodiscard]] bool is_vertical(db::Side s) noexcept {
  return s == db::Side::left || s == db::Side::right;
}

// The type of `n`'s part that faces the tile across side `s` of the tile.
[[nodiscard]] tech::TileType facing_type(const db::TileView& n, db::Side s) noexcept {
  if (!n.split) {
    return n.type;
  }
  const bool nw_se = n.diagonal == db::Diagonal::nw_se;
  switch (s) {
    case db::Side::right:
      return n.left_type;
    case db::Side::left:
      return n.right_type;
    case db::Side::top:  // n's bottom edge
      return nw_se ? n.left_type : n.right_type;
    case db::Side::bottom:  // n's top edge
      return nw_se ? n.right_type : n.left_type;
  }
  return n.type;
}

// The x of a vertical side, the y of a horizontal one.
[[nodiscard]] db::Coord side_coord(const db::Rect& r, db::Side s) noexcept {
  switch (s) {
    case db::Side::left:
      return r.x0;
    case db::Side::right:
      return r.x1;
    case db::Side::bottom:
      return r.y0;
    case db::Side::top:
      return r.y1;
  }
  return r.x0;
}

// Whether the reported part of a split tile owns side `s` of its rectangle.
[[nodiscard]] bool owns_side(const db::TileView& t, db::Side s) noexcept {
  const bool r = t.right_side;
  const bool nw_se = t.diagonal == db::Diagonal::nw_se;
  switch (s) {
    case db::Side::left:
      return !r;
    case db::Side::right:
      return r;
    case db::Side::bottom:
      return nw_se ? !r : r;
    case db::Side::top:
      return nw_se ? r : !r;
  }
  return false;
}

// The reported triangle of a split tile, counter-clockwise.
[[nodiscard]] Polygon triangle_of(const db::TileView& t) {
  const double x0 = t.area.x0;
  const double y0 = t.area.y0;
  const double x1 = t.area.x1;
  const double y1 = t.area.y1;
  if (t.diagonal == db::Diagonal::nw_se) {
    return t.right_side ? Polygon{{x1, y0}, {x1, y1}, {x0, y1}} : Polygon{{x0, y0}, {x1, y0}, {x0, y1}};
  }
  return t.right_side ? Polygon{{x0, y0}, {x1, y0}, {x1, y1}} : Polygon{{x0, y0}, {x1, y1}, {x0, y1}};
}

// One Sutherland-Hodgman pass: keeps the part of `in` where
// `sign * (axis coordinate - c) >= 0`.
[[nodiscard]] Polygon clip_half(const Polygon& in, bool x_axis, double c, double sign) {
  Polygon out;
  const auto coord = [x_axis](const Vec2& v) { return x_axis ? v.x : v.y; };
  const auto inside = [&](const Vec2& v) { return sign * (coord(v) - c) >= 0.0; };
  for (std::size_t i = 0; i < in.size(); ++i) {
    const Vec2& a = in[i];
    const Vec2& b = in[(i + 1) % in.size()];
    if (inside(a)) {
      out.push_back(a);
    }
    if (inside(a) != inside(b)) {
      const double t = (c - coord(a)) / (coord(b) - coord(a));
      out.push_back(x_axis ? Vec2{c, a.y + (t * (b.y - a.y))} : Vec2{a.x + (t * (b.x - a.x)), c});
    }
  }
  return out;
}

[[nodiscard]] Polygon clip_polygon(Polygon poly, const db::Rect& r) {
  poly = clip_half(poly, true, r.x0, 1.0);
  poly = clip_half(poly, true, r.x1, -1.0);
  poly = clip_half(poly, false, r.y0, 1.0);
  poly = clip_half(poly, false, r.y1, -1.0);
  Polygon out;
  for (const Vec2& v : poly) {
    if (out.empty() || !(out.back() == v)) {
      out.push_back(v);
    }
  }
  while (out.size() > 1 && out.front() == out.back()) {
    out.pop_back();
  }
  return out;
}

class Emitter {
 public:
  Emitter(const FlatType& flat, LayerId layer, const HeightTable& heights, const db::Rect& clip, Chunk& chunk)
      : flat_(flat), layer_(layer), heights_(heights), clip_(clip), chunk_(chunk) {
    // Zero thickness: top faces only (W3Dmain.c:287).
    sides_ = heights.layer(layer).thickness != 0.0F;
  }

  void tile(const db::TileView& t) {
    if (t.split) {
      split_tile(t);
    } else {
      rect_tile(t);
    }
  }

 private:
  void rect_tile(const db::TileView& t) {
    const db::Rect piece = intersect(t.area, chunk_.area);
    if (piece.is_null()) {
      return;
    }
    chunk_.rects.push_back(RectInstance{fx(piece.x0), fy(piece.y0), fx(piece.x1), fy(piece.y1), layer_});
    if (!sides_) {
      return;
    }
    for (const db::Side s : kSides) {
      boundary(t, s);
    }
    rect_clip_edges(piece);
  }

  void split_tile(const db::TileView& t) {
    const Polygon poly = clip_polygon(triangle_of(t), chunk_.area);
    if (poly.size() < 3) {
      return;
    }
    // J3: magic-compat ON takes the source left type's h and t; OFF the
    // triangle's own type's.
    const tech::TileType z_type = heights_.stack().magic_compat() ? flat_.z_type_of(t) : flat_.type;
    const DrawHeight z = z_type == flat_.type ? heights_.layer(layer_) : heights_.type(z_type);
    for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
      chunk_.triangles.push_back(Triangle{{fx(poly[0].x), fx(poly[i].x), fx(poly[i + 1].x)},
                                          {fy(poly[0].y), fy(poly[i].y), fy(poly[i + 1].y)},
                                          layer_, z.height, z.thickness});
    }
    if (z.thickness == 0.0F) {
      return;
    }
    if (z_type != flat_.type) {
      split_z_ = z;
    }
    for (const db::Side s : kSides) {
      if (owns_side(t, s)) {
        boundary(t, s);
      }
    }
    polygon_edges(poly);
    split_z_.reset();
  }

  // The runs of side `s` of `t` whose neighbour has another type, inside
  // this chunk and off the clip edge.
  void boundary(const db::TileView& t, db::Side s) {
    const bool vertical = is_vertical(s);
    const db::Coord at = side_coord(t.area, s);
    if (!edge_in_chunk(s, at) || on_clip_line(vertical, at)) {
      return;
    }
    const db::Coord lo = vertical ? t.area.y0 : t.area.x0;
    const db::Coord hi = vertical ? t.area.y1 : t.area.x1;
    flat_.plane.neighbors(t, s, lo, hi, scratch_);
    bool open = false;
    db::Coord run_lo = 0;
    db::Coord run_hi = 0;
    for (const db::TileView& n : scratch_) {
      const db::Coord n_lo = std::max(lo, vertical ? n.area.y0 : n.area.x0);
      const db::Coord n_hi = std::min(hi, vertical ? n.area.y1 : n.area.x1);
      if (facing_type(n, s) == flat_.type) {
        flush_run(s, at, open, run_lo, run_hi);
        continue;
      }
      if (!open || run_hi != n_lo) {
        flush_run(s, at, open, run_lo, run_hi);
        open = true;
        run_lo = n_lo;
      }
      run_hi = n_hi;
    }
    flush_run(s, at, open, run_lo, run_hi);
  }

  void flush_run(db::Side s, db::Coord at, bool& open, db::Coord lo, db::Coord hi) {
    if (!open) {
      return;
    }
    open = false;
    const bool vertical = is_vertical(s);
    const db::Coord c_lo = std::max(lo, vertical ? chunk_.area.y0 : chunk_.area.x0);
    const db::Coord c_hi = std::min(hi, vertical ? chunk_.area.y1 : chunk_.area.x1);
    if (c_lo < c_hi) {
      axis_side(s, at, c_lo, c_hi, 0);
    }
  }

  // A tile side at `at` belongs to the chunk holding the tile's interior
  // next to it (left and bottom sides: [lo, hi); right and top: (lo, hi]).
  [[nodiscard]] bool edge_in_chunk(db::Side s, db::Coord at) const noexcept {
    const db::Rect& k = chunk_.area;
    switch (s) {
      case db::Side::left:
        return k.x0 <= at && at < k.x1;
      case db::Side::right:
        return k.x0 < at && at <= k.x1;
      case db::Side::bottom:
        return k.y0 <= at && at < k.y1;
      case db::Side::top:
        return k.y0 < at && at <= k.y1;
    }
    return false;
  }

  [[nodiscard]] bool on_clip_line(bool vertical, double at) const noexcept {
    return vertical ? (at == clip_.x0 || at == clip_.x1) : (at == clip_.y0 || at == clip_.y1);
  }

  void rect_clip_edges(const db::Rect& piece) {
    if (piece.x0 == clip_.x0) {
      axis_side(db::Side::left, piece.x0, piece.y0, piece.y1, side_flag::clip_edge);
    }
    if (piece.x1 == clip_.x1) {
      axis_side(db::Side::right, piece.x1, piece.y0, piece.y1, side_flag::clip_edge);
    }
    if (piece.y0 == clip_.y0) {
      axis_side(db::Side::bottom, piece.y0, piece.x0, piece.x1, side_flag::clip_edge);
    }
    if (piece.y1 == clip_.y1) {
      axis_side(db::Side::top, piece.y1, piece.x0, piece.x1, side_flag::clip_edge);
    }
  }

  // The diagonal piece and the clip-edge pieces of a clipped triangle.
  void polygon_edges(const Polygon& poly) {
    Vec2 centre;
    for (const Vec2& v : poly) {
      centre.x += v.x / static_cast<double>(poly.size());
      centre.y += v.y / static_cast<double>(poly.size());
    }
    for (std::size_t i = 0; i < poly.size(); ++i) {
      const Vec2& a = poly[i];
      const Vec2& b = poly[(i + 1) % poly.size()];
      if (a.x != b.x && a.y != b.y) {
        diagonal(a, b, centre);
      } else if (a.x == b.x && on_clip_line(true, a.x)) {
        axis_side(a.x == clip_.x0 ? db::Side::left : db::Side::right, a.x, std::min(a.y, b.y), std::max(a.y, b.y),
                  side_flag::clip_edge);
      } else if (a.y == b.y && on_clip_line(false, a.y)) {
        axis_side(a.y == clip_.y0 ? db::Side::bottom : db::Side::top, a.y, std::min(a.x, b.x), std::max(a.x, b.x),
                  side_flag::clip_edge);
      }
    }
  }

  void diagonal(Vec2 a, Vec2 b, const Vec2& centre) {
    if (b.x < a.x) {
      std::swap(a, b);
    }
    // Left normal of a->b is (-dy, dx); the interior is on the centre's side.
    const double dot = (-(b.y - a.y) * (centre.x - a.x)) + ((b.x - a.x) * (centre.y - a.y));
    const std::uint8_t outward = dot < 0.0 ? side_flag::outward_left : std::uint8_t{0};
    push_side(a.x, a.y, b.x, b.y, static_cast<std::uint8_t>(side_flag::diagonal | outward));
  }

  // A segment on side `s` at `at`, from lo to hi (lo < hi).
  void axis_side(db::Side s, double at, double lo, double hi, std::uint8_t extra) {
    const bool outward_left = s == db::Side::left || s == db::Side::top;
    const auto flags = static_cast<std::uint8_t>(extra | (outward_left ? side_flag::outward_left : 0U));
    if (is_vertical(s)) {
      push_side(at, lo, at, hi, flags);
    } else {
      push_side(lo, at, hi, at, flags);
    }
  }

  void push_side(double x0, double y0, double x1, double y1, std::uint8_t flags) {
    if (!split_z_) {
      chunk_.sides.push_back(SideInstance{fx(x0), fy(y0), fx(x1), fy(y1), layer_, flags});
      return;
    }
    chunk_.sides.push_back(SideInstance{fx(x0), fy(y0), fx(x1), fy(y1), layer_,
                                        static_cast<std::uint8_t>(flags | side_flag::own_height), split_z_->height,
                                        split_z_->thickness});
  }

  [[nodiscard]] float fx(double x) const noexcept { return static_cast<float>(x - chunk_.origin_x); }
  [[nodiscard]] float fy(double y) const noexcept { return static_cast<float>(y - chunk_.origin_y); }

  const FlatType& flat_;
  LayerId layer_;
  const HeightTable& heights_;
  bool sides_ = false;
  std::optional<DrawHeight> split_z_;  // a split tile drawn with another type's h and t, while its sides are emitted
  db::Rect clip_;
  Chunk& chunk_;
  std::vector<db::TileView> scratch_;
};

}  // namespace

void emit_layer(const FlatType& flat, LayerId layer, const HeightTable& heights, const db::Rect& clip, Chunk& chunk) {
  Emitter emitter(flat, layer, heights, clip, chunk);
  flat.plane.enumerate(chunk.area, tech::TypeMask::of(flat.type), [&](const db::TileView& t) {
    emitter.tile(t);
    return true;
  });
}

}  // namespace hefesto::view3d::detail
