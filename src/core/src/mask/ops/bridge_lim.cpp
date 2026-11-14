// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 3, bridge-lim (capability mask-output-operators
// "bridge-lim"; design D6, Q-MASK-3, 5, 228, 229): CIFOP_BRIDGELIM.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGenLayer's
// CIFOP_BRIDGELIM case, bridgeLimFound, bridgeLimSrTiles,
// cifBridgeLimFunc0, bridgeLimCheckFunc, bridgeErase, cifBridgeLimFunc1,
// cifBridgeLimFunc2), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include <algorithm>

#include "mask/ops/bridge.hpp"

namespace hefesto::mask {

namespace {

db::Rect intersect(const db::Rect& a, const db::Rect& b) noexcept {
  return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

/// The limiting layers of a bridge-lim (bridgeLimSrTiles, bridgeErase),
/// with the units of Q-MASK-5 and Q-MASK-229.
class Limits {
 public:
  Limits(const OpArgs& args, const OperatorContext& ctx)
      : layout_(detail::layout_sources(args.inputs, args.op.types)),
        layers_(detail::layer_sources(args.inputs, args.op.cif_layers)),
        types_(args.op.types),
        scale_(detail::scale_of(args)),
        compat_(ctx.compat) {}

  struct Overlap {
    bool found = false;
    db::Coord xmax = 0;
    db::Coord ymax = 0;
  };

  /// bridgeLimSrTiles with calcOverlap: the largest right and top edges of
  /// the limiting tiles overlapping `a`.
  [[nodiscard]] Overlap overlap(const db::Rect& a) const {
    Overlap o{false, a.x0, a.y0};
    const auto take = [&](db::Coord right, db::Coord top) {
      o.xmax = std::max(o.xmax, right);
      o.ymax = std::max(o.ymax, top);
    };
    for (const detail::Source& s : layout_) {
      s.plane->enumerate(q_mask_5_limit_search_area(a, scale_, compat_), types_, [&](const db::TileView& t) {
        take(edge(t.area.x1), edge(t.area.y1));
        return true;
      });
    }
    for (const MaskPlane* m : layers_) {
      m->plane().enumerate(a, detail::solid_mask(), [&](const db::TileView& t) {
        take(t.area.x1, t.area.y1);
        return true;
      });
    }
    o.found = o.xmax != a.x0 || o.ymax != a.y0;
    return o;
  }

  /// bridgeLimSrTiles without calcOverlap: no limiting tile overlaps `a`.
  [[nodiscard]] bool free(const db::Rect& a) const {
    bool hit = false;
    const auto stop = [&hit](const db::TileView&) {
      hit = true;
      return false;
    };
    for (const detail::Source& s : layout_) {
      if (!hit) {
        s.plane->enumerate(q_mask_5_limit_search_area(a, scale_, compat_), types_, stop);
      }
    }
    for (const MaskPlane* m : layers_) {
      if (!hit) {
        m->plane().enumerate(a, detail::solid_mask(), stop);
      }
    }
    return !hit;
  }

  /// bridgeErase: removes the limiting geometry under `a` from `into`.
  void erase(MaskPlane& into, const db::Rect& a) const {
    const bool clip = q_mask_229_clip_erase(compat_);
    for (const detail::Source& s : layout_) {
      s.plane->enumerate(q_mask_5_limit_search_area(a, scale_, compat_), types_, [&](const db::TileView& t) {
        erase_tile(into, t, clip ? detail::scale_rect(t.area, scale_) : t.area, a, clip);
        return true;
      });
    }
    for (const MaskPlane* m : layers_) {
      m->plane().enumerate(a, detail::solid_mask(), [&](const db::TileView& t) {
        erase_tile(into, t, t.area, a, clip);
        return true;
      });
    }
  }

 private:
  [[nodiscard]] db::Coord edge(db::Coord v) const {
    return detail::clamp_coord(q_mask_5_tile_edge(v, scale_, compat_));
  }

  /// ON erases the whole tile; OFF its part inside `a` (a split tile is
  /// erased whole: its triangle cannot be clipped to a rectangle).
  void erase_tile(MaskPlane& into, const db::TileView& t, const db::Rect& r, const db::Rect& a, bool clip) const {
    if (clip && !t.split) {
      detail::erase(into, intersect(r, a));
    } else {
      detail::erase_shape(into, t, r, compat_.magic_compat);
    }
  }

  std::vector<detail::Source> layout_;
  std::vector<const MaskPlane*> layers_;
  tech::TypeMask types_;
  std::int32_t scale_;
  compat::CompatSettings compat_;
};

/// A corner-stitch step through Plane::tile_at (nullopt beyond the plane).
std::optional<db::TileView> tile_at(const db::Plane& plane, db::Point p, const db::TileView& near) {
  if (!db::in_usable_plane(db::Rect{p.x, p.y, p.x + 1, p.y + 1})) {
    return std::nullopt;
  }
  return plane.tile_at(p, &near);
}

// --- Pass 0 (cifBridgeLimFunc0) ----------------------------------------------

struct Widener {
  const db::Plane& plane;
  MaskPlane& out;
  const Limits& limits;
  db::Coord width;

  /// One column between bottom neighbour `p` and top-side tile `q`.
  void column(const db::TileView& t, const db::Rect& area, const db::TileView& p, const db::TileView& q,
              db::Coord lim) const {
    db::Rect col;
    col.x0 = std::max(q.area.x0, lim);
    col.x1 = std::min(std::min(q.area.x1, p.area.x1), area.x1);
    col.y1 = detail::bottom_type(q) == detail::top_type(t) ? q.area.y1 : area.y1;
    const db::Coord y0 = detail::top_type(p) == detail::bottom_type(t) ? p.area.y0 : area.y0;
    if (col.y1 - y0 >= width) {
      return;
    }
    col.y0 = col.y1 - width;
    const Limits::Overlap o = limits.overlap(col);
    if (o.found) {
      col.y0 = std::min(y0, o.ymax);
      col.y1 = col.y0 + width;
    }
    detail::paint(out, col);
  }

  void tile(const db::TileView& t) const {
    db::Rect area = t.area;
    if (area.width() < width) {
      area.x0 = area.x1 - width;
      const Limits::Overlap o = limits.overlap(area);
      if (o.found) {
        area.x0 = std::min(t.area.x0, o.xmax);
        area.x1 = area.x0 + width;
      }
    }
    if (area.height() < width) {
      for (const db::TileView& p : detail::side_tiles(plane, t, db::Side::bottom)) {
        const db::Coord lim = std::max(p.area.x0, area.x0);
        // RT, then BL: the tiles along the top side, right to left, past
        // the tile's left end while they reach beyond `lim`.
        for (auto q = tile_at(plane, db::Point{t.area.x1 - 1, t.area.y1}, t); q && q->area.x1 > lim;
             q = tile_at(plane, db::Point{q->area.x0 - 1, q->area.y0}, *q)) {
          if (q->area.x0 < p.area.x1) {
            column(t, area, p, *q, lim);
          }
        }
      }
    }
    detail::paint(out, area);
  }
};

// --- Passes 1 and 2 ----------------------------------------------------------

enum class Dir : std::uint8_t { nw, sw };

/// bridgeLimCheckFunc: the first tile of kind `search` in `area` (other than
/// `self`) at a violating NW or SW corner, closer than sqrt(`sq`) when it is
/// strictly catecorner.
std::optional<db::TileView> lim_violator(const db::Plane& plane, const db::Rect& area, const db::TileView& self,
                                         Dir dir, tech::TileType check, const tech::TypeMask& search, std::int64_t sq) {
  std::optional<db::TileView> found;
  plane.enumerate(area, search, [&](const db::TileView& v) {
    if (v.tile == self.tile) {
      return true;
    }
    const bool nw = dir == Dir::nw;
    if (v.split && (detail::right_type(v) == check || (nw ? detail::bottom_type(v) : detail::top_type(v)) == check)) {
      return true;
    }
    const tech::TileType p =
        nw ? detail::facing_type(detail::first_neighbor(plane, v, db::Side::top), db::Side::top)
           : detail::facing_type(detail::first_neighbor(plane, v, db::Side::bottom), db::Side::bottom);
    const tech::TileType q = nw ? detail::facing_type(detail::last_neighbor(plane, v, db::Side::left), db::Side::left)
                                : detail::facing_type(detail::first_neighbor(plane, v, db::Side::left), db::Side::left);
    if (p != check || q != check) {
      return true;
    }
    const std::int64_t dx = std::int64_t{v.area.x0} - self.area.x1;
    const std::int64_t dy = nw ? std::int64_t{self.area.y0} - v.area.y1 : std::int64_t{v.area.y0} - self.area.y1;
    if (dx > 0 && dy > 0 && dx * dx + dy * dy >= sq) {
      return true;
    }
    found = v;
    return false;
  });
  return found;
}

struct LimBridger {
  const db::Plane& scan;  // the plane enumerated and searched (P with ON, a snapshot with OFF)
  MaskPlane& out;         // P
  const Limits& limits;
  db::Coord spacing;
  db::Coord width;
  bool se_bottom_right;  // Q-MASK-3 (pass 1)

  void paint2(const db::Rect& a, const db::Rect& b) const {
    detail::paint(out, a);
    detail::paint(out, b);
  }

  void three(const db::Rect& c3) const {
    detail::paint(out, c3);
    limits.erase(out, c3);
  }

  void pass1_ne(const db::TileView& t, const db::TileView& v) const {
    const db::Coord r = t.area.x1, tp = t.area.y1, vl = v.area.x0, vb = v.area.y0, w = width;
    const db::Rect c1{std::min(vl, r), std::min(vb, tp - w), std::max(r, vl + w), std::max(tp, vb)};
    if (limits.free(c1)) {
      paint2(db::Rect{c1.x0, c1.y0, c1.x1, tp}, db::Rect{vl, c1.y0, c1.x1, c1.y1});
      return;
    }
    const db::Rect c2{std::min(vl, r - w), std::min(vb, tp), std::max(r, vl), std::max(tp, vb + w)};
    if (limits.free(c2)) {
      paint2(db::Rect{c2.x0, vb, c2.x1, c2.y1}, db::Rect{c2.x0, c2.y0, r, c2.y1});
      return;
    }
    three(db::Rect{std::min(vl, r - w), std::min(vb, tp - w), std::max(r, vl + w), std::max(tp, vb + w)});
  }

  void pass1_se(const db::TileView& t, const db::TileView& v) const {
    const db::Coord r = t.area.x1, b = t.area.y0, vl = v.area.x0, vt = v.area.y1, w = width;
    const db::Rect c1{std::min(vl, r - w), std::min(b, vt - w), std::max(r, vl), std::max(vt, b)};
    if (limits.free(c1)) {
      paint2(db::Rect{c1.x0, c1.y0, r, c1.y1}, db::Rect{c1.x0, c1.y0, c1.x1, vt});
      return;
    }
    const db::Rect c2{std::min(vl, r), std::min(b, vt), std::max(r, vl + w), std::max(vt, b + w)};
    if (limits.free(c2)) {
      paint2(db::Rect{vl, c2.y0, c2.x1, c2.y1}, db::Rect{c2.x0, b, c2.x1, c2.y1});
      return;
    }
    three(db::Rect{std::min(vl, r - w), std::min(b, vt - w), std::max(r, vl + w), std::max(vt, b + w)});
  }

  /// cifBridgeLimFunc1 (solid tiles).
  void solid_tile(const db::TileView& t) const {
    const detail::TileCorners c = detail::tile_corners(scan, t);
    const db::Coord r = t.area.x1;
    const std::int64_t sq = std::int64_t{spacing} * spacing;
    if (detail::ne_corner(c, tech::kSpace)) {
      const db::Rect s{r, t.area.y1, r + spacing, t.area.y1 + spacing};
      if (const auto v = lim_violator(scan, s, t, Dir::sw, tech::kSpace, detail::solid_mask(), sq)) {
        pass1_ne(t, *v);
      }
    }
    if (detail::se_corner(c, tech::kSpace, se_bottom_right)) {
      const db::Rect s{r, t.area.y0 - spacing, r + spacing, t.area.y0};
      if (const auto v = lim_violator(scan, s, t, Dir::nw, tech::kSpace, detail::solid_mask(), sq)) {
        pass1_se(t, *v);
      }
    }
  }

  /// One of pass 2's three candidates in order; the third erases limits.
  void pass2(const db::Rect& a1, const db::Rect& a2, const db::Rect& a3) const {
    if (limits.free(a1)) {
      detail::paint(out, a1);
    } else if (limits.free(a2)) {
      detail::paint(out, a2);
    } else {
      three(a3);
    }
  }

  /// cifBridgeLimFunc2 (space tiles).
  void space_tile(const db::TileView& t) const {
    const detail::TileCorners c = detail::tile_corners(scan, t);
    const db::Coord r = t.area.x1, w = width;
    const std::int64_t sq = std::int64_t{w} * w;
    if (detail::ne_corner(c, kMaskSolid)) {
      const db::Coord tp = t.area.y1;
      const db::Rect s{r, tp, r + w, tp + w};
      if (const auto v = lim_violator(scan, s, t, Dir::sw, kMaskSolid, detail::space_mask(), sq)) {
        const db::Coord vl = v->area.x0, vb = v->area.y0;
        pass2(db::Rect{vl - w, vb - w, r, tp}, db::Rect{vl, vb, r + w, tp + w},
              db::Rect{vl - w, vb - w, r + w, tp + w});
      }
    }
    if (detail::se_corner(c, kMaskSolid, true)) {
      const db::Coord b = t.area.y0;
      const db::Rect s{r, b - w, r + w, b};
      if (const auto v = lim_violator(scan, s, t, Dir::nw, kMaskSolid, detail::space_mask(), sq)) {
        const db::Coord vl = v->area.x0, vt = v->area.y1;
        pass2(db::Rect{vl, b - w, r + w, vt}, db::Rect{vl - w, b, r, vt + w}, db::Rect{vl - w, b - w, r + w, vt + w});
      }
    }
  }
};

/// Enumerates `plane` (which the visitor may paint, as Magic does with
/// magic-compat ON), stopping after a generous visit budget so that a plane
/// changing under its own search cannot loop for ever.
void scan_tiles(const db::Plane& plane, const tech::TypeMask& kind,
                const std::function<void(const db::TileView&)>& fn) {
  const std::size_t budget = 64 * plane.tile_count() + 4096;
  std::size_t visits = 0;
  plane.enumerate(db::whole_plane(), kind, [&](const db::TileView& t) {
    fn(t);
    return ++visits < budget;
  });
}

}  // namespace

diag::Status op_bridge_lim(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const db::Coord spacing = args.op.distance;
  const db::Coord width = detail::value32(args.op, bridge_value::width);
  const Limits limits{args, ctx};
  ScratchPlane next{*ctx.pool};
  const Widener widen{out.plane(), *next, limits, width};
  out.plane().enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
    widen.tile(t);
    detail::count_tile(ctx);
    return true;
  });
  // Q-MASK-228: ON scans the plane being painted; OFF a snapshot of pass 0.
  ScratchPlane snapshot{*ctx.pool};
  const bool snap = q_mask_228_snapshot(ctx.compat);
  if (snap) {
    snapshot->copy_from(*next);
  }
  const db::Plane& scan = snap ? snapshot->plane() : next->plane();
  const LimBridger b{scan, *next, limits, spacing, width, q_mask_3_se_bottom_right(ctx.compat)};
  scan_tiles(scan, detail::solid_mask(), [&](const db::TileView& t) { b.solid_tile(t); });
  scan_tiles(scan, detail::space_mask(), [&](const db::TileView& t) { b.space_tile(t); });
  out.swap(*next);
  return diag::Done{};
}

}  // namespace hefesto::mask
