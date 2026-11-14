// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 3, bridge (capability mask-output-operators "bridge";
// design D6, Q-MASK-3, Q-MASK-227): CIFOP_BRIDGE.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGenLayer's
// CIFOP_BRIDGE case, cifBridgeFunc1, cifBridgeFunc2, cifBridgeCheckFunc),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "mask/ops/bridge.hpp"

namespace hefesto::mask {

namespace detail {

TileCorners tile_corners(const db::Plane& plane, const db::TileView& t) {
  TileCorners c;
  const std::vector<db::TileView> right = side_tiles(plane, t, db::Side::right);
  const std::vector<db::TileView> top = side_tiles(plane, t, db::Side::top);
  const std::vector<db::TileView> bottom = side_tiles(plane, t, db::Side::bottom);
  if (!right.empty()) {
    c.tr = right.back();
    c.rbot = right.front();
  }
  if (!top.empty()) {
    c.rt = top.back();
  }
  if (!bottom.empty()) {
    c.lb = bottom.front();
    c.bright = bottom.back();
  }
  return c;
}

bool ne_corner(const TileCorners& c, tech::TileType k) {
  return facing_type(c.tr, db::Side::right) == k && facing_type(c.rt, db::Side::top) == k;
}

bool se_corner(const TileCorners& c, tech::TileType k, bool bottom_right) {
  return facing_type(c.rbot, db::Side::right) == k &&
         facing_type(bottom_right ? c.bright : c.lb, db::Side::bottom) == k;
}

}  // namespace detail

namespace {

enum class Dir : std::uint8_t { nw, sw };

/// cifBridgeCheckFunc's BRIDGE_NW case for tile `v`.
bool nw_violator(const db::Plane& plane, const db::Rect& area, const db::TileView& v, tech::TileType check) {
  if (v.area.x0 <= area.x0 || v.area.y1 >= area.y1) {
    return false;  // NW corner outside the search area
  }
  const tech::TileType p = detail::facing_type(detail::first_neighbor(plane, v, db::Side::top), db::Side::top);
  if (p == detail::top_type(v) || detail::right_type(v) == check || detail::bottom_type(v) == check) {
    return false;  // a false corner, or a split tile with its SE corner clipped
  }
  const tech::TileType q = detail::facing_type(detail::last_neighbor(plane, v, db::Side::left), db::Side::left);
  return p == check && q == check;
}

/// cifBridgeCheckFunc's BRIDGE_SW case for tile `v` (note Magic's OR).
bool sw_violator(const db::Plane& plane, const db::Rect& area, const db::TileView& v, tech::TileType check) {
  if (v.area.x0 <= area.x0 || v.area.y0 <= area.y0) {
    return false;  // SW corner outside the search area
  }
  const tech::TileType p = detail::facing_type(detail::first_neighbor(plane, v, db::Side::bottom), db::Side::bottom);
  if (p == detail::bottom_type(v) || detail::right_type(v) == check || detail::top_type(v) == check) {
    return false;  // a false corner, or a split tile with its NE corner clipped
  }
  const tech::TileType q = detail::facing_type(detail::first_neighbor(plane, v, db::Side::left), db::Side::left);
  return p == check || q == check;
}

/// cifBridgeCheckFunc: the first tile of kind `search` in `area` (other than
/// `self`) whose NW or SW corner makes a catecorner violation.
std::optional<db::TileView> violator(const db::Plane& plane, const db::Rect& area, const db::TileView& self, Dir dir,
                                     tech::TileType check, const tech::TypeMask& search) {
  std::optional<db::TileView> found;
  plane.enumerate(area, search, [&](const db::TileView& v) {
    if (v.tile == self.tile) {
      return true;
    }
    const bool hit = dir == Dir::nw ? nw_violator(plane, area, v, check) : sw_violator(plane, area, v, check);
    if (hit) {
      found = v;
    }
    return !hit;
  });
  return found;
}

struct Bridger {
  const db::Plane& plane;
  MaskPlane& out;
  db::Coord spacing;
  db::Coord width;
  std::int32_t grid_limit;
  bool se_bottom_right;  // Q-MASK-3 (pass A)

  void ne(const db::TileView& t, bool space_pass, const db::Rect& search) const {
    const tech::TileType check = space_pass ? kMaskSolid : tech::kSpace;
    const tech::TypeMask& kind = space_pass ? detail::space_mask() : detail::solid_mask();
    if (const auto v = violator(plane, search, t, Dir::sw, check, kind)) {
      const db::Rect gap{t.area.x1, t.area.y1, v->area.x0, v->area.y0};
      detail::paint(out, bridge_expand(gap, width, space_pass, grid_limit));
    }
  }

  void se(const db::TileView& t, bool space_pass, const db::Rect& search) const {
    const tech::TileType check = space_pass ? kMaskSolid : tech::kSpace;
    const tech::TypeMask& kind = space_pass ? detail::space_mask() : detail::solid_mask();
    if (const auto v = violator(plane, search, t, Dir::nw, check, kind)) {
      const db::Rect gap{t.area.x1, v->area.y1, v->area.x0, t.area.y0};
      detail::paint(out, bridge_expand(gap, width, space_pass, grid_limit));
    }
  }

  /// cifBridgeFunc1 (outer corners of solid tiles).
  void solid_tile(const db::TileView& t) const {
    const detail::TileCorners c = detail::tile_corners(plane, t);
    const db::Coord r = t.area.x1;
    if (detail::ne_corner(c, tech::kSpace)) {
      ne(t, false, db::Rect{r - width, t.area.y1 - width, r + spacing, t.area.y1 + spacing});
    }
    if (detail::se_corner(c, tech::kSpace, se_bottom_right)) {
      se(t, false, db::Rect{r - width, t.area.y0 - spacing, r + spacing, t.area.y0 + width});
    }
  }

  /// cifBridgeFunc2 (inner corners: space tiles).
  void space_tile(const db::TileView& t) const {
    const detail::TileCorners c = detail::tile_corners(plane, t);
    const db::Coord r = t.area.x1;
    if (detail::ne_corner(c, kMaskSolid)) {
      ne(t, true, db::Rect{r - spacing, t.area.y1 - spacing, r + width, t.area.y1 + width});
    }
    if (detail::se_corner(c, kMaskSolid, true)) {
      se(t, true, db::Rect{r - spacing, t.area.y0 - width, r + width, t.area.y0 + spacing});
    }
  }
};

}  // namespace

diag::Status op_bridge(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  ScratchPlane next{*ctx.pool};
  (void)next->unite(out);
  const db::Plane& plane = out.plane();
  const Bridger b{plane,
                  *next,
                  detail::value32(args.op, 0),
                  detail::value32(args.op, 1),
                  detail::grid_limit_of(args),
                  q_mask_3_se_bottom_right(ctx.compat)};
  plane.enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
    b.solid_tile(t);
    return true;
  });
  plane.enumerate(db::whole_plane(), detail::space_mask(), [&](const db::TileView& t) {
    b.space_tile(t);
    return true;
  });
  out.swap(*next);
  return diag::Done{};
}

}  // namespace hefesto::mask
