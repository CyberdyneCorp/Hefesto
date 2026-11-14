// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 4, region and shape filters (capability
// mask-output-operators "maxrect", "orthogonal", "not-square", "overlapping,
// nonoverlapping, interacting, noninteracting"; design D6, Q-MASK-7, 21):
// CIFOP_MAXRECT, CIFOP_MANHATTAN, CIFOP_NOTSQUARE and CIFOP_INTERACT.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c
// (cifRectBoundingBox, cifOrthogonalFunc, cifNotSquareFunc,
// cifInteractingRegions) and utils/maxrect.c (genCanonicalMaxwidth,
// FindMaxRects, FindMaxRectangle2), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include <algorithm>

#include "mask/ops/kernel.hpp"

namespace hefesto::mask {

namespace {

// --- maxrect ------------------------------------------------------------------

/// FindMaxRects: splits each candidate overlapping obstacle `o` into its
/// parts above, below, left and right of it (each at least 1 thick).
void split_candidates(std::vector<db::Rect>& candidates, const db::Rect& o) {
  std::vector<db::Rect> next;
  for (const db::Rect& sr : candidates) {
    if (!db::overlap(sr, o)) {
      next.push_back(sr);
      continue;
    }
    if (o.y1 < db::kPlaneMax && sr.y1 >= o.y1 + 1) {
      next.push_back(db::Rect{sr.x0, o.y1, sr.x1, sr.y1});
    }
    if (o.y0 > db::kPlaneMin && sr.y0 <= o.y0 - 1) {
      next.push_back(db::Rect{sr.x0, sr.y0, sr.x1, o.y0});
    }
    if (o.x0 > db::kPlaneMin && sr.x0 <= o.x0 - 1) {
      next.push_back(db::Rect{sr.x0, sr.y0, o.x0, sr.y1});
    }
    if (o.x1 < db::kPlaneMax && sr.x1 >= o.x1 + 1) {
      next.push_back(db::Rect{o.x1, sr.y0, sr.x1, sr.y1});
    }
  }
  candidates = std::move(next);
}

/// FindMaxRectangle2 over the region whose tiles are marked processed: the
/// largest-area candidate (the first on ties), or the start tile.
db::Rect max_rectangle(const db::Plane& plane, const db::Rect& bbox, const db::TileView& start,
                       const detail::Marks& marks) {
  std::vector<db::Rect> candidates{bbox};
  plane.enumerate(bbox, tech::TypeMask::all(), [&](const db::TileView& t) {
    if (marks.get(t) == detail::Mark::processed) {
      return true;
    }
    split_candidates(candidates, t.area);
    return !candidates.empty();
  });
  std::int64_t best = 0;
  const db::Rect* chosen = nullptr;
  for (const db::Rect& r : candidates) {
    const std::int64_t a = r.width() * r.height();
    if (a > best) {
      best = a;
      chosen = &r;
    }
  }
  return chosen != nullptr ? *chosen : start.area;
}

// --- not-square ---------------------------------------------------------------

/// cifNotSquareFunc's isolation test: no neighbour of the tile's type.
/// magic-compat ON tests every neighbour's bottom type (as Magic); OFF the
/// type each neighbour shows towards the tile.
bool isolated(const db::Plane& plane, const db::TileView& t, bool facing) {
  const tech::TileType own = detail::body_type(t);
  for (const db::Side side : {db::Side::top, db::Side::left, db::Side::bottom, db::Side::right}) {
    for (const db::TileView& n : detail::side_tiles(plane, t, side)) {
      const tech::TileType seen = facing ? detail::facing_type(n, side) : detail::bottom_type(n);
      if (seen == own) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

diag::Status op_maxrect(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const bool external = args.op.text.starts_with("ext");
  ScratchPlane next{*ctx.pool};
  const db::Plane& plane = out.plane();
  detail::Marks marks;
  detail::for_each_region_start(plane, db::whole_plane(), marks, [&](const db::TileView& start) {
    const detail::Region region = detail::collect_region(plane, start, marks, [](const db::TileView&) {});
    const bool whole = external || region.simple;
    detail::paint(*next, whole ? region.bbox : max_rectangle(plane, region.bbox, start, marks));
    detail::count_tile(ctx);
    for (const db::TileView& t : region.tiles) {
      marks.set(t, detail::Mark::ignore);
    }
  });
  out.swap(*next);
  return diag::Done{};
}

diag::Status op_orthogonal(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const bool fill = args.op.text == "fill";
  ScratchPlane next{*ctx.pool};
  out.plane().enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
    if (!t.split || fill) {
      detail::paint(*next, t.area);
    } else {
      detail::erase(*next, t.area);
    }
    detail::count_tile(ctx);
    return true;
  });
  out.swap(*next);
  return diag::Done{};
}

diag::Status op_not_square(const OpArgs& /*args*/, OperatorContext& ctx, MaskPlane& out) {
  const bool keep_triangles = q_mask_7_keeps_triangles(ctx.compat);
  ScratchPlane next{*ctx.pool};
  const db::Plane& plane = out.plane();
  plane.enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
    if (t.split) {
      if (keep_triangles) {
        detail::paint_shape(*next, t, t.area, ctx.compat.magic_compat);
      }
      return true;
    }
    if (isolated(plane, t, !ctx.compat.magic_compat) && t.area.width() == t.area.height()) {
      return true;
    }
    detail::paint(*next, t.area);
    detail::count_tile(ctx);
    return true;
  });
  out.swap(*next);
  return diag::Done{};
}

diag::Status op_interacting(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  ScratchPlane next{*ctx.pool};
  const db::Plane& plane = out.plane();
  detail::Marks marks;
  // Regions are seeded from the tiles overlapping the search area as Magic
  // passes it: in internal units, unscaled.
  detail::for_each_region_start(plane, args.area, marks, [&](const db::TileView& start) {
    bool interacts = false;
    const detail::Region region = detail::collect_region(plane, start, marks, [&](const db::TileView& t) {
      if (interacts) {
        return;
      }
      const db::Rect r = args.op.interact.touching ? db::expand(t.area, 1) : t.area;
      interacts = detail::search_op_tiles2(args, ctx, r, nullptr,
                                           [](const db::TileView&, const detail::Source&) { return true; });
    });
    if (args.op.interact.inverted) {
      interacts = !interacts;
    }
    for (const db::TileView& t : region.tiles) {
      if (interacts) {
        detail::paint(*next, t.area);
      }
      marks.set(t, detail::Mark::ignore);
    }
  });
  out.swap(*next);
  return diag::Done{};
}

}  // namespace hefesto::mask
