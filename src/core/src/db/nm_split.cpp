// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Splitting split (triangle) tiles along x or y and re-merging the
// Manhattan pieces (layout-geometry-model "Non-Manhattan (split tile)
// geometry", layout-paint-semantics "Non-Manhattan painting").
// Ported from Magic VLSI 8.3.694 (fc5daa9e) database/DBpaint.c: TiNMSplitY,
// TiNMSplitX (their dbSplitUndo records go to TilePlane::note_split),
// TiNMMergeRight, TiNMMergeLeft. Magic is
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).

#include <cstdint>
#include <utility>

#include "db/tile_plane.hpp"

namespace hefesto::db::detail {

namespace {

// Offset of the diagonal crossing, rounded half up (TiNMSplitX/Y).
Coord rounded_offset(std::int64_t num, std::int64_t den) noexcept {
  const std::int64_t twice_rem = (num % den) * 2;
  const std::int64_t round = twice_rem >= den ? 1 : 0;
  return static_cast<Coord>(num / den + round);
}

// The x split of a y-split tile is impossible: the diagonal crosses at a corner.
void assign_y_corner_case(Tile* oldtile, Tile* newtile, Coord x) {
  const Body body = oldtile->body;
  newtile->body = body;
  if (x == left(oldtile)) {
    (direction(newtile->body) != 0 ? newtile : oldtile)->body = right_type(body);
  } else {
    (direction(newtile->body) != 0 ? oldtile : newtile)->body = left_type(body);
  }
}

void assign_x_corner_case(Tile* oldtile, Tile* newtile, Coord y) {
  const Body body = oldtile->body;
  newtile->body = body;
  if (y == bottom(oldtile)) {
    if (direction(newtile->body) != 0) {
      newtile->body = right_type(body);
    } else {
      oldtile->body = left_type(body);
    }
  } else if (direction(newtile->body) != 0) {
    oldtile->body = left_type(body);
  } else {
    newtile->body = right_type(body);
  }
}

}  // namespace

void nm_split_y(TilePlane& plane, Tile*& oldtile, Tile*& newtile, Coord y, int dir) {
  const std::int64_t height = std::int64_t{top(oldtile)} - bottom(oldtile);
  const std::int64_t tmpdx = (std::int64_t{y} - bottom(oldtile)) * (std::int64_t{right(oldtile)} - left(oldtile));
  const Coord delx = rounded_offset(tmpdx, height);
  const Coord x = direction(oldtile->body) != 0 ? right(oldtile) - delx : left(oldtile) + delx;

  const Rect before = tile_rect(oldtile);
  newtile = plane.split_y(oldtile, y);
  if (x > left(oldtile) && x < right(oldtile)) {
    plane.note_split(false, before, oldtile->body);  // dbSplitUndo
    Tile* newxbot = plane.split_x(oldtile, x);
    Tile* newxtop = plane.split_x(newtile, x);
    const Body body = oldtile->body;
    if (direction(body) != 0) {
      newxbot->body = body;
      newtile->body = body;
      newxtop->body = right_type(body);
      oldtile->body = left_type(body);
    } else {
      newxtop->body = body;
      newxbot->body = right_type(body);
      newtile->body = left_type(body);
    }
  } else {
    assign_y_corner_case(oldtile, newtile, x);
  }
  if (dir == 0) {
    std::swap(oldtile, newtile);
  }
}

void nm_split_x(TilePlane& plane, Tile*& oldtile, Tile*& newtile, Coord x, int dir) {
  const std::int64_t width = std::int64_t{right(oldtile)} - left(oldtile);
  const std::int64_t tmpdy = (std::int64_t{x} - left(oldtile)) * (std::int64_t{top(oldtile)} - bottom(oldtile));
  const Coord dely = rounded_offset(tmpdy, width);
  const Coord y = direction(oldtile->body) != 0 ? top(oldtile) - dely : bottom(oldtile) + dely;

  const Rect before = tile_rect(oldtile);
  newtile = plane.split_x(oldtile, x);
  if (y > bottom(oldtile) && y < top(oldtile)) {
    plane.note_split(false, before, oldtile->body);  // dbSplitUndo
    Tile* newyleft = oldtile;
    oldtile = plane.split_y(newyleft, y);
    Tile* newyright = newtile;
    newtile = plane.split_y(newyright, y);
    const Body body = newyleft->body;
    if (direction(body) != 0) {
      oldtile->body = body;
      newyright->body = body;
      newtile->body = right_type(body);
      newyleft->body = left_type(body);
    } else {
      newtile->body = body;
      newyright->body = right_type(body);
      oldtile->body = left_type(body);
    }
  } else {
    assign_x_corner_case(oldtile, newtile, y);
  }
  if (dir == 0) {
    std::swap(oldtile, newtile);
  }
}

namespace {

// TiNMMergeRight loop body: joins one same-type neighbor `tp` fully beside
// `tile` (splitting `tile` to its extent).
void merge_right_neighbor(TilePlane& plane, Tile* tile, Tile* tp, Body ttype) {
  if (top(tp) < top(tile)) {
    plane.split_y(tile, top(tp))->body = ttype;
  }
  Tile* newtile = tile;
  if (bottom(tp) > bottom(tile)) {
    newtile = plane.split_y(tile, bottom(tp));
    newtile->body = ttype;
  }
  plane.join_x(newtile, tp);
}

// TiNMMergeRight tail: the neighbor straddling the bottom of `tile`.
void merge_right_bottom(TilePlane& plane, Tile* tile, Tile* tp, Body ttype) {
  if (top(tp) < top(tile)) {
    plane.split_y(tile, top(tp))->body = ttype;
  }
  Tile* newtile = plane.split_y(tp, bottom(tile));
  newtile->body = ttype;
  plane.join_x(tile, newtile);
  if (can_merge_y(tile, tile->rt)) {
    plane.join_y(tile, tile->rt);
  }
}

// TiNMMergeLeft loop body; returns the part of `tile` above `tp`.
Tile* merge_left_neighbor(TilePlane& plane, Tile* tile, Tile* tp, Body ttype) {
  if (bottom(tile) < bottom(tp)) {
    tile = plane.split_y(tile, bottom(tp));
    tile->body = ttype;
  }
  Tile* newtile = tile;
  if (top(tp) < top(tile)) {
    newtile = plane.split_y(tile, top(tp));
    newtile->body = ttype;
  }
  plane.join_x(tile, tp);
  return newtile;
}

// TiNMMergeLeft tail: the neighbor straddling the top of `tile`, or a
// vertical merge with the tile above.
Tile* merge_left_top(TilePlane& plane, Tile* tile, Tile* tp, Body ttype) {
  if (bottom(tp) >= top(tile)) {
    if (can_merge_y(tile, tp)) {
      plane.join_y(tile, tp);
    }
    return tile;
  }
  if (tp->body != ttype) {
    return tile;
  }
  if (bottom(tile) < bottom(tp)) {
    tile = plane.split_y(tile, bottom(tp));
    tile->body = ttype;
  }
  plane.split_y(tp, top(tile))->body = ttype;
  plane.join_x(tile, tp);
  return tile;
}

}  // namespace

Tile* nm_merge_right(TilePlane& plane, Tile* tile) {
  const Body ttype = tile->body;
  Tile* tp = tile->tr;
  if (top(tp) > top(tile) && tp->body == ttype) {
    plane.split_y(tp, top(tile))->body = ttype;
  }
  while (bottom(tp) >= bottom(tile)) {
    Tile* tp2 = tp->lb;
    if (tp->body == ttype) {
      merge_right_neighbor(plane, tile, tp, ttype);
    }
    tp = tp2;
  }
  if (top(tp) > bottom(tile) && tp->body == ttype) {
    merge_right_bottom(plane, tile, tp, ttype);
  }
  return tile;
}

Tile* nm_merge_left(TilePlane& plane, Tile* tile) {
  const Body ttype = tile->body;
  Tile* tp = tile->bl;
  if (bottom(tp) < bottom(tile) && tp->body == ttype) {
    tp = plane.split_y(tp, bottom(tile));
    tp->body = ttype;
  }
  while (top(tp) <= top(tile)) {
    Tile* tp2 = tp->rt;
    if (tp->body == ttype) {
      tile = merge_left_neighbor(plane, tile, tp, ttype);
    }
    tp = tp2;
  }
  return merge_left_top(plane, tile, tp, ttype);
}

}  // namespace hefesto::db::detail
