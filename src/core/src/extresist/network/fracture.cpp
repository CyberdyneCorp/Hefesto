// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResFracture (capability extresist-network "Tile fracturing"): the
// area enumeration over the scratch plane, the four concave-corner cases
// of ResCheckConcavity, the walks and ResSplitX, on db::StitchEditor.
// Split tiles at a cut (Q-NETLIST-53, compat): with magic-compat ON the
// tile is made Manhattan with its left type and then cut (what ResSplitX's
// TiGetType gives the new tile); with it OFF a split tile is never cut.
// (Magic ref: resis/ResFract.c:43-493)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include "hefesto/db/plane_stitch.hpp"
#include "hefesto/extresist/network/network.hpp"

namespace hefesto::extresist {

namespace {

using db::Stitch;
using db::StitchTile;

class Fracturer {
 public:
  Fracturer(db::StitchEditor& ed, bool magic_compat) : ed_(ed), compat_(magic_compat) {}

  void run(const db::Rect& r) {
    src_ = ed_.tile_at(db::Point{r.x0, r.y1 - 1}, ed_.hint());
    while (src_ && top(src_) > r.y0) {
      if (!row(r)) {
        return;
      }
      src_ = ed_.stitch(src_, Stitch::lb);
      while (src_ && right(src_) <= r.x0) {
        src_ = ed_.stitch(src_, Stitch::tr);
      }
    }
  }

 private:
  [[nodiscard]] db::Coord left(StitchTile t) const { return ed_.area(t).x0; }
  [[nodiscard]] db::Coord right(StitchTile t) const { return ed_.area(t).x1; }
  [[nodiscard]] db::Coord bottom(StitchTile t) const { return ed_.area(t).y0; }
  [[nodiscard]] db::Coord top(StitchTile t) const { return ed_.area(t).y1; }
  [[nodiscard]] tech::TileType type(StitchTile t) const { return ed_.type(t); }
  [[nodiscard]] StitchTile at(db::Coord x, db::Coord y, StitchTile from) const {
    return ed_.tile_at(db::Point{x, y}, from);
  }

  /// One pass of the enumeration from the current left-edge tile; false
  /// when the enumeration is over.
  bool row(const db::Rect& r) {
    while (true) {
      ed_.set_hint(src_);
      if (type(src_) != tech::kSpace) {
        process();
      }
      StitchTile next = ed_.stitch(src_, Stitch::tr);
      if (next && left(next) < r.x1) {
        while (next && bottom(next) >= r.y1) {
          next = ed_.stitch(next, Stitch::lb);
        }
        if (next && (bottom(next) >= bottom(src_) || bottom(src_) <= r.y0)) {
          src_ = next;
          continue;
        }
      }
      if (!back_left(r)) {
        return false;
      }
      if (!resumed_) {
        return true;
      }
    }
  }

  /// Returns one tile further left; sets resumed_ when the enumeration
  /// continues from a tile below. False when the enumeration is over.
  bool back_left(const db::Rect& r) {
    resumed_ = false;
    while (left(src_) > r.x0) {
      if (bottom(src_) <= r.y0) {
        return false;
      }
      const StitchTile below = ed_.stitch(src_, Stitch::lb);
      src_ = ed_.stitch(src_, Stitch::bl);
      if (!below || !src_) {
        return false;
      }
      if (bottom(below) >= bottom(src_) || bottom(src_) <= r.y0) {
        src_ = below;
        resumed_ = true;
        return true;
      }
    }
    return true;
  }

  void process() {
    const tech::TileType tt = type(src_);
    top_ = ed_.stitch(src_, Stitch::rt);
    while (top_ && right(top_) > left(src_)) {
      if (type(top_) != tt) {
        top_ = ed_.stitch(top_, Stitch::bl);
        continue;
      }
      concavity(src_, top_, tt);
      if (!top_) {
        break;
      }
      top_ = bottom(top_) != top(src_) ? ed_.stitch(src_, Stitch::rt) : ed_.stitch(top_, Stitch::bl);
    }
  }

