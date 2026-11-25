// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device dimensions: the terminal boundary lists, their separation for
// MOS capacitors, the effective length and width of non-rectangular gates
// and the capacitor bounding box. Capability extract-devices "Boundary
// lists for dimension computation", "Effective length and width of
// non-rectangular gates", "Capacitor dimensions and value";
// openspec/changes/m7-extraction/design.md D10, D22 (the pinned sqrt uses).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c:1207-1695
// (extComputeCapLW, extComputeEffectiveLW, extSeparateBounds) and
// :4229-4587 (extAnnularTileFunc, extResistorTileFunc,
// extSpecialPerimFunc), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Integer divisions are C's; the corner-chop product is single precision.
//
// Owner: EXTDEV.

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <list>
#include <vector>

#include "device_internal.hpp"

namespace hefesto::extract {

using device_detail::Boundary;

namespace {

// --- extSpecialPerimFunc's list merge ---

// The coordinates of a segment along its direction (y for left and right
// segments, x for top and bottom ones).
struct Along {
  db::Coord* lo;
  db::Coord* hi;
};

Along along(db::Rect& r, bool vertical) {
  return vertical ? Along{&r.y0, &r.y1} : Along{&r.x0, &r.x1};
}

// Extends `lb` by `seg` when they are collinear and abut; `last` is the
// segment extended before (a segment bridging two joins them into `last`).
bool extend_segment(BoundarySegment& lb, const db::Rect& seg, BoundarySegment* last) {
  if (lb.dir > BoundaryDir::bottom) {
    return false;  // non-Manhattan segments are not joined
  }
  const bool vertical = lb.dir == BoundaryDir::left || lb.dir == BoundaryDir::right;
  if ((vertical ? seg.x0 : seg.y0) != (vertical ? lb.r.x0 : lb.r.y0)) {
    return false;
  }
  db::Rect s = seg;
  const Along a = along(lb.r, vertical);
  const Along n = along(s, vertical);
  if (*n.lo == *a.hi) {
    if (last != nullptr) {
      *along(last->r, vertical).lo = *a.lo;
    } else {
      *a.hi = *n.hi;
    }
    return true;
  }
  if (*n.hi == *a.lo) {
    if (last != nullptr) {
      *along(last->r, vertical).hi = *a.hi;
    } else {
      *a.lo = *n.lo;
    }
    return true;
  }
  return false;
}

void add_segment(std::vector<BoundarySegment>& list, const db::Rect& seg, BoundaryDir dir) {
  int extended = 0;
  std::size_t last = 0;
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (list[i].dir == dir && extend_segment(list[i], seg, extended != 0 ? &list[last] : nullptr)) {
      ++extended;
      last = i;
    }
    if (extended == 2) {
      list.erase(list.begin() + static_cast<std::ptrdiff_t>(last));  // the bridged segment is redundant
      return;
    }
  }
  if (extended == 0) {
    list.insert(list.begin(), BoundarySegment{seg, dir});
  }
}

}  // namespace

namespace device_detail {

namespace {

struct SpecialSides {
  tech::TileType inside = tech::kSpace;
  tech::TileType outside = tech::kSpace;
  db::TileView out_view;
};

// extSpecialPerimFunc's inside and outside types (the diagonal of a split
// tile has the tile on both sides).
SpecialSides special_sides(const Boundary& b) {
  const db::TileView& o = b.outside;
  switch (b.dir) {
    case BoundaryDir::top:
      return {top_type(b.inside), bottom_type(o), side_view(o, o.split && !split_direction(o))};
    case BoundaryDir::bottom:
      return {bottom_type(b.inside), top_type(o), side_view(o, o.split && split_direction(o))};
    case BoundaryDir::right:
      return {right_type(b.inside), left_type(o), side_view(o, false)};
    case BoundaryDir::left:
    case BoundaryDir::nw:
    case BoundaryDir::sw:
      return {left_type(b.inside), right_type(o), side_view(o, true)};
    case BoundaryDir::ne:
    case BoundaryDir::se:
      break;
  }
  return {right_type(b.inside), left_type(o), side_view(o, false)};
}

// Planes of a type mask (DBTechTypesToPlanes); space is on every plane.
bool mask_on_plane(const Context& ctx, const tech::TypeMask& mask, tech::PlaneId plane) {
  if (mask.test(tech::kSpace)) {
    return true;
  }
  return mask.intersects(ctx.tech.plane_types(plane));
}

struct SpecialPerim {
  Context& ctx;
  TransRec& tr;
  BoundaryLists& lists;
  bool sense;

