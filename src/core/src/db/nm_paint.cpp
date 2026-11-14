// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Non-Manhattan painting: fracturing, triangle subdivision with Magic's
// rounding, and split-tile merging (layout-paint-semantics "Non-Manhattan
// painting", "Split tile merging").
// Ported from Magic VLSI 8.3.694 (fc5daa9e) database/DBpaint.c:
// DBFracturePlane, DBMergeNMTiles0 (mergeOnce = FALSE, no undo),
// DBDiagonalProc, DBNMPaintPlane0 (PAINT_NORMAL), dbNMEnumFunc. Magic is
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <list>
#include <optional>

#include "db/tile_plane.hpp"

namespace hefesto::db::detail {

namespace {

// ---------------------------------------------------------------------------
// DBFracturePlane

class Fracturer {
 public:
  Fracturer(TilePlane& plane, const Rect& area, const ResultRow& row) : plane_(plane), area_(area), row_(row) {}

  void run() {
    Tile* tile = area_first(plane_, area_);
    while (tile != nullptr) {
      const Coord clip_top = std::min(top(tile), area_.y1);
      if (interacts(tile)) {
        fracture(tile);
      }
      plane_.set_hint(tile);
      tile = area_next(tile, area_, clip_top);
    }
  }

 private:
  [[nodiscard]] bool interacts(const Tile* tile) const noexcept {
    if (!is_split(tile->body)) {
      return false;
    }
    const Body l = left_type(tile->body);
    const Body r = right_type(tile->body);
    return row_[l] != l || row_[r] != r;
  }

  void merge_after_y(Tile*& tile, Tile* newtile) {
    if (!is_split(tile->body)) {
      tile = nm_merge_left(plane_, tile);
      nm_merge_right(plane_, newtile->tr);
    } else {
      nm_merge_left(plane_, newtile);
      nm_merge_right(plane_, tile->tr);
    }
  }

  void fracture(Tile*& tile) {
    Tile* newtile = nullptr;
    if (top(tile) > area_.y1 && is_split(tile->body)) {
      nm_split_y(plane_, tile, newtile, area_.y1, 1);
      merge_after_y(tile, newtile);
    }
    if (right(tile) <= area_.x0) {
      return;
    }
    if (bottom(tile) < area_.y0 && is_split(tile->body)) {
      nm_split_y(plane_, tile, newtile, area_.y0, 0);
      merge_after_y(tile, newtile);
    }
    if (right(tile) <= area_.x0) {
      return;
    }
    if (right(tile) > area_.x1 && is_split(tile->body)) {
      nm_split_x(plane_, tile, newtile, area_.x1, 1);
      if (!is_split(tile->body)) {
        tile = nm_merge_left(plane_, tile);
        nm_merge_right(plane_, newtile->lb);
      } else {
        nm_merge_right(plane_, newtile);
        nm_merge_left(plane_, tile->lb);
      }
    }
    if (bottom(tile) >= area_.y1) {
      return;
    }
    if (left(tile) < area_.x0 && is_split(tile->body)) {
      nm_split_x(plane_, tile, newtile, area_.x0, 0);
      if (!is_split(tile->body)) {
        tile = nm_merge_right(plane_, tile);
        nm_merge_left(plane_, newtile->lb);
      } else {
        nm_merge_left(plane_, newtile);
        nm_merge_right(plane_, tile->lb);
      }
    }
  }

  TilePlane& plane_;
  const Rect area_;
  const ResultRow& row_;
};

// ---------------------------------------------------------------------------
// DBMergeNMTiles0

class NMMerger {
 public:
  NMMerger(TilePlane& plane, const Rect& area) : plane_(plane), area_(area) {}

  void run() {
    Tile* tile = area_first(plane_, area_);
    while (tile != nullptr) {
      const Coord clip_top = std::min(top(tile), area_.y1);
      while (is_split(tile->body) && try_merge(tile)) {
      }
      plane_.set_hint(tile);
      tile = area_next(tile, area_, clip_top);
    }
  }

