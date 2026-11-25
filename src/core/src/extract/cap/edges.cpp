// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Sidewall edges and nearest-neighbour shadowing (ExtCouple.c
// extBasicCouple, extAddCouple :1105-1245, extWalkTop/Bottom/Right/Left
// :2449-2885, extSideLeft/Right/Top/Bottom and extSideCommon :2886-3134;
// ExtPerim.c extEnumTilePerim; capability extract-capacitance "Sidewall
// edge enumeration", "Nearest-neighbor shadowing along an edge", "Sidewall
// coupling value" with Q-EXTRACT-20).
//
// The four extWalk* functions are one walk over a direction table: the
// outward step to the next row, the step along a row, and the row tests.
//
// Owner: EXTCAP.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "edge_pass.hpp"

namespace hefesto::extract::cap {

namespace {

bool horizontal(Dir d) noexcept {
  return d == Dir::top || d == Dir::bottom;
}

db::Coord lo_of(const db::Rect& r, Dir d) noexcept {
  return horizontal(d) ? r.x0 : r.y0;
}

db::Coord hi_of(const db::Rect& r, Dir d) noexcept {
  return horizontal(d) ? r.x1 : r.y1;
}

void set_lo(db::Rect& r, Dir d, db::Coord v) noexcept {
  (horizontal(d) ? r.x0 : r.y0) = v;
}

void set_hi(db::Rect& r, Dir d, db::Coord v) noexcept {
  (horizontal(d) ? r.x1 : r.y1) = v;
}

/// The walk of one direction (extWalkTop and its three rotations).
struct WalkOps {
  Stitch out;    // first step from b_outside, then to the next row
  Stitch along;  // back into the area and along a row
  bool (*more_rows)(const db::Rect& t, const db::Rect& a);
  bool (*past_start)(const db::Rect& t, const db::Rect& a);
  bool (*in_row)(const db::Rect& t, const db::Rect& a);
  tech::TileType (*facing)(const db::TileView& t);
  void (*far_bound)(db::Rect& a, const db::Rect& t);  // the area ends at the found tile
};

const WalkOps& ops(Dir d) {
  static const WalkOps kTop{
      Stitch::rt, Stitch::bl, [](const db::Rect& t, const db::Rect& a) { return t.y0 < a.y1; },
      [](const db::Rect& t, const db::Rect& a) { return t.x0 >= a.x1; },
      [](const db::Rect& t, const db::Rect& a) { return t.x1 > a.x0; }, &bottom_type,
      [](db::Rect& a, const db::Rect& t) { a.y1 = t.y0; }};
  static const WalkOps kBottom{
      Stitch::lb, Stitch::tr, [](const db::Rect& t, const db::Rect& a) { return t.y1 > a.y0; },
      [](const db::Rect& t, const db::Rect& a) { return t.x1 <= a.x0; },
      [](const db::Rect& t, const db::Rect& a) { return t.x0 < a.x1; }, &top_type,
      [](db::Rect& a, const db::Rect& t) { a.y0 = t.y1; }};
  static const WalkOps kRight{
      Stitch::tr, Stitch::lb, [](const db::Rect& t, const db::Rect& a) { return t.x0 < a.x1; },
      [](const db::Rect& t, const db::Rect& a) { return t.y0 >= a.y1; },
      [](const db::Rect& t, const db::Rect& a) { return t.y1 > a.y0; }, &left_type,
      [](db::Rect& a, const db::Rect& t) { a.x1 = t.x0; }};
  static const WalkOps kLeft{
      Stitch::bl, Stitch::rt, [](const db::Rect& t, const db::Rect& a) { return t.x1 > a.x0; },
      [](const db::Rect& t, const db::Rect& a) { return t.y1 <= a.y0; },
      [](const db::Rect& t, const db::Rect& a) { return t.y0 < a.y1; }, &right_type,
      [](db::Rect& a, const db::Rect& t) { a.x0 = t.x1; }};
  switch (d) {
    case Dir::top:
      return kTop;
    case Dir::bottom:
      return kBottom;
    case Dir::right:
      return kRight;
    case Dir::left:
      break;
  }
  return kLeft;
}

/// extGetBoundaryRegions: the split sides Magic reads for the inside tile
/// and the far tile (the far tile's top/bottom side follows the inside
/// tile's diagonal, as in Magic).
std::pair<Node, Node> boundary_regions(const EdgeState& es, const db::TileView& far) {
  const Context& ctx = *es.ctx;
  const Boundary& b = es.bp;
  const bool dir = split_direction(b.inside);
  bool in_right = false;
  bool far_right = false;
  switch (b.dir) {
    case Dir::left:
      far_right = true;
      break;
    case Dir::right:
      in_right = true;
      break;
    case Dir::top:
      in_right = dir;
      far_right = !dir;
      break;
    case Dir::bottom:
      in_right = !dir;
      far_right = dir;
      break;
  }
  return {ctx.node(es.plane, b.inside, b.inside.split && in_right), ctx.node(es.plane, far, far.split && far_right)};
}

/// extSideCommon: the sidewall rules of the edge for one near/far pair.
/// `sep + offset` is formed in single precision (Q-EXTRACT-20, both modes).
void side_common(const EdgeState& es, std::size_t rinside, std::size_t rfar, tech::TileType near,
                 tech::TileType far, std::int64_t overlap, std::int32_t sep) {
  if (es.couple_list == nullptr) {
    return;
  }
  for (const EdgeRule& e : *es.couple_list) {
    if (e.near_types.test(near) && e.far_types.test(far)) {
      const float denominator = static_cast<float>(sep) + e.offset;
      const double swcap = (e.cap * static_cast<double>(overlap)) / static_cast<double>(denominator);
      es.ctx->add_coupling(rinside, rfar, swcap);
    }
  }
}

/// The near tiles along the far tile's side facing the edge, in Magic's
/// order, and the types each pair uses (extGetBoundaryTypes2).
struct FarSide {
  db::Side side;
  bool reverse;
  std::int32_t sep;
};

FarSide far_side(const Boundary& b, const db::Rect& far) {
  switch (b.dir) {
    case Dir::left:  // TR(tpfar) then LB: its right side, top to bottom
      return {db::Side::right, true, b.segment.x0 - far.x1};
    case Dir::right:  // BL(tpfar) then RT: its left side, bottom to top
      return {db::Side::left, false, far.x0 - b.segment.x1};
    case Dir::top:  // LB(tpfar) then TR: its bottom side, left to right
      return {db::Side::bottom, false, far.y0 - b.segment.y1};
    case Dir::bottom:
      break;
  }
  return {db::Side::top, true, b.segment.y0 - far.y1};  // RT(tpfar) then BL
}

std::pair<tech::TileType, tech::TileType> near_far_types(Dir d, const db::TileView& near, const db::TileView& far) {
  switch (d) {
    case Dir::left:
      return {left_type(near), right_type(far)};
    case Dir::right:
      return {right_type(near), left_type(far)};
    case Dir::top:
      return {top_type(near), bottom_type(far)};
    case Dir::bottom:
      break;
  }
  return {bottom_type(near), top_type(far)};
}

/// extSideLeft/Right/Top/Bottom for the far tile found by the walk.
void side_far(const EdgeState& es, const db::TileView& far, const Boundary& bloc) {
  const auto [rinside, rfar] = boundary_regions(es, far);
  if (!rfar || rfar == rinside || !rinside) {
    return;
  }
  const FarSide fs = far_side(bloc, far.area);
  const db::Coord lo = std::max(lo_of(bloc.segment, bloc.dir), lo_of(far.area, bloc.dir));
  const db::Coord hi = std::min(hi_of(bloc.segment, bloc.dir), hi_of(far.area, bloc.dir));
  std::vector<db::TileView> nears;
  es.ctx->plane(es.plane).neighbors(far, fs.side, lo, hi, nears);
  if (fs.reverse) {
    std::reverse(nears.begin(), nears.end());
  }
  for (const db::TileView& near : nears) {
    const std::int64_t overlap = std::int64_t{std::min(hi_of(near.area, bloc.dir), hi)} -
                                 std::max(lo_of(near.area, bloc.dir), lo);
    if (overlap > 0) {
      const auto [nt, ft] = near_far_types(bloc.dir, near, far);
      side_common(es, *rinside, *rfar, nt, ft, overlap, fs.sep);
    }
  }
}

void walk(EdgeState& es, const db::Rect& area, const Boundary& bp, const tech::TypeMask& mask);

/// A coupling tile `tp` was found: sidewall to it, fringe over the gap, the
/// blocked substrate fringe, then the two sides of the segment it leaves.
void found(EdgeState& es, const db::Rect& area, const Boundary& bp, const db::TileView& tp,
           const tech::TypeMask& mask) {
  const Dir d = bp.dir;
  const WalkOps& w = ops(d);
  Boundary bloc = bp;
  const bool look_low = lo_of(tp.area, d) > lo_of(bp.segment, d);
  const bool look_high = hi_of(tp.area, d) < hi_of(bp.segment, d);
  if (look_low) {
    set_lo(bloc.segment, d, lo_of(tp.area, d));
  }
  if (look_high) {
    set_hi(bloc.segment, d, hi_of(tp.area, d));
  }
  if (es.kinds.sidewall) {
    side_far(es, tp, bloc);
  }
  db::Rect aloc = area;
  w.far_bound(aloc, tp.area);
  set_lo(aloc, d, lo_of(bloc.segment, d));
  set_hi(aloc, d, hi_of(bloc.segment, d));
  find_overlap(es, aloc);
  remove_subcap(es, bloc, aloc);
  if (look_low) {
    aloc = area;
    set_hi(aloc, d, lo_of(bloc.segment, d));
    set_lo(bloc.segment, d, lo_of(bp.segment, d));
    set_hi(bloc.segment, d, hi_of(aloc, d));
    walk(es, aloc, bloc, mask);
  }
  if (look_high) {
    aloc = area;
    set_lo(aloc, d, hi_of(tp.area, d));
    set_hi(bloc.segment, d, hi_of(bp.segment, d));
    set_lo(bloc.segment, d, lo_of(aloc, d));
    walk(es, aloc, bloc, mask);
  }
}

/// extWalk*: the nearest tile of `mask` in `area`, row by row outward.
void walk(EdgeState& es, const db::Rect& area, const Boundary& bp, const tech::TypeMask& mask) {
  const WalkOps& w = ops(bp.dir);
  const db::Plane& plane = es.ctx->plane(es.plane);
  db::TileView tile = step(plane, bp.outside, w.out);
  while (w.more_rows(tile.area, area)) {
    while (w.past_start(tile.area, area)) {
      tile = step(plane, tile, w.along);
    }
    for (db::TileView tp = tile; w.in_row(tp.area, area); tp = step(plane, tp, w.along)) {
      if (mask.test(w.facing(tp))) {
        found(es, area, bp, tp, mask);
        return;
      }
    }
    tile = step(plane, tile, w.out);
  }
  find_overlap(es, area);
}

const std::vector<EdgeRule>* rules(const std::map<TypePair, std::vector<EdgeRule>>& lists, tech::TileType a,
                                   tech::TileType b) {
  const auto it = lists.find({a, b});
  return it == lists.end() || it->second.empty() ? nullptr : &it->second;
}

tech::TypeMask mask_of(const std::map<TypePair, tech::TypeMask>& masks, tech::TileType a, tech::TileType b) {
  const auto it = masks.find({a, b});
  return it == masks.end() ? tech::TypeMask{} : it->second;
}

db::Rect grown(db::Rect r, Dir d, std::int32_t by) {
  switch (d) {
    case Dir::left:
      r.x0 -= by;
      break;
    case Dir::right:
      r.x1 += by;
      break;
    case Dir::top:
      r.y1 += by;
      break;
    case Dir::bottom:
      r.y0 -= by;
      break;
  }
  return r;
}

/// extAddCouple for one boundary segment of a tile.
void add_couple(const Context& ctx, const Kinds& kinds, tech::PlaneId plane, Boundary bp) {
  const CapTables& t = ctx.tables();
  auto [tin, tout] = boundary_types(bp);
  const ExtractOptions& o = ctx.options();
  if (mask_of(t.side_couple_other_edges, tin, tout).empty() && o.fringehalo && ctx.halo() != 0) {
    return;
  }
  tin = ctx.residue(tin, plane);
  tout = ctx.residue(tout, plane);
  EdgeState es;
  es.ctx = &ctx;
  es.kinds = kinds;
  es.plane = plane;
  es.tin = tin;
  es.tout = tout;
  es.couple_list = rules(t.side_couple, tin, tout);
  es.overlap_list = rules(t.side_overlap, tin, tout);
  if (es.couple_list == nullptr && es.overlap_list == nullptr) {
    return;
  }
  if (ctx.clip()) {
    if (!db::overlap(bp.segment, *ctx.clip())) {
      return;
    }
    bp.segment = geoclip(bp.segment, *ctx.clip());
  }
  es.bp = bp;
  es.fringe_halo = o.fringehalo && ctx.halo() != 0;
  std::int32_t dist_fringe = o.fringehalo ? ctx.halo() : 1;
  if (dist_fringe == 0) {
    dist_fringe = 1;
  }
  es.area = grown(bp.segment, bp.dir, dist_fringe);
  walk(es, grown(bp.segment, bp.dir, ctx.halo()), bp, mask_of(t.side_couple_other_edges, tin, tout));
  if (kinds.corners && o.corners && es.fringe_halo && es.overlap_list != nullptr && horizontal(bp.dir)) {
    add_corner_couple(es);
  }
}

/// The Manhattan sides extEnumTilePerim walks for a tile (two for a split
/// tile; its diagonal boundary is skipped by extAddCouple).
struct PerimSide {
  db::Side side;
  Dir dir;
  bool reverse;
};

constexpr PerimSide kPerim[] = {{db::Side::top, Dir::top, true},
                                {db::Side::bottom, Dir::bottom, false},
                                {db::Side::left, Dir::left, false},
                                {db::Side::right, Dir::right, true}};

bool walks_side(const db::TileView& tile, bool right, Dir d) {
  if (!tile.split) {
    return true;
  }
  const bool skip_bottom = right == split_direction(tile);
  switch (d) {
    case Dir::left:
      return !right;
    case Dir::right:
      return right;
    case Dir::bottom:
      return !skip_bottom;
    case Dir::top:
      break;
  }
  return skip_bottom;
}

tech::TileType outside_type(Dir d, const db::TileView& t) {
  switch (d) {
    case Dir::top:
      return bottom_type(t);
    case Dir::bottom:
      return top_type(t);
    case Dir::left:
      return right_type(t);
    case Dir::right:
      break;
  }
  return left_type(t);
}

db::Rect segment_of(Dir d, const db::Rect& in, const db::Rect& out) {
  switch (d) {
    case Dir::top:
      return {std::max(in.x0, out.x0), in.y1, std::min(in.x1, out.x1), in.y1};
    case Dir::bottom:
      return {std::max(in.x0, out.x0), in.y0, std::min(in.x1, out.x1), in.y0};
    case Dir::left:
      return {in.x0, std::max(in.y0, out.y0), in.x0, std::min(in.y1, out.y1)};
    case Dir::right:
      break;
  }
  return {in.x1, std::max(in.y0, out.y0), in.x1, std::min(in.y1, out.y1)};
}

/// extEnumTilePerim over the sides of `tile` with the side-edge mask.
void enum_perimeter(const Context& ctx, const Kinds& kinds, tech::PlaneId plane, const db::TileView& tile,
                    bool right, const tech::TypeMask& mask) {
  std::vector<db::TileView> out;
  for (const PerimSide& ps : kPerim) {
    if (!walks_side(tile, right, ps.dir)) {
      continue;
    }
    const bool vertical = !horizontal(ps.dir);
    ctx.plane(plane).neighbors(tile, ps.side, vertical ? tile.area.y0 : tile.area.x0,
                               vertical ? tile.area.y1 : tile.area.x1, out);
    if (ps.reverse) {
      std::reverse(out.begin(), out.end());
    }
    for (const db::TileView& tp : out) {
      if (mask.test(outside_type(ps.dir, tp))) {
        add_couple(ctx, kinds, plane, Boundary{tile, tp, segment_of(ps.dir, tile.area, tp.area), ps.dir});
      }
    }
  }
}

}  // namespace

/// extGetBoundaryTypes: the inside and outside types facing the boundary.
std::pair<tech::TileType, tech::TileType> boundary_types(const Boundary& b) {
  switch (b.dir) {
    case Dir::left:
      return {left_type(b.inside), right_type(b.outside)};
    case Dir::right:
      return {right_type(b.inside), left_type(b.outside)};
    case Dir::top:
      return {top_type(b.inside), bottom_type(b.outside)};
    case Dir::bottom:
      break;
  }
  return {bottom_type(b.inside), top_type(b.outside)};
}

Node inside_node(const EdgeState& es) {
  return es.ctx->node(es.plane, es.bp.inside, false);
}

void couple_plane(const Context& ctx, tech::PlaneId plane, const Kinds& kinds) {
  const CapTables& t = ctx.tables();
  if ((t.side_planes & tech::plane_bit(plane)) == 0) {
    return;
  }
  ctx.plane(plane).enumerate(ctx.search_area(), t.side_types[plane], [&](const db::TileView& tile) {
    const tech::TileType type = side_type(tile, tile.right_side);
    enum_perimeter(ctx, kinds, plane, tile, tile.right_side, t.side_edges[type]);
    return true;
  });
}

}  // namespace hefesto::extract::cap