  // The node and survey decision of an off-plane terminal mask.
  bool off_plane_survey(const DevicePattern& d, const SpecialSides& s, NodeRef& term_node) const {
    for (std::size_t i = 0; !sd_types(d, i).empty(); ++i) {
      tech::TypeMask mmask = sd_types(d, i);
      if (s.outside != tech::kSpace) {
        mmask.clear_type(tech::kSpace);
      }
      if (!mask_on_plane(ctx, mmask, ctx.home(s.inside))) {
        term_node = tr.terms[i].node;
        return true;
      }
    }
    return false;
  }

  // The boundary's terminal node, and whether it is a terminal boundary.
  bool survey(const DevicePattern& d, const SpecialSides& s, NodeRef& term_node) const {
    bool on_terminal = false;
    for (std::size_t i = 0; !sd_types(d, i).empty() && !on_terminal; ++i) {
      on_terminal = sd_types(d, i).test(s.outside);
    }
    if ((!sense || on_terminal) && s.outside == tech::kSpace && ctx.in.substrate_node) {
      term_node = ctx.in.substrate_node;
    }
    return on_terminal || off_plane_survey(d, s, term_node);
  }

  void operator()(const Boundary& b) {
    const SpecialSides s = special_sides(b);
    NodeRef term_node = ctx.node(b.plane, s.out_view);
    if (tr.rec == nullptr || (!survey(*tr.rec, s, term_node) && sense)) {
      return;
    }
    std::size_t thisterm = 0;
    if (sense) {
      while (thisterm < tr.nterm && tr.terms[thisterm].node != term_node) {
        ++thisterm;
      }
      if (thisterm >= tr.nterm) {
        return;  // e.g. a diode with space among its terminal types
      }
    }
    if (thisterm < lists.size()) {
      add_segment(lists[thisterm], b.segment, b.dir);
    }
  }
};

// extResistorTileFunc: the boundary against everything but the device and
// terminal-0 types, trying the records until list 0 has a segment.
void resistor_tile(Context& ctx, TransRec& tr, SpecialPerim& perim, tech::PlaneId pnum, const db::TileView& tile) {
  tech::TypeMask mask = tile.type < ctx.style.device_conn.size() ? ctx.style.device_conn[tile.type] : tech::TypeMask{};
  const std::vector<const DevicePattern*> list = ctx.style.records_for(tile.type);
  const DevicePattern* devptr = tr.rec != nullptr ? tr.rec : (list.empty() ? nullptr : list.front());
  while (devptr != nullptr) {
    mask |= sd_types(*devptr, 0);
    mask = ~mask;  // Magic complements the accumulated mask in place
    (void)enum_tile_perim(ctx.cell.plane(pnum), pnum, tile, mask, [&](const Boundary& b) { perim(b); });
    if (!perim.lists.empty() && !perim.lists[0].empty()) {
      break;
    }
    devptr = next_record(ctx.style, devptr, tile.type);
  }
  if (devptr != nullptr) {
    tr.rec = devptr;
  }
}

}  // namespace

BoundaryLists special_bounds(Context& ctx, const std::vector<PlaneTile>& tiles, TransRec& tr, std::size_t n,
                             bool sense) {
  BoundaryLists lists(n);
  SpecialPerim perim{ctx, tr, lists, sense};
  for (const auto& [pnum, tile] : tiles) {
    if (!sense) {
      resistor_tile(ctx, tr, perim, pnum, tile);
      continue;
    }
    const tech::TypeMask mask =
        tile.type < ctx.style.device_conn.size() ? ~ctx.style.device_conn[tile.type] : tech::TypeMask::all();
    (void)enum_tile_perim(ctx.cell.plane(pnum), pnum, tile, mask, [&](const Boundary& b) { perim(b); });
  }
  return lists;
}

}  // namespace device_detail

// --- extSeparateBounds ---

namespace {

using SegList = std::list<BoundarySegment>;

// One pass over list 0, moving every segment that chains onto the ends of
// the growing chain; true when one moved.
bool chain_pass(SegList& src, SegList& chain, SegList::iterator& start, SegList::iterator& end, db::Rect& ends) {
  bool found = false;
  for (auto lb = src.begin(); lb != src.end();) {
    const db::Rect r = lb->r;
    const auto next = std::next(lb);
    if (r.x0 == ends.x0 && r.y0 == ends.y0) {
      start = chain.insert(std::next(start), *lb);
      ends.x0 = r.x1;
      ends.y0 = r.y1;
    } else if (r.x1 == ends.x0 && r.y1 == ends.y0) {
      start = chain.insert(std::next(start), *lb);
      ends.x0 = r.x0;
      ends.y0 = r.y0;
    } else if (r.x1 == ends.x1 && r.y1 == ends.y1) {
      end = chain.insert(std::next(end), *lb);
      ends.x1 = r.x0;
      ends.y1 = r.y0;
    } else if (r.x0 == ends.x1 && r.y0 == ends.y1) {
      end = chain.insert(std::next(end), *lb);
      ends.x1 = r.x1;
      ends.y1 = r.y1;
    } else {
      lb = next;
      continue;
    }
    src.erase(lb);
    lb = next;
    found = true;
  }
  return found;
}

}  // namespace

