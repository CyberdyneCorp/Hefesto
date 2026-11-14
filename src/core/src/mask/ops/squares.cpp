// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 2, contact cuts (capability mask-output-operators
// "Contact strip pre-pass for squares and slots", "squares", "squares-grid",
// "slots"; design D6): CIFOP_SQUARES, CIFOP_SQUARES_G and CIFOP_SLOTS.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c
// (cifSquaresStripFunc, cifUnconnectFunc, cifSquaresFillArea,
// cifSlotsFillArea, cifSlotFunc, cifSquareFunc, cifSquareGridFunc),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include <cstdlib>
#include <functional>

#include "mask/ops/kernel.hpp"

namespace hefesto::mask {

namespace {

/// A guard against a grid-limit placement that never settles (Q-MASK-27 OFF
/// can move an edge both ways); Magic's sign-blind ON loop always ends.
constexpr int kMaxGridSteps = 4096;

// --- Placement functions -----------------------------------------------------

/// The result of a placement function: rows (nUp), columns (nAcross) and
/// the lower-left cut.
struct Placement {
  int rows = 0;
  int cols = 0;
  db::Rect cut;
};

struct SquaresData {
  std::int32_t border = 0;
  std::int32_t size = 0;
  std::int32_t sep = 0;
  std::int32_t gridx = 1;
  std::int32_t gridy = 1;
};

SquaresData squares_data(const LayerOp& op) {
  SquaresData s{detail::value32(op, 0), detail::value32(op, 1), detail::value32(op, 2), detail::value32(op, 3, 1),
                detail::value32(op, 4, 1)};
  if (op.values.size() == 4) {
    s.gridy = s.gridx;
  }
  s.gridx = s.gridx > 0 ? s.gridx : 1;
  s.gridy = s.gridy > 0 ? s.gridy : 1;
  return s;
}

/// One axis of cifSquareFunc: the count and first cut origin along an axis
/// [lo, hi], moving `hi` until the origin is on the grid limit.
int square_axis(db::Coord lo, db::Coord& hi, const SquaresData& s, std::int32_t limit, db::Coord& origin,
                const compat::CompatSettings& compat) {
  const std::int32_t pitch = s.size + s.sep;
  for (int step = 0; step < kMaxGridSteps; ++step) {
    const int n = (hi - lo + s.sep - (2 * s.border)) / pitch;
    if (n == 0) {
      return 0;
    }
    origin = (lo + hi + s.sep - (n * pitch)) / 2;
    const std::int32_t delta = limit > 1 ? std::abs(origin) % limit : 0;
    if (delta == 0) {
      return n;
    }
    hi = q_mask_27_shift(hi, origin, delta, compat);
  }
  return 0;
}

/// cifSquareFunc. `area` is updated as Magic's is (the grid-limit steps
/// move its upper edges).
Placement square_func(db::Rect& area, const SquaresData& s, std::int32_t limit, const compat::CompatSettings& c) {
  Placement p;
  p.cols = square_axis(area.x0, area.x1, s, limit, p.cut.x0, c);
  if (p.cols == 0) {
    return p;
  }
  p.rows = square_axis(area.y0, area.y1, s, limit, p.cut.y0, c);
  if (p.rows == 0) {
    return p;
  }
  p.cut.x1 = p.cut.x0 + s.size;
  p.cut.y1 = p.cut.y0 + s.size;
  return p;
}

/// The smallest multiple of g at or above v (cifSquareGridFunc's rounding).
db::Coord ceil_multiple(db::Coord v, std::int32_t g) {
  db::Coord m = (v / g) * g;
  return m < v ? m + g : m;
}

/// cifSquareGridFunc.
Placement square_grid_func(const db::Rect& area, const SquaresData& s) {
  Placement p;
  const std::int32_t pitch = s.size + s.sep;
  const db::Rect l{area.x0 + s.border, area.y0 + s.border, area.x1 - s.border, area.y1 - s.border};
  const db::Coord left = ceil_multiple(l.x0, s.gridx);
  const db::Coord bottom = ceil_multiple(l.y0, s.gridy);
  p.cols = (l.x1 - left + s.sep) / pitch;
  if (p.cols == 0) {
    return p;
  }
  p.rows = (l.y1 - bottom + s.sep) / pitch;
  if (p.rows == 0) {
    return p;
  }
  const db::Coord right = left + p.cols * s.size + (p.cols - 1) * s.sep;
  const db::Coord top = bottom + p.rows * s.size + (p.rows - 1) * s.sep;
  const db::Coord mx = ((l.x1 - right) - (left - l.x0)) / (s.gridx * 2);
  const db::Coord my = ((l.y1 - top) - (bottom - l.y0)) / (s.gridy * 2);
  p.cut.x0 = left + mx * s.gridx;
  p.cut.y0 = bottom + my * s.gridy;
  p.cut.x1 = p.cut.x0 + s.size;
  p.cut.y1 = p.cut.y0 + s.size;
  return p;
}

struct SlotsData {
  std::int32_t sborder = 0;
  std::int32_t ssize = 0;
  std::int32_t ssep = 0;
  std::int32_t lborder = 0;
  std::int32_t lsize = 0;
  std::int32_t lsep = 0;
  std::int32_t offset = 0;
  std::int32_t start = 0;
};

SlotsData slots_data(const LayerOp& op) {
  return SlotsData{detail::value32(op, 0), detail::value32(op, 1), detail::value32(op, 2), detail::value32(op, 3),
                   detail::value32(op, 4), detail::value32(op, 5), detail::value32(op, 6), detail::value32(op, 7)};
}

/// cifSlotFunc's view of a rectangle along the short ("x") and long ("y")
/// axes of a slot: vertical slots use x and y, horizontal ones swap them.
struct SlotAxes {
  db::Coord& ax0;
  db::Coord& ax1;
  db::Coord& ay0;
  db::Coord& ay1;
  db::Coord& sx0;
  db::Coord& sx1;
  db::Coord& sy0;
  db::Coord& sy1;
  int& rows;
  int& cols;
};

SlotAxes slot_axes(db::Rect& area, Placement& p, bool vertical) {
  if (vertical) {
    return SlotAxes{area.x0, area.x1, area.y0, area.y1, p.cut.x0, p.cut.x1, p.cut.y0, p.cut.y1, p.rows, p.cols};
  }
  return SlotAxes{area.y0, area.y1, area.x0, area.x1, p.cut.y0, p.cut.y1, p.cut.x0, p.cut.x1, p.cols, p.rows};
}

/// One axis of cifSlotFunc: the count and the cut along [lo, hi] for cut
/// size `size`, separation `sep` and border `border`, with Magic's
/// sign-aware grid-limit step.
int slot_axis(db::Coord lo, db::Coord& hi, std::int32_t size, std::int32_t sep, std::int32_t border, std::int32_t limit,
              db::Coord& c0, db::Coord& c1) {
  const std::int32_t pitch = size + sep;
  for (int step = 0; step < kMaxGridSteps; ++step) {
    const int n = (hi - lo + sep - (2 * border)) / pitch;
    if (n == 0) {
      return 0;
    }
    c0 = (lo + hi + sep - (n * pitch)) / 2;
    c1 = c0 + size;
    const std::int32_t delta = limit > 1 ? std::abs(c0) % limit : 0;
    if (delta == 0) {
      return n;
    }
    hi = c0 >= 0 ? hi - 2 * delta : hi + 2 * delta;
  }
  return 0;
}

/// cifSlotFunc. `area` is updated as Magic's is.
Placement slot_func(db::Rect& area, const SlotsData& s, std::int32_t limit, bool vertical) {
  Placement p;
  SlotAxes a = slot_axes(area, p, vertical);
  a.cols = slot_axis(a.ax0, a.ax1, s.ssize, s.ssep, s.sborder, limit, a.sx0, a.sx1);
  if (a.cols == 0) {
    a.rows = 0;
    return p;
  }
  if (s.lsize > 0) {
    a.rows = slot_axis(a.ay0, a.ay1, s.lsize, s.lsep, s.lborder, limit, a.sy0, a.sy1);
    return p;
  }
  a.rows = 1;
  a.sy0 = a.ay0 + s.lborder;
  a.sy1 = a.ay1 - s.lborder;
  return p;
}

// --- The strip pre-pass (cifSquaresStripFunc) -------------------------------

struct Strip {
  db::Rect area;
  bool vertical = false;
  bool shrink_ld = false;  // shrink left or down
  bool shrink_ur = false;  // shrink right or up
};

bool is_solid(tech::TileType t) noexcept { return t == kMaskSolid; }
bool is_space(tech::TileType t) noexcept { return t == tech::kSpace; }

/// A horizontal tile's strips: the x-ranges with space directly above and
/// directly below (the segment walks of cifSquaresStripFunc).
class HorizontalStrips {
 public:
  HorizontalStrips(const db::Plane& plane, const db::TileView& t)
      : t_(t),
        top_(detail::side_tiles(plane, t, db::Side::top)),
        bottom_(detail::side_tiles(plane, t, db::Side::bottom)) {}

