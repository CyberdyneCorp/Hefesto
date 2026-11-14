// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 3, selective bloats (capability mask-output-operators
// "bloat-or", "bloat-max and bloat-min"; design D6): CIFOP_BLOAT,
// CIFOP_BLOATMAX and CIFOP_BLOATMIN.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (cifBloatFunc,
// cifBloatMaxFunc, cifSrTiles), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include <algorithm>
#include <limits>

#include "mask/ops/grow.hpp"

namespace hefesto::mask {

namespace {

/// The bloat table (bl_distance) of a bloat-or/min/max operator.
class BloatTable {
 public:
  explicit BloatTable(const LayerOp& op) : op_(op) {}
  [[nodiscard]] db::Coord operator()(tech::TileType t) const {
    return t < tech::kMaxTileTypes ? detail::bloat_distance(op_, std::size_t{t}) : 0;
  }
  [[nodiscard]] tech::PlaneId plane() const {
    return static_cast<tech::PlaneId>(std::max(detail::bloat_plane(op_), 0));
  }

 private:
  const LayerOp& op_;
};

/// cifSrTiles for the bloats: the operator's types on the bloat plane only.
template <typename Fn>
void search_bloat_plane(const OpArgs& args, Fn&& fn) {
  const LayerInputs& in = args.inputs;
  const tech::PlaneId p = BloatTable{args.op}.plane();
  if (in.layout == nullptr || p >= in.layout->plane_count() || args.op.types.empty()) {
    return;
  }
  if (in.technology != nullptr && !in.technology->plane_types(p).intersects(args.op.types)) {
    return;
  }
  const db::Plane& plane = in.layout->plane(p);
  plane.enumerate(args.area, args.op.types, [&](const db::TileView& t) {
    fn(plane, t);
    return true;
  });
}

// --- bloat-or (cifBloatFunc) --------------------------------------------------

struct BloatOr {
  const db::Plane& plane;
  const db::TileView& t;
  const BloatTable& table;
  MaskPlane& out;
  std::int32_t s;
  db::Rect tile;  // tileArea
  db::Rect cif;   // cifArea
  tech::TileType top_left = tech::kSpace;
  tech::TileType bottom_right = tech::kSpace;

  [[nodiscard]] bool side() const noexcept { return t.split && t.right_side; }
  [[nodiscard]] bool dir() const noexcept { return t.split && detail::direction(t); }
  [[nodiscard]] bool leg_on_top() const noexcept { return side() == dir(); }
  [[nodiscard]] db::Coord sc(db::Coord v) const { return detail::clamp_coord(std::int64_t{v} * s); }
  void bloat(const db::Rect& r, tech::TileType other, tech::TileType own) const {
    if (table(other) != 0 && other != own) {
      detail::paint(out, r);
    }
  }

  /// The diagonal part without grow-euclidean: the leg rectangles and the
  /// triangle moved along its hypotenuse direction.
  void split_square(db::Coord dist) const {
    const bool top = leg_on_top();
    db::Rect b;
    b.y0 = top ? cif.y1 - dist : cif.y0;
    b.y1 = top ? cif.y1 : cif.y0 + dist;
    b.x0 = side() ? cif.x0 - dist : cif.x0;
    b.x1 = side() ? cif.x1 : cif.x1 + dist;
    detail::paint(out, b);
    b.y0 = top ? cif.y0 - dist : cif.y0;
    b.y1 = top ? cif.y1 : cif.y1 + dist;
    b.x0 = side() ? cif.x1 - dist : cif.x0;
    b.x1 = side() ? cif.x1 : cif.x0 + dist;
    detail::paint(out, b);
    const db::Coord sx = side() ? -dist : dist;
    const db::Coord sy = top ? -dist : dist;
    detail::paint_shape(out, t, db::Rect{cif.x0 + sx, cif.y0 + sy, cif.x1 + sx, cif.y1 + sy});
  }