void separate_bounds(BoundaryLists& lists, std::size_t n) {
  if (n == 0 || n > lists.size() || lists[0].empty() || !lists[n - 1].empty()) {
    return;
  }
  SegList src(lists[0].begin(), lists[0].end());
  SegList chain{src.front()};
  src.pop_front();
  auto start = chain.begin();
  auto end = chain.begin();
  db::Rect ends = chain.front().r;
  while (chain_pass(src, chain, start, end, ends)) {
  }
  lists[0].assign(src.begin(), src.end());
  lists[n - 1].assign(chain.begin(), chain.end());
}

// --- extComputeEffectiveLW ---

namespace {

constexpr std::int64_t kInfinity = std::numeric_limits<std::int32_t>::max();

BoundaryDir opposite(BoundaryDir d) {
  switch (d) {
    case BoundaryDir::left:
      return BoundaryDir::right;
    case BoundaryDir::right:
      return BoundaryDir::left;
    case BoundaryDir::top:
      return BoundaryDir::bottom;
    default:
      return BoundaryDir::top;
  }
}

// The segment's span across the gate (lo, hi) and its distance to `t` on
// the far side (or nullopt when t is not beyond s).
struct Span {
  std::int64_t lo = 0;
  std::int64_t hi = 0;
};

Span span_of(const BoundarySegment& s) {
  const bool vertical = s.dir == BoundaryDir::left || s.dir == BoundaryDir::right;
  return vertical ? Span{s.r.y0, s.r.y1} : Span{s.r.x0, s.r.x1};
}

std::optional<std::int64_t> distance(const BoundarySegment& s, const BoundarySegment& t) {
  switch (s.dir) {
    case BoundaryDir::left:
      return t.r.x0 > s.r.x0 ? std::optional<std::int64_t>(t.r.x0 - s.r.x0) : std::nullopt;
    case BoundaryDir::right:
      return t.r.x1 < s.r.x1 ? std::optional<std::int64_t>(s.r.x1 - t.r.x1) : std::nullopt;
    case BoundaryDir::top:
      return t.r.y1 < s.r.y1 ? std::optional<std::int64_t>(s.r.y1 - t.r.y1) : std::nullopt;
    default:
      return t.r.y0 > s.r.y0 ? std::optional<std::int64_t>(t.r.y0 - s.r.y0) : std::nullopt;
  }
}

// First pass: the closest opposite segment within the corner extension.
std::optional<std::int64_t> test_length(const BoundarySegment& s, const BoundarySegment& t) {
  std::optional<std::int64_t> d = distance(s, t);
  if (!d) {
    return std::nullopt;
  }
  const Span a = span_of(s);
  const Span b = span_of(t);
  if (!(b.lo < a.hi + *d && b.hi > a.lo - *d)) {
    return std::nullopt;
  }
  if (b.lo > a.hi) {
    *d += b.lo - a.hi;
  } else if (b.hi < a.lo) {
    *d += a.lo - b.hi;
  }
  return d;
}

// Second pass: the opposite width (x2) and corner extensions of `t`.
void width_of(const BoundarySegment& s, const BoundarySegment& t, std::int64_t loc, std::int64_t& lw,
              std::int64_t& cw) {
  const std::optional<std::int64_t> d = distance(s, t);
  if (!d || *d != loc) {
    return;
  }
  const Span a = span_of(s);
  const Span b = span_of(t);
  const std::int64_t segp = std::min(b.hi, a.hi);
  const std::int64_t segn = std::max(b.lo, a.lo);
  const std::int64_t w = segp - segn;
  if (w > 0) {
    lw += w * 2;
  }
  if (w <= -loc) {
    return;
  }
  const std::int64_t top = std::min(std::max(b.hi, a.hi) - std::max(segp, segn), loc);
  const std::int64_t bottom = std::min(std::min(segp, segn) - std::min(b.lo, a.lo), loc);
  cw += std::max<std::int64_t>(top, 0) + std::max<std::int64_t>(bottom, 0);
}

template <typename F>
void each_opposite(const BoundaryLists& lists, std::size_t n, std::size_t p, const BoundarySegment& s, const F& f) {
  const BoundaryDir opp = opposite(s.dir);
  for (std::size_t i = 0; i < n; ++i) {
    if (i == p && n > 1) {
      continue;
    }
    for (const BoundarySegment& t : lists[i]) {
      if (t.dir == opp) {
        f(t);
      }
    }
  }
}

struct LW {
  std::int64_t length = 0;
  std::int64_t width = 0;
  bool complex = false;
};

void add_primary_segment(const BoundaryLists& lists, std::size_t n, std::size_t p, const BoundarySegment& s,
                         float chop, LW& acc) {
  std::int64_t loc = kInfinity;
  each_opposite(lists, n, p, s, [&](const BoundarySegment& t) {
    if (const auto d = test_length(s, t)) {
      loc = std::min(loc, *d);
    }
  });
  if (loc == kInfinity) {
    return;  // not current-carrying: it only adds gate capacitance
  }
  if (acc.length == 0) {
    acc.length = loc;
  } else if (acc.length != loc) {
    if (loc < acc.length) {
      acc.width = acc.width * loc / acc.length;
      acc.length = loc;
    }
    acc.complex = true;
  }
  std::int64_t lw = 0;
  std::int64_t cw = 0;
  each_opposite(lists, n, p, s, [&](const BoundarySegment& t) { width_of(s, t, loc, lw, cw); });
  lw += static_cast<std::int64_t>(0.5 + static_cast<double>(static_cast<float>(cw) * chop));
  if (loc != acc.length) {
    lw = lw * acc.length / loc;
  }
  acc.width += lw;
}

std::int64_t diagonal_length(const BoundaryLists& lists, std::size_t n) {
  std::int64_t nm = 0;
  for (std::size_t i = 0; i < n; ++i) {
    for (const BoundarySegment& s : lists[i]) {
      if (s.dir >= BoundaryDir::nw) {
        const double a = static_cast<double>(s.r.x1 - s.r.x0);
        const double b = static_cast<double>(s.r.y1 - s.r.y0);
        nm += static_cast<std::int64_t>(0.5 + std::sqrt((a * a) + (b * b)));
      }
    }
  }
  return nm;
}

}  // namespace