  void collect(std::vector<Strip>& out) {
    int i = static_cast<int>(top_.size()) - 1;
    db::Coord segend = t_.area.x1;
    while (i >= 0) {
      const int before = i;
      while (i >= 0 && is_solid(top_facing(i))) {
        --i;
      }
      segend = std::min(segend, top_right(i));
      while (i >= 0 && is_space(top_facing(i))) {
        --i;
      }
      const db::Coord segstart = std::max(t_.area.x0, top_right(i));
      if (segend <= segstart || i == before) {
        break;  // (a tile neither solid nor space cannot occur on a mask plane)
      }
      match_bottom(segstart, segend, out);
    }
  }

 private:
  [[nodiscard]] db::Coord top_right(int i) const {
    return i >= 0 ? top_[static_cast<std::size_t>(i)].area.x1 : t_.area.x0;
  }
  [[nodiscard]] tech::TileType top_facing(int i) const {
    return detail::bottom_type(top_[static_cast<std::size_t>(i)]);
  }
  [[nodiscard]] db::Coord bottom_left(std::size_t j) const {
    return j < bottom_.size() ? bottom_[j].area.x0 : t_.area.x1;
  }
  [[nodiscard]] db::Coord bottom_right(std::size_t j) const {
    return j < bottom_.size() ? bottom_[j].area.x1 : t_.area.x1;
  }
  [[nodiscard]] tech::TileType bottom_facing(std::size_t j) const {
    return j < bottom_.size() ? detail::top_type(bottom_[j]) : detail::kBoundaryType;
  }