  void left_side() {
    db::Coord tilestop = tile.y1;
    db::Coord cifstop = cif.y1;
    tech::TileType type = t.type;
    top_left = type;
    if (t.split) {
      type = t.left_type;
      if (side()) {
        if (dir()) {
          top_left = type;
          return;
        }
        tilestop = tile.y0;
        cifstop = cif.y0;
        type = detail::bottom_type(t);
      }
    }
    db::Rect b;
    b.y0 = cif.y0 - table(detail::facing_type(detail::first_neighbor(plane, t, db::Side::bottom), db::Side::bottom));
    b.x1 = cif.x0;
    for (const db::TileView& n : detail::side_tiles(plane, t, db::Side::left)) {
      if (n.area.y0 >= tilestop) {
        continue;
      }
      top_left = detail::right_type(n);
      b.x0 = b.x1 - table(top_left);
      b.y1 = n.area.y1 > tilestop ? cifstop : sc(n.area.y1);
      bloat(b, top_left, type);
      b.y0 = b.y1;
    }
  }

  void top_side() const {
    db::Coord cifstart = cif.x1;
    db::Coord tilestart = tile.x1;
    tech::TileType type = t.type;
    if (t.split) {
      type = detail::top_type(t);
      if (!leg_on_top()) {
        if (side()) {
          return;
        }
        cifstart = cif.x0;
        tilestart = tile.x0;
        type = t.left_type;
      }
    }
    db::Rect b;
    b.y0 = cif.y1;
    b.x1 = cifstart;
    const std::vector<db::TileView> top = detail::side_tiles(plane, t, db::Side::top);
    for (auto n = top.rbegin(); n != top.rend(); ++n) {
      if (n->area.x0 >= tilestart) {
        continue;
      }
      const tech::TileType other = detail::bottom_type(*n);
      b.y1 = b.y0 + table(other);
      b.x0 = n->area.x0 <= tile.x0 ? cif.x0 - table(top_left) : sc(n->area.x0);
      bloat(b, other, type);
      b.x1 = b.x0;
    }
  }

  void right_side() {
    db::Coord tilestop = tile.y0;
    db::Coord cifstop = cif.y0;
    tech::TileType type = t.type;
    bottom_right = type;
    if (t.split) {
      type = t.right_type;
      if (!side()) {
        if (dir()) {
          bottom_right = type;
          return;
        }
        tilestop = tile.y1;
        cifstop = cif.y1;
        type = detail::top_type(t);
      }
    }
    db::Rect b;
    b.y1 = cif.y1 + table(detail::facing_type(detail::last_neighbor(plane, t, db::Side::top), db::Side::top));
    b.x0 = cif.x1;
    const std::vector<db::TileView> right = detail::side_tiles(plane, t, db::Side::right);
    for (auto n = right.rbegin(); n != right.rend(); ++n) {
      if (n->area.y1 <= tilestop) {
        continue;
      }
      bottom_right = detail::left_type(*n);
      b.x1 = b.x0 + table(bottom_right);
      b.y0 = n->area.y0 < tilestop ? cifstop : sc(n->area.y0);
      bloat(b, bottom_right, type);
      b.y1 = b.y0;
    }
  }

