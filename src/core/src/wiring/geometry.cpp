// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The exact geometry of the wiring tool (wiring-tool "Wire type selection",
// "Wire legs", "Contact placement"): leg and contact rectangles, the boxes
// they leave, and the chunk search. Ported from Magic VLSI 8.3.694
// (fc5daa9e) wiring/wireOps.c (WirePickType, WireAddLeg, WireAddContact) and
// select/selCreate.c:569-671 (selFindChunk), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
// Integer division is C's (truncation toward zero), as in Magic.
// Owner: WIRE.

#include <algorithm>
#include <utility>

#include "hefesto/db/transform.hpp"
#include "hefesto/wiring/wiring.hpp"

namespace hefesto::wiring {

namespace {

// WireAddLeg's direction choice: the distance from the point to the box on
// each axis (0 within the span); horizontal when x wins.
bool choose_horizontal(const db::Rect& r, const db::Point& p) {
  db::Coord dx = p.x - r.x1;
  if (dx < 0) {
    dx = std::max(r.x0 - p.x, 0);
  }
  db::Coord dy = p.y - r.y1;
  if (dy < 0) {
    dy = std::max(r.y0 - p.y, 0);
  }
  return dx > dy;
}

// "If the rect height is not the same as WireWidth, then center the new
// wire segment on the rect": a w x w square at the rectangle's middle.
db::Rect recenter(const db::Rect& r, db::Coord w) {
  const db::Coord h = w / 2;
  const db::Coord mx = (r.x1 + r.x0) / 2;
  const db::Coord my = (r.y1 + r.y0) / 2;
  return db::Rect{mx - h, my - h, mx - h + w, my - h + w};
}

// The leg's position across its axis: centered on the point, clamped to the
// box ("hook the segment up to the nearest point along the box").
db::Coord across(db::Coord p, db::Coord lo, db::Coord hi, db::Coord w) {
  db::Coord v = p - w / 2;
  if (v < lo) {
    v = lo;
  } else if (v > hi - w) {
    v = hi - w;
  }
  return v;
}

std::optional<LegPlan> horizontal_leg(db::Rect r, const db::Point& p, db::Coord w) {
  if (r.height() != w) {
    r = recenter(r, w);
  }
  const db::Coord h = w / 2;
  LegPlan plan;
  if (p.x > r.x1) {
    plan.area.x0 = r.x0;
    plan.area.x1 = p.x + h;
    plan.direction = LegDirection::east;
  } else if (p.x < r.x0) {
    plan.area.x1 = r.x1;
    plan.area.x0 = p.x - h;
    plan.direction = LegDirection::west;
  } else {
    return std::nullopt;  // nothing to paint
  }
  plan.area.y0 = across(p.y, r.y0, r.y1, w);
  plan.area.y1 = plan.area.y0 + w;
  return plan;
}

std::optional<LegPlan> vertical_leg(db::Rect r, const db::Point& p, db::Coord w) {
  if (r.width() != w) {
    r = recenter(r, w);
  }
  const db::Coord h = w / 2;
  LegPlan plan;
  if (p.y > r.y1) {
    plan.area.y0 = r.y0;
    plan.area.y1 = p.y + h;
    plan.direction = LegDirection::north;
  } else if (p.y < r.y0) {
    plan.area.y1 = r.y1;
    plan.area.y0 = p.y - h;
    plan.direction = LegDirection::south;
  } else {
    return std::nullopt;
  }
  plan.area.x0 = across(p.x, r.x0, r.x1, w);
  plan.area.x1 = plan.area.x0 + w;
  return plan;
}

// Grows [lo, hi) to `total` around its middle when it is shorter.
void grow_axis(db::Coord& lo, db::Coord& hi, db::Coord total) {
  if (hi - lo < total) {
    lo -= (total - (hi - lo)) / 2;
    hi = lo + total;
  }
}

// Keeps only the `total`-long part of the contact area at the leg's end.
void trim_to_leg_end(db::Rect& a, LegDirection dir, db::Coord total) {
  switch (dir) {
    case LegDirection::north:
      a.y0 = std::max(a.y0, a.y1 - total);
      break;
    case LegDirection::south:
      a.y1 = std::min(a.y1, a.y0 + total);
      break;
    case LegDirection::east:
      a.x0 = std::max(a.x0, a.x1 - total);
      break;
    case LegDirection::west:
      a.x1 = std::min(a.x1, a.x0 + total);
      break;
    case LegDirection::none:
      break;
  }
}

// A contact layer's extension: `ext` along one axis and its surround along
// the other. Without a leg direction Magic's switch leaves the rectangle as
// it is.
db::Rect extension(const db::Rect& t, LegDirection dir, bool along_y, db::Coord ext, db::Coord sur) {
  if (dir == LegDirection::none) {
    return t;
  }
  return along_y ? db::Rect{t.x0 - sur, t.y0 - ext, t.x1 + sur, t.y1 + ext}
                 : db::Rect{t.x0 - ext, t.y0 - sur, t.x1 + ext, t.y1 + sur};
}

struct ChunkSearch {
  const db::Plane& plane;
  const tech::TypeMask& wrong;
  const db::Rect& contained;
  db::Coord best_min = 0;
  db::Coord best_max = 0;
  std::optional<db::Rect> best;