  void concavity(StitchTile bot, StitchTile up, tech::TileType tt) {
    corner_top_right(bot, up, tt);
    if (top_) {
      corner_bottom_right(bot, up, tt);
    }
    if (top_) {
      corner_top_left(bot, up, tt);
    }
    if (top_) {
      corner_bottom_left(bot, up, tt);
    }
  }

  // corner #1: the top tile extends right of the bottom tile.
  void corner_top_right(StitchTile bot, StitchTile up, tech::TileType tt) {
    if (!(right(up) > right(bot) && type(ed_.stitch(bot, Stitch::tr)) != tt)) {
      return;
    }
    const db::Coord x = right(bot);
    const db::Coord y = bottom(up);
    if (x - walk_left(up, tt, x, y) > walk_up(up, tt, x, y, false) - y) {
      walk_up(up, tt, x, y, true);
    }
  }

  // corner #2: the bottom tile extends right of the top tile.
  void corner_bottom_right(StitchTile bot, StitchTile up, tech::TileType tt) {
    if (!(right(up) < right(bot))) {
      return;
    }
    StitchTile tp = ed_.stitch(up, Stitch::tr);
    while (tp && bottom(tp) > bottom(up)) {
      tp = ed_.stitch(tp, Stitch::lb);
    }
    if (type(tp) == tt) {
      return;
    }
    const db::Coord x = right(up);
    const db::Coord y = bottom(up);
    if (x - walk_left(up, tt, x, y) > y - walk_down(bot, tt, x, y, false)) {
      walk_down(bot, tt, x, y, true);
    }
  }

  // corner #3: the top tile extends left of the bottom tile.
  void corner_top_left(StitchTile bot, StitchTile up, tech::TileType tt) {
    if (!(left(up) < left(bot))) {
      return;
    }
    StitchTile tp = ed_.stitch(bot, Stitch::bl);
    while (tp && top(tp) < top(bot)) {
      tp = ed_.stitch(tp, Stitch::rt);
    }
    if (type(tp) == tt) {
      return;
    }
    const db::Coord x = left(bot);
    const db::Coord y = bottom(up);
    if (walk_right(up, tt, x, y) - x > walk_up(up, tt, x, y, false) - y) {
      walk_up(up, tt, x, y, true);
    }
  }

  // corner #4: the bottom tile extends left of the top tile.
  void corner_bottom_left(StitchTile bot, StitchTile up, tech::TileType tt) {
    if (!(left(up) > left(bot) && type(ed_.stitch(up, Stitch::bl)) != tt)) {
      return;
    }
    const db::Coord x = left(up);
    const db::Coord y = bottom(up);
    if (walk_right(up, tt, x, y) - x > y - walk_down(bot, tt, x, y, false)) {
      walk_down(bot, tt, x, y, true);
    }
  }

  db::Coord walk_up(StitchTile tile, tech::TileType tt, db::Coord x, db::Coord y, bool split) {
    while (tile && type(tile) == tt) {
      if (x == left(tile)) {
        StitchTile tp = ed_.stitch(tile, Stitch::bl);
        while (tp && top(tp) <= y) {
          tp = ed_.stitch(tp, Stitch::rt);
        }
        for (; tp && bottom(tp) < top(tile); tp = ed_.stitch(tp, Stitch::rt)) {
          if (type(tp) != tt) {
            return bottom(tp);
          }
        }
      } else if (split) {
        tile = split_x(tile, x);
      }
      tile = at(x, top(tile), tile);
    }
    return tile ? bottom(tile) : db::kInfinity;
  }

  db::Coord walk_down(StitchTile tile, tech::TileType tt, db::Coord x, db::Coord y, bool split) {
    while (tile && type(tile) == tt) {
      if (x == left(tile)) {
        StitchTile end;
        for (StitchTile tp = ed_.stitch(tile, Stitch::bl); tp && bottom(tp) < top(tile);
             tp = ed_.stitch(tp, Stitch::rt)) {
          if (type(tp) != tt && bottom(tp) < y) {
            end = tp;
          }
        }
        if (end) {
          return top(end);
        }
      } else if (split) {
        tile = split_x(tile, x);
      }
      tile = at(x, bottom(tile) - 1, tile);
    }
    return tile ? top(tile) : -db::kInfinity;
  }

