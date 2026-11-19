// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SelectChunk over the flat copy of a LEF macro (capability lef-write
// "Hidden-detail pin geometry with zero setback"): the largest rectangle
// of homogeneous material of one type (and the stacked contacts holding
// it) that contains the label rectangle, or the rectangle of a split tile
// of that type under it.
//
// Ported from Magic 8.3.694 (fc5daa9e) select/selCreate.c SelectChunk and
// selFindChunk, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md). The cell is flat, so the
// search runs over its own planes (no tree search).
//
// Owner: LEFW (design D2).

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include "hefesto/db/paint.hpp"
#include "lef/write/chunk.hpp"

namespace hefesto::lef::write_detail {

namespace {

struct ChunkSearch {
  const db::Plane* plane = nullptr;
  tech::TypeMask wrong;
  db::Rect contained;
  std::int64_t best_min = 0;
  std::int64_t best_max = 0;
  db::Rect best{0, 0, 0, 0};
};

// selFindChunk's recursion limit.
constexpr int kMaxChunkRecursion = 22;
// Window growths before the search gives up (hardening: Magic grows until
// the chunk fits, which material out to the plane's edge never does).
constexpr int kMaxWindowGrowths = 64;

std::optional<db::Rect> first_wrong(const ChunkSearch& s, const db::Rect& search) {
  std::optional<db::Rect> wrong;
  (void)s.plane->enumerate(search, s.wrong, [&](const db::TileView& t) {
    wrong = t.area;
    return false;
  });
  return wrong;
}

void find_chunk(ChunkSearch& s, const db::Rect& search, int level);

// The two smaller searches past the wrong tile, left/right then
// below/above, as selFindChunk tries them.
void split_search(ChunkSearch& s, const db::Rect& search, const db::Rect& wrong, int level) {
  db::Rect smaller = search;
  bool try_x = true;
  if (wrong.x0 >= s.contained.x1) {
    smaller.x1 = wrong.x0;
  } else if (wrong.x1 <= s.contained.x0) {
    smaller.x0 = wrong.x1;
  } else {
    try_x = false;
  }
  if (try_x) {
    find_chunk(s, smaller, level + 1);
  }
  smaller = search;
  if (wrong.y0 >= s.contained.y1) {
    smaller.y1 = wrong.y0;
  } else if (wrong.y1 <= s.contained.y0) {
    smaller.y0 = wrong.y1;
  } else {
    return;
  }
  find_chunk(s, smaller, level + 1);
}

// selFindChunk: the largest rectangle inside `search` containing the
// reference area and free of wrong types.
void find_chunk(ChunkSearch& s, const db::Rect& search, int level) {
  if (level == kMaxChunkRecursion) {
    return;
  }
  std::int64_t lo = search.width();
  std::int64_t hi = search.height();
  if (lo > hi) {
    std::swap(lo, hi);
  }
  if (lo < s.best_min || (lo == s.best_min && hi <= s.best_max)) {
    return;
  }
  const std::optional<db::Rect> wrong = first_wrong(s, search);
  if (!wrong) {
    s.best_min = lo;
    s.best_max = hi;
    s.best = search;
    return;
  }
  split_search(s, search, *wrong, level);
}

// Grows the search window past the sides the best rectangle touches. Magic
// computes in int, which overflows once the window is wider than about
// 2^30 and touches a side; here the sum is 64-bit and saturates, so the
// window leaves the usable plane and the search stops there.
db::Rect grow_window(const db::Rect& w, const db::Rect& best) {
  const auto clamp = [](std::int64_t v) {
    return static_cast<db::Coord>(
        std::clamp<std::int64_t>(v, std::numeric_limits<db::Coord>::min(), std::numeric_limits<db::Coord>::max()));
  };
  const std::int64_t width = std::int64_t{w.x1} - w.x0;
  const std::int64_t height = std::int64_t{w.y1} - w.y0;
  db::Rect n = w;
  n.x0 = clamp(best.x0 == w.x0 ? w.x0 - width : std::int64_t{best.x0} - 1);
  n.y0 = clamp(best.y0 == w.y0 ? w.y0 - height : std::int64_t{best.y0} - 1);
  n.x1 = clamp(best.x1 == w.x1 ? w.x1 + width : std::int64_t{best.x1} + 1);
  n.y1 = clamp(best.y1 == w.y1 ? w.y1 + height : std::int64_t{best.y1} + 1);
  return n;
}

// DBMaskAddStacking: the stacked contacts holding `type`.
tech::TypeMask with_stacking(const tech::Technology& tech, tech::TileType type) {
  tech::TypeMask m = tech::TypeMask::of(type);
  if (!tech.is_contact(type)) {
    return m;
  }
  for (const tech::ContactInfo& ci : tech.contacts()) {
    if (ci.stacked && (ci.component_a == type || ci.component_b == type)) {
      m.set(ci.type);
    }
  }
  return m;
}

// selSplitFunc: the first split tile of `mask` under `area`, on any plane.
std::optional<db::Rect> split_under(const db::CellDef& flat, const db::Rect& area, const tech::TypeMask& mask) {
  std::optional<db::Rect> split;
  for (std::size_t p = tech::kFirstUserPlane; p < flat.plane_count() && !split; ++p) {
    (void)flat.plane(static_cast<tech::PlaneId>(p)).enumerate(area, mask, [&](const db::TileView& t) {
      if (t.split) {
        split = t.area;
        return false;
      }
      return true;
    });
  }
  return split;
}

// DBCellCopyManhattanPaint of the home plane: the Manhattan tiles of
// `mask` under `window`, clipped.
db::Plane manhattan_copy(const tech::Technology& tech, const db::Plane& source, tech::PlaneId plane,
                         const db::Rect& window, const tech::TypeMask& mask) {
  db::Plane copy;
  source.enumerate(window, mask, [&](const db::TileView& t) {
    if (!t.split) {
      const db::Rect r{std::max(t.area.x0, window.x0), std::max(t.area.y0, window.y0), std::min(t.area.x1, window.x1),
                       std::min(t.area.y1, window.y1)};
      (void)copy.paint(r, db::paint_row(tech, plane, t.type), db::PaintMerge::none);
    }
    return true;
  });
  return copy;
}

}  // namespace

db::Rect chunk_rect(const tech::Technology& tech, const db::CellDef& flat, const db::Rect& area,
                    tech::TileType type) {
  if (type == tech::kSpace) {
    return db::Rect{};
  }
  const tech::TypeMask mask = with_stacking(tech, type);
  if (const auto split = split_under(flat, area, mask)) {
    return *split;
  }
  const auto home = tech.home_plane(type);
  if (!home || *home >= flat.plane_count()) {
    return db::Rect{};
  }
  ChunkSearch s;
  s.wrong = ~mask;
  s.contained = area;
  db::Rect window = db::expand(area, 10);
  for (int growth = 0; growth < kMaxWindowGrowths; ++growth) {
    const db::Plane copy = manhattan_copy(tech, flat.plane(*home), *home, window, mask);
    s.plane = &copy;
    find_chunk(s, window, 0);
    if (s.best.is_null() || db::strong_surround(window, s.best)) {
      return s.best;
    }
    window = grow_window(window, s.best);
    if (!db::in_usable_plane(window)) {
      return s.best;
    }
  }
  return s.best;
}

}  // namespace hefesto::lef::write_detail