  void bottom_side() const {
    db::Coord cifstart = cif.x0;
    db::Coord tilestart = tile.x0;
    tech::TileType type = t.type;
    if (t.split) {
      type = detail::bottom_type(t);
      if (leg_on_top()) {
        if (!dir()) {
          return;
        }
        cifstart = cif.x1;
        tilestart = tile.x1;
        type = t.right_type;
      }
    }
    db::Rect b;
    b.y1 = cif.y0;
    b.x0 = cifstart;
    for (const db::TileView& n : detail::side_tiles(plane, t, db::Side::bottom)) {
      if (n.area.x1 <= tilestart) {
        continue;
      }
      const tech::TileType other = detail::top_type(n);
      b.y0 = b.y1 - table(other);
      b.x1 = n.area.x1 >= tile.x1 ? cif.x1 + table(bottom_right) : sc(n.area.x1);
      bloat(b, other, type);
      b.x0 = b.x1;
    }
  }
};

void bloat_or_tile(const OpArgs& args, OperatorContext& ctx, const db::Plane& plane, const db::TileView& t,
                   MaskPlane& out) {
  const BloatTable table{args.op};
  const std::int32_t s = detail::scale_of(args);
  BloatOr b{plane, t, table, out, s, t.area, detail::scale_rect(t.area, s)};
  if (!t.split) {
    detail::paint(out, b.cif);
  } else {
    const db::Coord dist = table(t.right_side ? t.left_type : t.right_type);
    if (detail::euclidean(args)) {
      detail::grow_tile_euclidean(out, plane, t, dist, false, detail::grid_limit_of(args), ctx.compat.magic_compat, s);
    } else {
      b.split_square(dist);
    }
  }
  b.left_side();
  b.top_side();
  b.right_side();
  b.bottom_side();
  detail::count_tile(ctx);
}

// --- bloat-max, bloat-min (cifBloatMaxFunc) ----------------------------------

constexpr db::Coord kNoBloat = 10000000;

/// The extreme distance over the neighbours of one side whose (left) type
/// differs from the tile's; kNoBloat-sentinels when there is none.
db::Coord side_bloat(const std::vector<db::TileView>& n, tech::TileType own, const BloatTable& table, bool max) {
  db::Coord bloat = max ? -kNoBloat : kNoBloat;
  for (const db::TileView& v : n) {
    const tech::TileType other = detail::body_type(v);
    if (other == own) {
      continue;
    }
    const db::Coord d = table(other);
    bloat = max ? std::max(bloat, d) : std::min(bloat, d);
  }
  return bloat;
}

bool real_bloat(db::Coord b) noexcept { return b < kNoBloat && b > -kNoBloat; }

void bloat_extreme_tile(const OpArgs& args, OperatorContext& ctx, const db::Plane& plane, const db::TileView& t,
                        MaskPlane& out, bool max) {
  const BloatTable table{args.op};
  const tech::TileType own = detail::body_type(t);
  const db::Rect scaled = detail::scale_rect(t.area, detail::scale_of(args));
  db::Rect a = scaled;
  const db::Coord left = side_bloat(detail::side_tiles(plane, t, db::Side::left), own, table, max);
  a.x0 -= real_bloat(left) ? left : 0;
  const db::Coord top = side_bloat(detail::side_tiles(plane, t, db::Side::top), own, table, max);
  a.y1 += real_bloat(top) ? top : 0;
  const db::Coord right = side_bloat(detail::side_tiles(plane, t, db::Side::right), own, table, max);
  a.x1 += real_bloat(right) ? right : 0;
  const db::Coord bottom = side_bloat(detail::side_tiles(plane, t, db::Side::bottom), own, table, max);
  a.y0 -= real_bloat(bottom) ? bottom : 0;
  if (a.x0 > a.x1 || a.y0 > a.y1) {
    detail::cif_error(args, ctx, scaled, detail::kTileInverted, "tile inverted by shrink");
  } else {
    detail::paint_shape(out, t, a, ctx.compat.magic_compat);
  }
  detail::count_tile(ctx);
}

}  // namespace

diag::Status op_bloat_or(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  search_bloat_plane(args,
                     [&](const db::Plane& plane, const db::TileView& t) { bloat_or_tile(args, ctx, plane, t, out); });
  return diag::Done{};
}

diag::Status op_bloat_max(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  search_bloat_plane(
      args, [&](const db::Plane& plane, const db::TileView& t) { bloat_extreme_tile(args, ctx, plane, t, out, true); });
  return diag::Done{};
}

diag::Status op_bloat_min(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  search_bloat_plane(args, [&](const db::Plane& plane, const db::TileView& t) {
    bloat_extreme_tile(args, ctx, plane, t, out, false);
  });
  return diag::Done{};
}

}  // namespace hefesto::mask
