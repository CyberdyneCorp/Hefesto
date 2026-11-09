// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Painting a rectangle into a tile plane through a result row, keeping the
// maximal-horizontal-strip invariant (layout-geometry-model "Canonical
// maximal-horizontal-strip decomposition", layout-paint-semantics
// "Result-table model of painting").
// Ported from Magic VLSI 8.3.694 (fc5daa9e) database/DBpaint.c:
// DBPaintPlane0 (method PAINT_NORMAL, no undo) and dbPaintMerge. The goto
// structure of the original is split into one method per clipping step;
// the sequence of splits, joins and type changes is unchanged. Magic is
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).

#include <algorithm>

#include "db/tile_plane.hpp"

namespace hefesto::db::detail {

namespace {

enum : unsigned { kMrgTop = 0x01U, kMrgLeft = 0x02U, kMrgRight = 0x04U, kMrgBottom = 0x08U };

class Painter {
 public:
  Painter(TilePlane& plane, const Rect& area, const ResultRow& row) : plane_(plane), area_(area), row_(row) {}

  void run() {
    Tile* tile = area_first(plane_, area_);
    while (tile != nullptr) {
      const Coord clip_top = std::min(top(tile), area_.y1);
      tile = process(tile);
      plane_.set_hint(tile);
      tile = area_next(tile, area_, clip_top);
    }
  }

 private:
  enum class Step { next, paint_done, clip_done };

  [[nodiscard]] Body result_of(Body old) const noexcept {
    if (!is_split(old)) {
      return row_[old];
    }
    return Body{row_[left_type(old)]} | (Body{row_[right_type(old)]} << kRightShift) |
           (old & (kDiagonalBit | kDirectionBit | kSideBit));
  }

  void recompute(Tile* tile) {
    old_ = tile->body;
    new_ = row_[old_];
  }

  // The in-line paint code of DBPaintPlane0 for one enumerated tile.
  Tile* process(Tile* tile) {
    flags_ = kMrgTop | kMrgLeft;
    if (right(tile) >= area_.x1) {
      flags_ |= kMrgRight;
    }
    if (bottom(tile) <= area_.y0) {
      flags_ |= kMrgBottom;
    }
    old_ = tile->body;
    new_ = result_of(old_);
    if (old_ != new_ && clip(tile) == Step::paint_done) {
      return tile;
    }
    return change_and_merge(tile);
  }

  Step clip(Tile*& tile) {
    for (auto step : {&Painter::clip_up, &Painter::clip_down, &Painter::clip_right, &Painter::clip_left}) {
      const Step s = (this->*step)(tile);
      if (s != Step::next) {
        return s;
      }
    }
    return Step::clip_done;
  }

  // Shared NM handling of the up/down clips (dir 1 = up, 0 = down).
  void nm_clip_y(Tile*& tile, Coord y, int dir) {
    Tile* newtile = nullptr;
    nm_split_y(plane_, tile, newtile, y, dir);
    if (!is_split(tile->body)) {
      recompute(tile);
      if ((flags_ & kMrgLeft) != 0) {
        tile = nm_merge_left(plane_, tile);
      }
      if ((flags_ & kMrgRight) != 0 || direction(newtile->body) == dir) {
        nm_merge_right(plane_, newtile->tr);
      }
    } else {
      if ((flags_ & kMrgLeft) != 0) {
        nm_merge_left(plane_, newtile);
      }
      if ((flags_ & kMrgRight) != 0 || direction(tile->body) == dir) {
        nm_merge_right(plane_, tile->tr);
      }
    }
  }

  [[nodiscard]] Step after_vertical_clip(const Tile* tile) const noexcept {
    if (right(tile) <= area_.x0) {
      return Step::paint_done;
    }
    return old_ == new_ ? Step::clip_done : Step::next;
  }