 private:
  // The lower split tile that shares the diagonal corner, or nullptr.
  static Tile* partner(Tile* tile) {
    Tile* tp = tile->lb;
    if (direction(tile->body) == 0) {
      tp = tp->bl;
      while (bottom(tile) > top(tp)) {
        tp = tp->rt;
      }
      return right(tp) != left(tile) || bottom(tile) != top(tp) ? nullptr : tp;
    }
    while (left(tp) < right(tile)) {
      tp = tp->tr;
    }
    return right(tile) != left(tp) || bottom(tile) != top(tp) ? nullptr : tp;
  }

  bool try_merge(Tile*& tile) {
    const Body ttype = tile->body & ~kSideBit;
    Tile* tp = partner(tile);
    if (tp == nullptr || (tp->body & ~kSideBit) != ttype) {
      return false;
    }
    const std::int64_t aspecta = (std::int64_t{right(tp)} - left(tp)) * (std::int64_t{top(tile)} - bottom(tile));
    const std::int64_t aspectb = (std::int64_t{right(tile)} - left(tile)) * (std::int64_t{top(tp)} - bottom(tp));
    if (aspecta != aspectb) {
      return false;
    }
    ltype_ = left_type(tile->body);
    rtype_ = right_type(tile->body);
    if (direction(tile->body) == 0) {
      if (!gaps_filled_dir0(tile, tp)) {
        return false;
      }
      merge_dir0(tile, tp);
    } else {
      if (!gaps_filled_dir1(tile, tp)) {
        return false;
      }
      merge_dir1(tile, tp);
    }
    plane_.join_x(tile, tp);
    tile->body = ttype;
    plane_.note_split(true, tile_rect(tile), ttype);  // dbJoinUndo
    return true;
  }

  [[nodiscard]] bool gaps_filled_dir0(const Tile* tile, const Tile* tp) const {
    const Tile* tp2 = tp->rt;
    while (bottom(tp2) < top(tile) && left(tp2) <= left(tp) && tp2->body == ltype_) {
      tp2 = tp2->rt;
    }
    if (bottom(tp2) < top(tile)) {
      return false;
    }
    tp2 = tile->lb;
    while (top(tp2) > bottom(tp) && right(tp2) >= right(tile) && tp2->body == rtype_) {
      tp2 = tp2->lb;
    }
    return top(tp2) <= bottom(tp);
  }

  [[nodiscard]] bool gaps_filled_dir1(const Tile* tile, const Tile* tp) const {
    const Tile* tp2 = tile->lb;
    while (top(tp2) > bottom(tp)) {
      while (right(tp2) < left(tp)) {
        tp2 = tp2->tr;
      }
      if (left(tp2) > left(tile) || tp2->body != ltype_) {
        break;
      }
      tp2 = tp2->lb;
    }
    if (top(tp2) > bottom(tp)) {
      return false;
    }
    tp2 = tp->rt;
    while (bottom(tp2) < top(tile)) {
      while (left(tp2) > right(tile)) {
        tp2 = tp2->bl;
      }
      if (right(tp2) < right(tp) || tp2->body != rtype_) {
        break;
      }
      tp2 = tp2->rt;
    }
    return bottom(tp2) >= top(tile);
  }

  void merge_x_neighbors(Tile* t) {
    if (can_merge_x(t, t->bl)) {
      plane_.join_x(t, t->bl);
    }
    if (can_merge_x(t, t->tr)) {
      plane_.join_x(t, t->tr);
    }
  }

  void merge_y_neighbors(Tile* t) {
    if (can_merge_y(t, t->lb)) {
      plane_.join_y(t, t->lb);
    }
    if (can_merge_y(t, t->rt)) {
      plane_.join_y(t, t->rt);
    }
  }