  void match_bottom(db::Coord segstart, db::Coord segend, std::vector<Strip>& out) const {
    std::size_t j = 0;
    while (bottom_right(j) < segstart && j < bottom_.size()) {
      ++j;
    }
    while (bottom_left(j) < segend) {
      while (bottom_right(j) < segstart && j < bottom_.size()) {
        ++j;
      }
      while (bottom_left(j) < segend && is_solid(bottom_facing(j))) {
        ++j;
      }
      const db::Coord matchstart = std::max(bottom_left(j), segstart);
      while (bottom_left(j) < segend && is_space(bottom_facing(j))) {
        ++j;
      }
      const db::Coord matchend = std::min(bottom_left(j), segend);
      if (matchend <= matchstart) {
        break;
      }
      Strip s;
      s.area = db::Rect{matchstart, t_.area.y0, matchend, t_.area.y1};
      s.shrink_ur = matchend != t_.area.x1;
      s.shrink_ld = matchstart != t_.area.x0;
      out.push_back(s);
    }
  }

  db::TileView t_;
  std::vector<db::TileView> top_;
  std::vector<db::TileView> bottom_;
};

bool left_side_has_split(const db::Plane& plane, const db::TileView& t) {
  const std::vector<db::TileView> left = detail::side_tiles(plane, t, db::Side::left);
  return std::any_of(left.begin(), left.end(), [](const db::TileView& v) { return v.split; });
}

void strip_of_tile(const db::Plane& plane, const db::TileView& t, std::int32_t size, std::int32_t pitch,
                   std::vector<Strip>& out) {
  if (t.split) {
    return;
  }
  const std::int64_t width = t.area.width();
  const std::int64_t height = t.area.height();
  const bool vertical = height > width;
  const std::int64_t short_side = vertical ? width : height;
  if (short_side < size || short_side >= std::int64_t{size} + pitch || left_side_has_split(plane, t)) {
    return;
  }
  if (!vertical) {
    HorizontalStrips{plane, t}.collect(out);
    return;
  }
  Strip s;
  s.area = t.area;
  s.vertical = true;
  s.shrink_ur = is_solid(detail::facing_type(detail::last_neighbor(plane, t, db::Side::top), db::Side::top));
  s.shrink_ld = is_solid(detail::facing_type(detail::first_neighbor(plane, t, db::Side::bottom), db::Side::bottom));
  out.push_back(s);
}

/// Every strip of `plane`, in Magic's processing order (the most recently
/// found first).
std::vector<Strip> find_strips(const db::Plane& plane, std::int32_t size, std::int32_t pitch) {
  std::vector<Strip> found;
  plane.enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
    strip_of_tile(plane, t, size, pitch, found);
    return true;
  });
  return {found.rbegin(), found.rend()};
}