  static constexpr int kMaxRecursion = 22;  // MAX_RECURSION_LEVEL

  std::optional<db::Rect> wrong_tile(const db::Rect& area) const {
    std::optional<db::Rect> found;
    plane.enumerate(area, wrong, [&](const db::TileView& t) {
      found = t.area;
      return false;
    });
    return found;
  }

  void run(const db::Rect& search, int level) {
    if (level == kMaxRecursion) {
      return;
    }
    db::Coord mn = search.x1 - search.x0;
    db::Coord mx = search.y1 - search.y0;
    if (mn > mx) {
      std::swap(mn, mx);
    }
    if (mn < best_min || (mn == best_min && mx <= best_max)) {
      return;
    }
    const std::optional<db::Rect> bad = wrong_tile(search);
    if (!bad) {
      best_min = mn;
      best_max = mx;
      best = search;
      return;
    }
    // Exclude the wrong material on x, then on y, when it lies beside the
    // contained area on that axis.
    db::Rect smaller = search;
    if (exclude(bad->x0, bad->x1, contained.x0, contained.x1, smaller.x0, smaller.x1)) {
      run(smaller, level + 1);
    }
    smaller = search;
    if (exclude(bad->y0, bad->y1, contained.y0, contained.y1, smaller.y0, smaller.y1)) {
      run(smaller, level + 1);
    }
  }