  // Splits `tp2` at y = `y` (piece above gets `body`) and merges the upper piece.
  void split_top_off(Tile* tp2, Coord y, Body body) {
    Tile* newtile = plane_.split_y(tp2, y);
    newtile->body = body;
    merge_x_neighbors(newtile);
    if (can_merge_y(newtile, newtile->rt)) {
      plane_.join_y(newtile, newtile->rt);
    }
  }

  // Splits `tp2` at y = `y` and merges the lower piece; returns the upper piece.
  Tile* split_bottom_off(Tile* tp2, Coord y, Body body) {
    Tile* newtile = plane_.split_y(tp2, y);
    newtile->body = body;
    merge_x_neighbors(tp2);
    if (can_merge_y(tp2, tp2->lb)) {
      plane_.join_y(tp2, tp2->lb);
    }
    return newtile;
  }

  // Splits `tp2` at x; merges the left piece vertically; returns the right piece.
  Tile* split_left_off(Tile* tp2, Coord x, Body body) {
    Tile* newtile = plane_.split_x(tp2, x);
    newtile->body = body;
    merge_y_neighbors(tp2);
    return newtile;
  }

  // Splits `tp2` at x; merges the right piece vertically.
  void split_right_off(Tile* tp2, Coord x, Body body) {
    Tile* newtile = plane_.split_x(tp2, x);
    newtile->body = body;
    merge_y_neighbors(newtile);
  }

  void merge_dir0(Tile*& tile, Tile*& tp) {
    Tile* tp2 = tp->rt;
    while (bottom(tp2) < top(tile)) {
      if (top(tp2) > top(tile)) {
        split_top_off(tp2, top(tile), ltype_);
      }
      if (left(tp2) < left(tp)) {
        tp2 = split_left_off(tp2, left(tp), ltype_);
      }
      plane_.join_y(tp2, tp);
      tp = tp2;
      tp2 = tp2->rt;
    }
    tp2 = tile->lb;
    while (top(tp2) > bottom(tp)) {
      if (bottom(tp2) < bottom(tp)) {
        tp2 = split_bottom_off(tp2, bottom(tp), rtype_);
      }
      if (right(tp2) > right(tile)) {
        split_right_off(tp2, right(tile), rtype_);
      }
      plane_.join_y(tp2, tile);
      tile = tp2;
      tp2 = tp2->lb;
    }
  }

  void merge_dir1(Tile*& tile, Tile*& tp) {
    Tile* tp2 = tile->lb;
    while (top(tp2) > bottom(tp)) {
      while (right(tp2) < left(tp)) {
        tp2 = tp2->tr;
      }
      if (bottom(tp2) < bottom(tp)) {
        tp2 = split_bottom_off(tp2, bottom(tp), ltype_);
      }
      if (left(tp2) < left(tile)) {
        tp2 = split_left_off(tp2, left(tile), ltype_);
      }
      plane_.join_y(tp2, tile);
      tile = tp2;
      tp2 = tp2->lb;
    }
    tp2 = tp->rt;
    while (bottom(tp2) < top(tile)) {
      while (left(tp2) > right(tile)) {
        tp2 = tp2->bl;
      }
      if (top(tp2) > top(tile)) {
        split_top_off(tp2, top(tile), rtype_);
      }
      if (right(tp2) > right(tp)) {
        split_right_off(tp2, right(tp), rtype_);
      }
      plane_.join_y(tp2, tp);
      tp = tp2;
      tp2 = tp2->rt;
    }
  }