std::optional<Dimensions> effective_lw(const BoundaryLists& lists, std::size_t n, float chop,
                                       bool* multiple_lengths) {
  n = std::min(n, lists.size());
  std::size_t p = 0;
  std::size_t most = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (lists[i].size() > most) {
      most = lists[i].size();
      p = i;
    }
  }
  LW acc;
  if (p < lists.size()) {
    for (const BoundarySegment& s : lists[p]) {
      if (s.dir <= BoundaryDir::bottom) {
        add_primary_segment(lists, n, p, s, chop, acc);
      }
    }
  }
  acc.width += diagonal_length(lists, n);
  if (multiple_lengths != nullptr) {
    *multiple_lengths = acc.complex;
  }
  if (acc.length <= 0 || acc.width <= 0) {
    return std::nullopt;
  }
  return Dimensions{acc.length, n == 1 ? (acc.width >> 2) : (acc.width >> 1)};
}

std::optional<Dimensions> cap_lw(const BoundaryLists& lists) {
  if (lists.empty() || lists[0].empty()) {
    return std::nullopt;
  }
  db::Rect box = lists[0].front().r;
  for (const BoundarySegment& s : lists[0]) {
    box = {std::min(box.x0, s.r.x0), std::min(box.y0, s.r.y0), std::max(box.x1, s.r.x1), std::max(box.y1, s.r.y1)};
  }
  return Dimensions{box.height(), box.width()};
}

std::int64_t equivalent_square(std::int64_t area) noexcept {
  return static_cast<std::int64_t>(std::sqrt(static_cast<double>(area)) + 0.5);
}

std::int64_t terminal_width(std::int64_t area, std::int64_t perimeter) noexcept {
  double w = static_cast<double>(perimeter * perimeter);
  w -= static_cast<double>(16 * area);
  w = std::sqrt(w);
  w += static_cast<double>(perimeter);
  const double v = 0.25 * w;
  if (std::isnan(v)) {
    return 0;  // a negative discriminant: Magic's (int)NaN is undefined (Q-EXTRACT-30: NaN as 0)
  }
  return static_cast<std::int64_t>(std::clamp(v, -2147483648.0, 2147483647.0));
}

}  // namespace hefesto::extract