  Step clip_up(Tile*& tile) {
    if (top(tile) > area_.y1) {
      if (is_split(tile->body)) {
        nm_clip_y(tile, area_.y1, 1);
      } else {
        Tile* newtile = plane_.split_y(tile, area_.y1);
        newtile->body = tile->body;
      }
      flags_ &= ~kMrgTop;
    }
    return after_vertical_clip(tile);
  }

  Step clip_down(Tile*& tile) {
    if (bottom(tile) < area_.y0) {
      if (is_split(tile->body)) {
        nm_clip_y(tile, area_.y0, 0);
      } else {
        Tile* lower = tile;
        tile = plane_.split_y(tile, area_.y0);
        tile->body = lower->body;
      }
      flags_ &= ~kMrgBottom;
    }
    return after_vertical_clip(tile);
  }

  // Merges the outside tile left by a Manhattan x split up and down.
  void merge_outside_vertically(Tile* outside) {
    Tile* tp = outside->rt;
    if (can_merge_y(outside, tp)) {
      plane_.join_y(outside, tp);
    }
    tp = outside->lb;
    if (can_merge_y(outside, tp)) {
      plane_.join_y(outside, tp);
    }
  }

  Step clip_right(Tile*& tile) {
    if (right(tile) > area_.x1) {
      if (is_split(tile->body)) {
        nm_clip_right(tile);
      } else {
        Tile* newtile = plane_.split_x(tile, area_.x1);
        newtile->body = tile->body;
        merge_outside_vertically(newtile);
      }
      flags_ &= ~kMrgRight;
    }
    if (bottom(tile) >= area_.y1 || right(tile) <= area_.x0) {
      return Step::paint_done;
    }
    return old_ == new_ ? Step::clip_done : Step::next;
  }

  void nm_clip_right(Tile*& tile) {
    Tile* newtile = nullptr;
    nm_split_x(plane_, tile, newtile, area_.x1, 1);
    if (!is_split(tile->body)) {
      recompute(tile);
      if ((flags_ & kMrgLeft) != 0) {
        tile = nm_merge_left(plane_, tile);
      }
      if ((flags_ & kMrgRight) != 0) {
        nm_merge_right(plane_, newtile->lb);
      }
    } else {
      if ((flags_ & kMrgLeft) != 0) {
        nm_merge_right(plane_, newtile);
      }
      if ((flags_ & kMrgRight) != 0) {
        nm_merge_left(plane_, tile->lb);
      }
    }
  }

  Step clip_left(Tile*& tile) {
    if (left(tile) < area_.x0) {
      if (is_split(tile->body)) {
        Tile* newtile = nullptr;
        nm_split_x(plane_, tile, newtile, area_.x0, 0);
        if (!is_split(tile->body)) {
          recompute(tile);
          if ((flags_ & kMrgLeft) != 0) {
            nm_merge_left(plane_, newtile->lb);
          }
        } else if ((flags_ & kMrgLeft) != 0) {
          nm_merge_left(plane_, newtile);
        }
      } else {
        Tile* outside = tile;
        tile = plane_.split_x(tile, area_.x0);
        tile->body = outside->body;
        merge_outside_vertically(outside);
      }
      flags_ &= ~kMrgLeft;
    }
    return bottom(tile) >= area_.y1 ? Step::paint_done : Step::clip_done;
  }

  // The "clipdone:" part: diagonal collapse, merges and the type change.
  Tile* change_and_merge(Tile* tile) {
    if ((new_ & kDiagonalBit) != 0) {
      if ((new_ & kLeftMask) == ((new_ & kRightMask) >> kRightShift)) {
        new_ &= kLeftMask;
        tile->body = new_;
        flags_ |= kMrgLeft;
        if (right(tile) >= area_.x1) {
          flags_ |= kMrgRight;
        }
      } else {
        flags_ = 0;
      }
    }
    if (needs_side_merge(tile)) {
      return paint_merge(tile);
    }
    tile->body = new_;
    if ((flags_ & kMrgTop) != 0 && can_merge_y(tile, tile->rt)) {
      plane_.join_y(tile, tile->rt);
    }
    if ((flags_ & kMrgBottom) != 0 && can_merge_y(tile, tile->lb)) {
      plane_.join_y(tile, tile->lb);
    }
    return tile;
  }