  TilePlane& plane_;
  const Rect area_;
  Body ltype_ = 0;
  Body rtype_ = 0;
};

// ---------------------------------------------------------------------------
// DBDiagonalProc / DBNMPaintPlane0

struct DiagInfo {
  const ResultRow* row = nullptr;
  int dir = 0;
  int side = 0;
};

// Result of painting one side of a triangle over `oldtype`, or nullopt when
// it needs quartering (DBDiagonalProc returning -1).
std::optional<Body> diagonal_result(Body oldtype, const DiagInfo& d) {
  const ResultRow& row = *d.row;
  Body old_n = oldtype;
  Body old_s = oldtype;
  Body old_e = oldtype;
  Body old_w = oldtype;
  if (is_split(oldtype)) {
    old_e = (oldtype & kRightMask) >> kRightShift;
    old_w = oldtype & kLeftMask;
    old_n = (oldtype & kDirectionBit) != 0 ? old_e : old_w;
    old_s = (oldtype & kDirectionBit) != 0 ? old_w : old_e;
  }
  const Body new_e = d.side == 0 ? old_e : row[old_e];
  const Body new_w = d.side == 0 ? row[old_w] : old_w;
  const Body new_n = d.dir == d.side ? row[old_n] : old_n;
  const Body new_s = d.dir == d.side ? old_s : row[old_s];
  if (new_n == new_e && new_s == new_w) {
    return new_n == new_w ? new_n : split_body(new_w, new_e, 1);
  }
  if (new_n == new_w && new_s == new_e) {
    return split_body(new_w, new_e, 0);
  }
  return std::nullopt;
}

// Magic's diagonal crossing: rounded half up for `/`, truncated for `\`
// (Q-MODEL-3); with magic-compat off both diagonals round half up.
Coord crossing(std::int64_t ref, std::int64_t den, int dir, bool magic_compat) noexcept {
  if (dir != 0 && !magic_compat) {
    ref = -ref;
    return static_cast<Coord>(ref / den + (((ref % den) * 2 >= den) ? 1 : 0));
  }
  const std::int64_t round = ((ref % den) * 2 >= den) ? 1 : 0;
  if (dir != 0) {
    ref = -ref;
  }
  return static_cast<Coord>(round + ref / den);
}

enum class Res : std::uint8_t { left, diag, right, skip };

// One level of DBNMPaintPlane0: a sub-area whose own corner-to-corner
// diagonal is painted, the rectangles enumerated over it, and the next one
// to process.
struct PaintFrame {
  Rect area;
  std::int64_t height = 0;
  std::int64_t width = 0;
  std::list<Rect> rects;
  std::list<Rect>::iterator next;

  explicit PaintFrame(const Rect& a) : area(a), height(a.height()), width(a.width()) {}

  [[nodiscard]] Rect clip(const Rect& r) const noexcept {
    return Rect{std::max(r.x0, area.x0), std::max(r.y0, area.y0), std::min(r.x1, area.x1), std::min(r.y1, area.y1)};
  }
};

// The most sub-areas one triangle paint descends into (Q-MODEL-34). Over
// opposite-diagonal triangles the count grows linearly with the triangle's
// size in internal units and the work with its square; ordinary triangles
// stay far below it.
constexpr std::size_t kMaxSubAreas = 4096;

// DBNMPaintPlane0. Magic recurses into every rectangle the diagonal still
// crosses; over opposite-diagonal triangles that depth grows with the
// triangle's size in internal units, so a large triangle (or a large
// `magscale`) overflowed the call stack. The recursion runs here on an
// explicit stack of frames, in Magic's order: a sub-area is fractured and
// enumerated when it is reached, and its parent resumes with the rectangle
// after it once it is done. Past kMaxSubAreas a piece still on the diagonal
// is filled as a rectangle and reported off grid, as Magic does for a piece
// it cannot quarter (Q-MODEL-4).
class NMPainter {
 public:
  NMPainter(TilePlane& plane, Body exact, const ResultRow& row, bool magic_compat)
      : plane_(plane), row_(row), compat_(magic_compat) {
    d_.row = &row;
    d_.dir = (exact & kDirectionBit) != 0 ? 1 : 0;
    d_.side = (exact & kSideBit) != 0 ? 1 : 0;
  }

  /// Paints; true when a 1x1 area had to be filled because a triangle could
  /// not be quartered any further (DBNMPaintPlane0's "return 1", used by
  /// the DRC flattening to flag an off-grid overlap), or a piece past
  /// kMaxSubAreas had to be filled.
  bool run(const Rect& area) {
    enter(area);
    while (!frames_.empty()) {
      PaintFrame& f = frames_.back();  // deque: stays valid while children are pushed
      if (f.next == f.rects.end()) {
        frames_.pop_back();
        continue;
      }
      process(f, f.next);
      ++f.next;
    }
    return off_grid_;
  }