  // Shrinks [lo, hi) to leave out the wrong material [blo, bhi) when it lies
  // beside the contained span [clo, chi); false when it overlaps it.
  static bool exclude(db::Coord blo, db::Coord bhi, db::Coord clo, db::Coord chi, db::Coord& lo, db::Coord& hi) {
    if (blo >= chi) {
      hi = blo;
      return true;
    }
    if (bhi <= clo) {
      lo = bhi;
      return true;
    }
    return false;
  }
};

}  // namespace

std::optional<LegPlan> plan_leg(const db::Rect& box, const db::Point& point, db::Coord width, LegMode mode) {
  const bool horizontal = mode == LegMode::horizontal || (mode == LegMode::choose && choose_horizontal(box, point));
  return horizontal ? horizontal_leg(box, point, width) : vertical_leg(box, point, width);
}

db::Rect leg_tip_box(const LegPlan& leg, const db::Rect& chunk, db::Coord width) {
  db::Rect r = leg.area;
  switch (leg.direction) {
    case LegDirection::north:
      r.y0 = std::min(r.y0, chunk.y0);
      r.y0 = r.y1 - r.y0 > width ? r.y1 - width : r.y0;
      break;
    case LegDirection::south:
      r.y1 = std::max(r.y1, chunk.y1);
      r.y1 = r.y1 - r.y0 > width ? r.y0 + width : r.y1;
      break;
    case LegDirection::east:
      r.x0 = std::min(r.x0, chunk.x0);
      r.x0 = r.x1 - r.x0 > width ? r.x1 - width : r.x0;
      break;
    case LegDirection::west:
      r.x1 = std::max(r.x1, chunk.x1);
      r.x1 = r.x1 - r.x0 > width ? r.x0 + width : r.x1;
      break;
    case LegDirection::none:
      break;
  }
  return r;
}

db::Rect pick_box(const db::Rect& point_area, const db::Rect& chunk, db::Coord width) {
  db::Rect b;
  if ((width & 1) != 0) {
    b = db::expand(point_area, width / 2);
  } else {
    b.x0 = point_area.x0 - width / 2;
    b.y0 = point_area.y0 - width / 2;
    b.x1 = b.x0 + width;
    b.y1 = b.y0 + width;
  }
  if (b.x0 < chunk.x0) {
    b.x0 = chunk.x0;
    b.x1 = b.x0 + width;
  }
  if (b.y0 < chunk.y0) {
    b.y0 = chunk.y0;
    b.y1 = b.y0 + width;
  }
  if (b.x1 > chunk.x1) {
    b.x1 = chunk.x1;
    b.x0 = b.x1 - width;
  }
  if (b.y1 > chunk.y1) {
    b.y1 = chunk.y1;
    b.y0 = b.y1 - width;
  }
  return b;
}

std::optional<ContactPlan> plan_contact(const WiringContact& c, tech::TileType old_type, const db::Rect& old_leg,
                                        LegDirection old_dir, db::Coord new_width, const db::Transform& to_edit) {
  if (c.layer1 != old_type && c.layer2 != old_type) {
    return std::nullopt;
  }
  const bool up = c.layer1 == old_type;
  const db::Coord old_overlap = up ? c.surround1 : c.surround2;
  const db::Coord new_overlap = up ? c.surround2 : c.surround1;
  const db::Coord total = std::max(c.size + 2 * old_overlap, new_width - 2 * new_overlap);

  db::Rect area = old_leg;
  grow_axis(area.x0, area.x1, total);
  grow_axis(area.y0, area.y1, total);
  trim_to_leg_end(area, old_dir, total);

  ContactPlan plan;
  plan.changed = db::apply(to_edit, area);
  const db::Rect cut = db::expand(plan.changed, -old_overlap);
  const auto add = [&plan](tech::TileType type, const db::Rect& r) {
    plan.paint.push_back(ContactPaint{type, r});
    plan.changed = db::include(plan.changed, r);
  };
  add(c.type, cut);
  if (c.surround1 != 0) {
    add(c.layer1, db::expand(cut, c.surround1));
  }
  if (c.surround2 != 0) {
    add(c.layer2, db::expand(cut, c.surround2));
  }
  const bool vertical_leg = old_dir == LegDirection::north || old_dir == LegDirection::south;
  const bool ext1_along_y = vertical_leg == up;
  if (c.extend1 != 0) {
    add(c.layer1, extension(cut, old_dir, ext1_along_y, c.extend1, c.surround1));
  }
  if (c.extend2 != 0) {
    add(c.layer2, extension(cut, old_dir, !ext1_along_y, c.extend2, c.surround2));
  }

  const db::Rect root_cut = db::expand(area, -old_overlap);
  plan.select.push_back(ContactPaint{c.type, root_cut});
  if (c.surround1 != 0) {
    plan.select.push_back(ContactPaint{c.layer1, db::expand(root_cut, c.surround1)});
  }
  if (c.surround2 != 0) {
    plan.select.push_back(ContactPaint{c.layer2, db::expand(root_cut, c.surround2)});
  }
  plan.box = db::expand(root_cut, new_overlap);
  return plan;
}

std::optional<db::Rect> find_chunk(const db::Plane& plane, const tech::TypeMask& wrong, const db::Rect& search,
                                   const db::Rect& contained, std::optional<db::Rect> best) {
  ChunkSearch s{plane, wrong, contained, 0, 0, std::nullopt};
  if (best) {
    s.best_min = static_cast<db::Coord>(std::min(best->width(), best->height()));
    s.best_max = static_cast<db::Coord>(std::max(best->width(), best->height()));
    s.best = best;
  }
  s.run(search, 0);
  return s.best;
}

}  // namespace hefesto::wiring