db::Rect shortened(const Strip& s, std::int32_t diff) {
  db::Rect r = s.area;
  if (diff <= 0) {
    return r;
  }
  db::Coord& lo = s.vertical ? r.y0 : r.x0;
  db::Coord& hi = s.vertical ? r.y1 : r.x1;
  if (s.shrink_ur) {
    hi -= diff;
  }
  if (s.shrink_ld) {
    lo += diff;
  }
  return r;
}

// --- The region pass ---------------------------------------------------------

/// cifUnconnectFunc over `square`: true when every tile under it is a solid,
/// non-split tile of the region being processed.
bool square_in_region(const db::Plane& plane, const db::Rect& square, const detail::Marks& marks) {
  return plane.enumerate(square, tech::TypeMask::all(), [&](const db::TileView& t) {
    return t.type != tech::kSpace && !t.split && marks.get(t) == detail::Mark::processed;
  });
}

/// Shrinks a two-cut bounding box that placed no cut (beveled corners):
/// returns false when no further pass applies.
bool shrink_for_retry(db::Rect& bbox, const Placement& p, std::int32_t xdiff, std::int32_t ydiff) {
  const auto bdiff = [](std::int64_t extent, std::int32_t diff) {
    auto b = static_cast<std::int32_t>(1 + extent - diff);
    if ((b & 0x1) != 0) {
      ++b;
    }
    return b >> 1;
  };
  if (p.rows == 2) {
    const std::int32_t b = bdiff(bbox.height(), ydiff);
    bbox.y1 -= b;
    bbox.y0 += b;
    return true;
  }
  if (p.cols == 2) {
    const std::int32_t b = bdiff(bbox.width(), xdiff);
    bbox.x1 -= b;
    bbox.x0 += b;
    return true;
  }
  return false;
}

/// How one contact operator places cuts: the strip sizes, the placement of
/// a strip and of a region bounding box, and the cut walk of a region.
struct CutRules {
  std::int32_t strip_size = 0;   // S
  std::int32_t strip_pitch = 0;  // P
  std::int32_t shrink = 0;       // D
  std::function<Placement(db::Rect&, bool vertical)> place;
  std::int32_t xpitch_v = 0, ypitch_v = 0, xborder_v = 0, yborder_v = 0, xdiff_v = 0, ydiff_v = 0;  // vertical
  std::int32_t xpitch_h = 0, ypitch_h = 0, xborder_h = 0, yborder_h = 0, xdiff_h = 0, ydiff_h = 0;  // horizontal
  std::int32_t offset = 0;
  std::int32_t start = 0;
  bool oriented = false;  // slots: the region orientation selects the axes
};

struct KernelState {
  const OpArgs& args;
  OperatorContext& ctx;
  MaskPlane& plane;  // the input (strips are erased from it)
  MaskPlane& out;    // the cuts
};

/// Paints the cut array of a strip: squares paint the full array, slots
/// along the long axis only (with the long pitch).
int paint_strip_cuts(MaskPlane& out, const Placement& p, std::int32_t pitch, const Strip& s, bool slots) {
  int painted = 0;
  if (!slots) {
    for (int i = 0; i < p.rows; ++i) {
      for (int j = 0; j < p.cols; ++j) {
        const db::Rect c{p.cut.x0 + j * pitch, p.cut.y0 + i * pitch, p.cut.x1 + j * pitch, p.cut.y1 + i * pitch};
        detail::paint(out, c);
        ++painted;
      }
    }
    return painted;
  }
  const int n = s.vertical ? p.rows : p.cols;
  for (int i = 0; i < n; ++i) {
    const db::Coord dx = s.vertical ? 0 : i * pitch;
    const db::Coord dy = s.vertical ? i * pitch : 0;
    detail::paint(out, db::Rect{p.cut.x0 + dx, p.cut.y0 + dy, p.cut.x1 + dx, p.cut.y1 + dy});
    ++painted;
  }
  return painted;
}