 private:
  // The start of one DBNMPaintPlane0 call on `area`.
  void enter(const Rect& area) {
    PaintFrame& f = frames_.emplace_back(area);
    Fracturer(plane_, area, row_).run();
    search_area(plane_, area, tech::TypeMask::all(), [&](Tile* t, Body dinfo) {
      if (!(is_split(t->body) && (dinfo & kSideBit) != 0)) {
        f.rects.push_front(tile_rect(t));  // dbNMEnumFunc prepends
      }
      return false;
    });
    if (f.rects.size() == 1 && single_tile(f)) {
      frames_.pop_back();
      return;
    }
    f.next = f.rects.begin();
  }

  // The one-tile case; returns true when painting is complete.
  bool single_tile(PaintFrame& f) {
    Rect& lr = f.rects.front();
    lr = f.clip(lr);
    Tile* tile = goto_point(plane_.hint(), Point{f.area.x0, f.area.y1 - 1});
    const Body l = left_type(tile->body);
    const Body r = right_type(tile->body);
    if (row_[l] == l && row_[r] == r) {
      return true;
    }
    const Body old = tile->body;
    const std::optional<Body> next = diagonal_result(old, d_);
    if (next && *next == old) {
      return true;
    }
    if (!next) {
      return quarter(f);
    }
    if ((*next & kDiagonalBit) != 0) {
      ResultRow special{};
      special.fill(tech::kCheckSubcell);  // DBSpecialPaintTbl
      paint_plane(plane_, lr, special);
      goto_point(plane_.hint(), Point{lr.x0, lr.y0})->body = *next;
    } else {
      paint_plane(plane_, lr, row_);
    }
    return true;
  }

  // Quartering of a triangle over the opposite diagonal (Q-MODEL-4).
  bool quarter(PaintFrame& f) {
    std::list<Rect>& rects = f.rects;
    Rect& lr = rects.front();
    if (f.width == 1 || f.height == 1) {
      paint_plane(plane_, lr, row_);
      off_grid_ = true;
      return true;
    }
    const Rect& area = f.area;
    lr.x1 -= static_cast<Coord>(f.width >> 1);
    lr.y1 -= static_cast<Coord>(f.height >> 1);
    const auto after = std::next(rects.begin());
    rects.insert(after, Rect{lr.x1, lr.y1, area.x1, area.y1});
    rects.insert(std::next(rects.begin(), 2), Rect{lr.x1, area.y0, area.x1, lr.y1});
    rects.insert(std::next(rects.begin(), 3), Rect{area.x0, lr.y1, lr.x1, area.y1});
    return false;
  }

  [[nodiscard]] Coord x_at(const PaintFrame& f, std::int64_t dy) const noexcept {
    return f.area.x0 + crossing(f.width * dy, f.height, d_.dir, compat_);
  }
  [[nodiscard]] Coord y_at(const PaintFrame& f, std::int64_t dx) const noexcept {
    return f.area.y0 + crossing(f.height * dx, f.width, d_.dir, compat_);
  }

  Res split_left(PaintFrame& f, std::list<Rect>::iterator it) {
    Rect& lr = *it;
    const Rect& a = f.area;
    const Coord xc = x_at(f, d_.dir != 0 ? std::int64_t{lr.y1} - a.y1 : std::int64_t{lr.y0} - a.y0);
    if (xc > lr.x0 && xc < lr.x1) {
      f.rects.insert(std::next(it), Rect{xc, lr.y0, lr.x1, lr.y1});
      lr.x1 = xc;
      return Res::left;
    }
    if (xc >= lr.x1) {
      return d_.side == 0 ? Res::left : Res::skip;
    }
    return Res::diag;
  }