  // True when a left or right neighbor has the new type (dbPaintMerge needed).
  bool needs_side_merge(const Tile* tile) {
    if ((flags_ & kMrgLeft) != 0) {
      for (const Tile* tp = tile->bl; bottom(tp) < top(tile); tp = tp->rt) {
        if (tp->body == new_) {
          return true;
        }
      }
      flags_ &= ~kMrgLeft;
    }
    if ((flags_ & kMrgRight) != 0) {
      for (const Tile* tp = tile->tr; top(tp) > bottom(tile); tp = tp->lb) {
        if (tp->body == new_) {
          return true;
        }
      }
      flags_ &= ~kMrgRight;
    }
    return false;
  }

  // dbPaintMerge: the lowest y from which `tile` can merge left.
  Coord left_split_point(const Tile* tile, Coord ysplit) {
    const Tile* last = nullptr;
    for (const Tile* tp = tile->bl; bottom(tp) < top(tile); tp = tp->rt) {
      if (tp->body == new_) {
        last = tp;
      }
    }
    if (last == nullptr || top(last) < top(tile)) {
      flags_ &= ~kMrgLeft;
      if (last != nullptr && top(last) > ysplit) {
        ysplit = top(last);
      }
    } else if (bottom(last) > ysplit) {
      ysplit = bottom(last);
    }
    return ysplit;
  }

  Coord right_split_point(const Tile* tile, Coord ysplit) {
    const Tile* tp = tile->tr;
    if (tp->body == new_) {
      return std::max(ysplit, bottom(tp));
    }
    do {
      tp = tp->lb;
    } while (tp->body != new_ && top(tp) > ysplit);
    flags_ &= ~kMrgRight;
    return std::max(ysplit, top(tp));
  }

  // Joins `tile` with the single neighbor `tp` of type new_ beside it.
  void merge_side(Tile* tile, Tile* tp) {
    if (top(tp) > top(tile)) {
      Tile* upper = plane_.split_y(tp, top(tile));
      upper->body = new_;
    }
    if (bottom(tp) < bottom(tile)) {
      tp = plane_.split_y(tp, bottom(tile));
    }
    plane_.join_x(tile, tp);
  }

  Tile* paint_merge(Tile* tile) {
    Coord ysplit = bottom(tile);
    if ((flags_ & kMrgLeft) != 0) {
      ysplit = left_split_point(tile, ysplit);
    }
    if ((flags_ & kMrgRight) != 0) {
      ysplit = right_split_point(tile, ysplit);
    }
    if (ysplit > bottom(tile)) {
      flags_ &= ~kMrgBottom;
      Tile* tp = plane_.split_y(tile, ysplit);
      tp->body = tile->body;
      tile = tp;
    }
    tile->body = new_;
    if ((flags_ & kMrgLeft) != 0) {
      merge_side(tile, tile->bl);
    }
    if ((flags_ & kMrgRight) != 0) {
      merge_side(tile, tile->tr);
    }
    if ((flags_ & kMrgTop) != 0 && can_merge_y(tile->rt, tile)) {
      plane_.join_y(tile, tile->rt);
    }
    if ((flags_ & kMrgBottom) != 0 && can_merge_y(tile->lb, tile)) {
      plane_.join_y(tile, tile->lb);
    }
    return tile;
  }

  TilePlane& plane_;
  const Rect area_;
  const ResultRow& row_;
  unsigned flags_ = 0;
  Body old_ = 0;
  Body new_ = 0;
};

}  // namespace

void paint_plane(TilePlane& plane, const Rect& area, const ResultRow& row) {
  if (area.x1 <= area.x0 || area.y1 <= area.y0) {
    return;
  }
  Painter(plane, area, row).run();
}

}  // namespace hefesto::db::detail