struct StripChecks {
  bool slots = false;
  std::int32_t short_size = 0;  // squares: size; slots: ssize
  std::int32_t long_size = 0;   // squares: size; slots: lsize
  std::int64_t area_limit = 0;  // 2*P² (squares) or 2*lpitch*spitch (slots)
  std::int32_t pitch = 0;       // the strip cut pitch
};

bool exact_cut_size(const db::Rect& r, const StripChecks& k) {
  const std::int64_t w = r.width();
  const std::int64_t h = r.height();
  return (w == k.short_size && h == k.long_size) || (k.slots && w == k.long_size && h == k.short_size);
}

void process_strip(const KernelState& st, const CutRules& rules, const StripChecks& checks, const Strip& s) {
  db::Rect less = shortened(s, rules.shrink);
  const Placement p = rules.place(less, s.vertical);
  paint_strip_cuts(st.out, p, checks.pitch, s, checks.slots);
  const bool none = checks.slots ? (p.rows == 0 || p.cols == 0) : p.rows == 0;
  if (none) {
    if (!s.shrink_ur && !s.shrink_ld) {
      if (exact_cut_size(s.area, checks)) {
        detail::paint(st.out, s.area);
        detail::count_tile(st.ctx);
      } else {
        detail::cif_error(st.args, st.ctx, s.area, detail::kNoRoomForCuts, "no room for contact cuts in area!");
      }
    } else if (s.area.width() * s.area.height() > checks.area_limit) {
      detail::cif_error(st.args, st.ctx, s.area, detail::kStripWithoutCuts, "contact strip with no room for cuts!");
    }
  }
  detail::erase(st.plane, s.area);
}

struct Axes {
  std::int32_t xpitch, ypitch, xborder, yborder, xdiff, ydiff;
};

Axes region_axes(const CutRules& r, bool vertical) {
  if (vertical) {
    return Axes{r.xpitch_v, r.ypitch_v, r.xborder_v, r.yborder_v, r.xdiff_v, r.ydiff_v};
  }
  return Axes{r.xpitch_h, r.ypitch_h, r.xborder_h, r.yborder_h, r.xdiff_h, r.ydiff_h};
}

/// One pass of cuts over a region's bounding box; returns the cuts painted.
int place_region_cuts(const KernelState& st, const CutRules& rules, const Axes& ax, const Placement& p,
                      const detail::Region& region, const detail::Marks& marks) {
  int painted = 0;
  db::Coord offset = rules.start;
  for (int i = 0; i < p.rows; ++i) {
    const db::Coord y0 = p.cut.y0 + rules.start + i * ax.ypitch;
    const db::Coord y1 = p.cut.y1 + rules.start + i * ax.ypitch;
    for (int j = 0; j < p.cols; ++j) {
      const db::Rect cut{p.cut.x0 + offset + j * ax.xpitch, y0, p.cut.x1 + offset + j * ax.xpitch, y1};
      const db::Rect square{cut.x0 - ax.xborder, cut.y0 - ax.yborder, cut.x1 + ax.xborder, cut.y1 + ax.yborder};
      if (region.simple || square_in_region(st.plane.plane(), square, marks)) {
        detail::paint(st.out, cut);
        detail::count_tile(st.ctx);
        ++painted;
      }
    }
    offset += rules.offset;
    if (offset >= ax.xpitch) {
      offset -= ax.xpitch;
    }
  }
  return painted;
}

void process_region(const KernelState& st, const CutRules& rules, const detail::Region& region,
                    const detail::Marks& marks) {
  db::Rect bbox = region.bbox;
  const bool vertical = rules.oriented && bbox.width() <= bbox.height();
  const Axes ax = region_axes(rules, vertical);
  int painted = 0;
  for (int k = 0; k < 3; ++k) {
    const Placement p = rules.place(bbox, vertical);
    painted += place_region_cuts(st, rules, ax, p, region, marks);
    if (painted > 0 || !shrink_for_retry(bbox, p, ax.xdiff, ax.ydiff)) {
      break;
    }
  }
  if (painted == 0) {
    detail::cif_error(st.args, st.ctx, bbox, detail::kNoRoomForContacts, "no room for contacts in area!");
  }
}