  Res split_right(PaintFrame& f, std::list<Rect>::iterator it) {
    Rect& lr = *it;
    const Rect& a = f.area;
    const Coord xc = x_at(f, d_.dir != 0 ? std::int64_t{lr.y0} - a.y1 : std::int64_t{lr.y1} - a.y0);
    if (xc > lr.x0 && xc < lr.x1) {
      f.rects.insert(std::next(it), Rect{lr.x0, lr.y0, xc, lr.y1});
      lr.x0 = xc;
      return Res::right;
    }
    if (xc <= lr.x0) {
      return d_.side == 1 ? Res::right : Res::skip;
    }
    return Res::diag;
  }

  [[nodiscard]] Res side_res() const noexcept { return d_.side != 0 ? Res::right : Res::left; }

  Res split_top(PaintFrame& f, std::list<Rect>::iterator it) {
    Rect& lr = *it;
    const Rect& a = f.area;
    const Coord yc = y_at(f, d_.dir != 0 ? std::int64_t{lr.x0} - a.x1 : std::int64_t{lr.x1} - a.x0);
    if (yc > lr.y0 && yc < lr.y1) {
      f.rects.insert(std::next(it), Rect{lr.x0, lr.y0, lr.x1, yc});
      lr.y0 = yc;
      return d_.dir != 0 ? Res::right : Res::left;
    }
    if (yc <= lr.y0) {
      return d_.side == d_.dir ? side_res() : Res::skip;
    }
    return Res::diag;
  }

  Res split_bottom(PaintFrame& f, std::list<Rect>::iterator it) {
    Rect& lr = *it;
    const Rect& a = f.area;
    const Coord yc = y_at(f, d_.dir != 0 ? std::int64_t{lr.x1} - a.x1 : std::int64_t{lr.x0} - a.x0);
    if (yc > lr.y0 && yc < lr.y1) {
      f.rects.insert(std::next(it), Rect{lr.x0, yc, lr.x1, lr.y1});
      lr.y1 = yc;
      return d_.dir != 0 ? Res::left : Res::right;
    }
    if (yc >= lr.y1) {
      return d_.side != d_.dir ? side_res() : Res::skip;
    }
    return Res::diag;
  }

  void descend(const Rect& piece) {
    if (sub_areas_ < kMaxSubAreas) {
      ++sub_areas_;
      enter(piece);
      return;
    }
    paint_plane(plane_, piece, row_);
    off_grid_ = true;
  }

  // One rectangle of frame `f`; a rectangle still on the diagonal starts a
  // child frame (Magic's recursive call), which run() drains before `f`
  // resumes.
  void process(PaintFrame& f, std::list<Rect>::iterator it) {
    *it = f.clip(*it);
    Res res = Res::diag;
    for (auto step : {&NMPainter::split_left, &NMPainter::split_right, &NMPainter::split_top,
                      &NMPainter::split_bottom}) {
      res = (this->*step)(f, it);
      if (res != Res::diag) {
        break;
      }
    }
    if (res == Res::diag) {
      descend(*it);
    } else if ((res == Res::left && d_.side == 0) || (res == Res::right && d_.side != 0)) {
      paint_plane(plane_, *it, row_);
    }
  }

  TilePlane& plane_;
  const ResultRow& row_;
  const bool compat_;
  DiagInfo d_;
  std::deque<PaintFrame> frames_;
  std::size_t sub_areas_ = 0;
  bool off_grid_ = false;  // DBNMPaintPlane0's result, OR-ed over every level
};

}  // namespace

void merge_nm_tiles(TilePlane& plane, const Rect& area) {
  NMMerger(plane, area).run();
}

bool nm_paint_plane(TilePlane& plane, Body exact, const Rect& area, const ResultRow& row, bool magic_compat) {
  if ((exact & kDiagonalBit) == 0) {
    paint_plane(plane, area, row);
    return false;
  }
  return NMPainter(plane, exact, row, magic_compat).run(area);
}

}  // namespace hefesto::db::detail