  db::Coord walk_right(StitchTile tile, tech::TileType tt, db::Coord x, db::Coord y) {
    while (tile && type(tile) == tt) {
      if (y == bottom(tile)) {
        StitchTile tp = ed_.stitch(tile, Stitch::lb);
        while (tp && left(tp) < x) {
          tp = ed_.stitch(tp, Stitch::tr);
        }
        for (; tp && left(tp) < right(tile); tp = ed_.stitch(tp, Stitch::tr)) {
          if (type(tp) != tt) {
            return left(tp);
          }
        }
      }
      tile = at(right(tile), y, tile);
    }
    return tile ? left(tile) : db::kInfinity;
  }

  db::Coord walk_left(StitchTile tile, tech::TileType tt, db::Coord x, db::Coord y) {
    while (tile && type(tile) == tt) {
      if (y == bottom(tile)) {
        StitchTile end;
        for (StitchTile tp = ed_.stitch(tile, Stitch::lb); tp && left(tp) < right(tile);
             tp = ed_.stitch(tp, Stitch::tr)) {
          if (type(tp) != tt && left(tp) < x) {
            end = tp;
          }
        }
        if (end) {
          return right(end);
        }
      }
      tile = at(left(tile) - 1, y, tile);
    }
    return tile ? right(tile) : -db::kInfinity;
  }

  /// TiJoinY1 of `gone` into `keep` when `other` is a same-type tile with
  /// the same LEFT and RIGHT as `tile`.
  [[nodiscard]] bool joinable(StitchTile other, StitchTile tile, tech::TileType tt) const {
    return other && type(other) == tt && left(other) == left(tile) && right(other) == right(tile);
  }

  /// ResSplitX.
  StitchTile split_x(StitchTile tile, db::Coord x) {
    const tech::TileType tt = type(tile);
    if (ed_.view(tile).split) {
      if (!compat_) {
        return tile;  // Q-NETLIST-53 OFF: a split tile is never cut
      }
      ed_.set_type(tile, tt);  // Q-NETLIST-53 ON: Manhattan, its left type
    }
    StitchTile tp = ed_.split_x(tile, x, compat_);
    if (!tp) {
      return tile;
    }
    ed_.set_type(tp, tt);
    for (const Stitch s : {Stitch::rt, Stitch::lb}) {
      const StitchTile tp2 = ed_.stitch(tile, s);
      if (!joinable(tp2, tile, tt)) {
        continue;
      }
      if (tp2 == src_) {
        if (top_ == tile) {
          top_ = StitchTile{};
        }
        ed_.join_y(tp2, tile);
        tile = tp2;
      } else {
        if (top_ == tp2) {
          top_ = StitchTile{};
        }
        ed_.join_y(tile, tp2);
      }
    }
    StitchTile above = ed_.stitch(tp, Stitch::rt);
    if (joinable(above, tp, tt)) {
      ed_.join_y(above, tp);
      tp = above;
    }
    const StitchTile below = ed_.stitch(tp, Stitch::lb);
    if (joinable(below, tp, tt)) {
      ed_.join_y(below, tp);
    }
    return tile;
  }

  db::StitchEditor& ed_;
  bool compat_;
  StitchTile src_;  // resSrTile
  StitchTile top_;  // resTopTile
  bool resumed_ = false;
};

}  // namespace

diag::Status fracture(db::StitchEditor& editor, const db::Rect& area) {
  return fracture(editor, area, true);
}

diag::Status fracture(db::StitchEditor& editor, const db::Rect& area, bool magic_compat) {
  if (area.is_null()) {
    return diag::Done{};
  }
  Fracturer(editor, magic_compat).run(area);
  editor.release();
  return diag::Done{};
}

}  // namespace hefesto::extresist