void fill_area(const KernelState& st, const CutRules& rules, const StripChecks& checks) {
  for (const Strip& s : find_strips(st.plane.plane(), rules.strip_size, rules.strip_pitch)) {
    process_strip(st, rules, checks, s);
  }
  const db::Plane& plane = st.plane.plane();
  detail::Marks marks;
  detail::for_each_region_start(plane, db::whole_plane(), marks, [&](const db::TileView& start) {
    const detail::Region region = detail::collect_region(plane, start, marks, [](const db::TileView&) {});
    process_region(st, rules, region, marks);
    for (const db::TileView& t : region.tiles) {
      marks.set(t, detail::Mark::ignore);
    }
  });
}

diag::Status squares_kernel(const OpArgs& args, OperatorContext& ctx, MaskPlane& out, bool grid) {
  const SquaresData s = squares_data(args.op);
  if (s.size <= 0 || s.size + s.sep <= 0) {
    return diag::Done{};
  }
  const std::int32_t limit = detail::grid_limit_of(args);
  CutRules rules;
  rules.strip_size = s.size + 2 * s.border;
  rules.strip_pitch = s.size + s.sep;
  rules.shrink = s.sep - 2 * s.border;
  rules.place = [&](db::Rect& area, bool) {
    return grid ? square_grid_func(area, s) : square_func(area, s, limit, ctx.compat);
  };
  const std::int32_t pitch = s.size + s.sep;
  const std::int32_t diff = 2 * (s.size + s.border) + s.sep;
  rules.xpitch_v = rules.ypitch_v = rules.xpitch_h = rules.ypitch_h = pitch;
  rules.xborder_v = rules.yborder_v = rules.xborder_h = rules.yborder_h = s.border;
  rules.xdiff_v = rules.ydiff_v = rules.xdiff_h = rules.ydiff_h = diff;
  const StripChecks checks{false, s.size, s.size, 2 * std::int64_t{pitch} * pitch, pitch};
  ScratchPlane next{*ctx.pool};
  fill_area(KernelState{args, ctx, out, *next}, rules, checks);
  out.swap(*next);
  return diag::Done{};
}

}  // namespace

diag::Status op_squares(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  return squares_kernel(args, ctx, out, false);
}

diag::Status op_squares_grid(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  return squares_kernel(args, ctx, out, true);
}

diag::Status op_slots(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const SlotsData s = slots_data(args.op);
  const std::int32_t spitch = s.ssize + s.ssep;
  const std::int32_t lpitch = s.lsize + s.lsep;
  if (s.ssize <= 0 || spitch <= 0) {
    return diag::Done{};
  }
  const std::int32_t limit = detail::grid_limit_of(args);
  CutRules rules;
  rules.strip_size = s.ssize + 2 * s.sborder;
  rules.strip_pitch = spitch;
  rules.shrink = s.lsep - s.lborder - s.sborder;
  rules.place = [&](db::Rect& area, bool vertical) { return slot_func(area, s, limit, vertical); };
  const std::int32_t sdiff = 2 * (s.ssize + s.sborder) + s.ssep;
  const std::int32_t ldiff = 2 * (s.lsize + s.lborder) + s.lsep;
  rules.xpitch_v = spitch;
  rules.ypitch_v = lpitch;
  rules.xborder_v = s.sborder;
  rules.yborder_v = s.lborder;
  rules.xdiff_v = sdiff;
  rules.ydiff_v = ldiff;
  rules.xpitch_h = lpitch;
  rules.ypitch_h = spitch;
  rules.xborder_h = s.lborder;
  rules.yborder_h = s.sborder;
  rules.xdiff_h = ldiff;
  rules.ydiff_h = sdiff;
  rules.offset = s.offset;
  rules.start = s.start;
  rules.oriented = true;
  const StripChecks checks{true, s.ssize, s.lsize, 2 * std::int64_t{lpitch} * spitch, lpitch};
  ScratchPlane next{*ctx.pool};
  fill_area(KernelState{args, ctx, out, *next}, rules, checks);
  out.swap(*next);
  return diag::Done{};
}

}  // namespace hefesto::mask
