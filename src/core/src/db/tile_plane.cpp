// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Corner-stitch primitives and area searches.
// Ported from Magic VLSI 8.3.694 (fc5daa9e): tiles/tile.c (TiNewPlane,
// TiSplitX, TiSplitY, TiJoinX, TiJoinY), tiles/tile.h (GOTOPOINT),
// database/DBcellsubr.c (DBNewPlane), database/DBtiles.c (DBSrPaintArea,
// DBSrPaintNMArea, DBTestNMInteract, dbEvalCorner). Magic is Copyright (C)
// 1985, 1990 Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).

#include "db/tile_plane.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>

namespace hefesto::db::detail {

namespace {

constexpr Coord kInf = kInfinity;
constexpr Coord kMinf = kMinusInfinity;

}  // namespace

Tile* TilePlane::alloc() {
  if (!free_.empty()) {
    Tile* t = free_.back();
    free_.pop_back();
    *t = Tile{};
    return t;
  }
  storage_.emplace_back();
  return &storage_.back();
}

void TilePlane::note_split(bool join, const Rect& area, Body body) {
  if (split_log_ == nullptr) {
    return;
  }
  split_log_->push_back(SplitEdit{join, area, direction(body) != 0 ? Diagonal::nw_se : Diagonal::ne_sw,
                                  static_cast<tech::TileType>(left_type(body)),
                                  static_cast<tech::TileType>(right_type(body))});
}

void TilePlane::end_operation() {
  free_.insert(free_.end(), pending_.begin(), pending_.end());
  pending_.clear();
}

TilePlane::TilePlane() {
  top_ = alloc();
  right_ = alloc();
  bottom_ = alloc();
  left_ = alloc();
  infinity_ = alloc();
  infinity_->x = kInf + 1;
  infinity_->y = kInf + 1;

  Tile* tile = alloc();  // DBNewPlane: the central space tile
  tile->body = 0;
  tile->x = kPlaneMin;
  tile->y = kPlaneMin;
  tile->rt = top_;
  tile->tr = right_;
  tile->lb = bottom_;
  tile->bl = left_;

  *bottom_ = Tile{kBoundaryBody, nullptr, left_, right_, tile, kMinf, kMinf};
  *top_ = Tile{kBoundaryBody, tile, left_, right_, infinity_, kMinf, kInf};
  *left_ = Tile{kBoundaryBody, bottom_, nullptr, tile, top_, kMinf, kMinf};
  *right_ = Tile{kBoundaryBody, bottom_, tile, infinity_, top_, kInf, kMinf};
  hint_ = tile;
}

Tile* TilePlane::split_x(Tile* tile, Coord x) {
  Tile* nt = alloc();
  nt->body = 0;
  nt->x = x;
  nt->y = tile->y;
  nt->bl = tile;
  nt->tr = tile->tr;
  nt->rt = tile->rt;

  Tile* tp = nullptr;
  for (tp = tile->tr; tp->bl == tile; tp = tp->lb) {  // right edge
    tp->bl = nt;
  }
  tile->tr = nt;
  for (tp = tile->rt; left(tp) >= x; tp = tp->bl) {  // top edge
    tp->lb = nt;
  }
  tile->rt = tp;
  for (tp = tile->lb; right(tp) <= x; tp = tp->tr) {  // bottom edge
  }
  nt->lb = tp;
  while (tp->rt == tile) {
    tp->rt = nt;
    tp = tp->tr;
  }
  return nt;
}

Tile* TilePlane::split_y(Tile* tile, Coord y) {
  Tile* nt = alloc();
  nt->body = 0;
  nt->x = tile->x;
  nt->y = y;
  nt->lb = tile;
  nt->rt = tile->rt;
  nt->tr = tile->tr;

  Tile* tp = nullptr;
  for (tp = tile->rt; tp->lb == tile; tp = tp->bl) {  // top edge
    tp->lb = nt;
  }
  tile->rt = nt;
  for (tp = tile->tr; bottom(tp) >= y; tp = tp->lb) {  // right edge
    tp->bl = nt;
  }
  tile->tr = tp;
  for (tp = tile->bl; top(tp) <= y; tp = tp->rt) {  // left edge
  }
  nt->bl = tp;
  while (tp->tr == tile) {
    tp->tr = nt;
    tp = tp->rt;
  }
  return nt;
}

void TilePlane::join_x(Tile* keep, Tile* gone) {
  Tile* tp = nullptr;
  for (tp = gone->rt; tp->lb == gone; tp = tp->bl) {
    tp->lb = keep;
  }
  for (tp = gone->lb; tp->rt == gone; tp = tp->tr) {
    tp->rt = keep;
  }
  if (left(keep) < left(gone)) {
    for (tp = gone->tr; tp->bl == gone; tp = tp->lb) {
      tp->bl = keep;
    }
    keep->tr = gone->tr;
    keep->rt = gone->rt;
  } else {
    for (tp = gone->bl; tp->tr == gone; tp = tp->rt) {
      tp->tr = keep;
    }
    keep->bl = gone->bl;
    keep->lb = gone->lb;
    keep->x = gone->x;
  }
  if (hint_ == gone) {
    hint_ = keep;
  }
  free_later(gone);
}

void TilePlane::join_y(Tile* keep, Tile* gone) {
  Tile* tp = nullptr;
  for (tp = gone->tr; tp->bl == gone; tp = tp->lb) {
    tp->bl = keep;
  }
  for (tp = gone->bl; tp->tr == gone; tp = tp->rt) {
    tp->tr = keep;
  }
  if (bottom(keep) < bottom(gone)) {
    for (tp = gone->rt; tp->lb == gone; tp = tp->bl) {
      tp->lb = keep;
    }
    keep->rt = gone->rt;
    keep->tr = gone->tr;
  } else {
    for (tp = gone->lb; tp->rt == gone; tp = tp->tr) {
      tp->rt = keep;
    }
    keep->lb = gone->lb;
    keep->bl = gone->bl;
    keep->y = gone->y;
  }
  if (hint_ == gone) {
    hint_ = keep;
  }
  free_later(gone);
}

void TilePlane::for_each_tile(const std::function<void(const Tile*)>& visit) const {
  const Rect all = whole_plane();
  Tile* tile = area_first(const_cast<TilePlane&>(*this), all);
  while (tile != nullptr) {
    visit(tile);
    tile = area_next(tile, all, std::min(top(tile), all.y1));
  }
}

void TilePlane::copy_from(const TilePlane& other) {
  std::vector<const Tile*> tiles;
  other.for_each_tile([&](const Tile* t) { tiles.push_back(t); });
  std::unordered_map<const Tile*, Tile*> map{
      {other.left_, left_}, {other.top_, top_}, {other.right_, right_}, {other.bottom_, bottom_},
      {other.infinity_, infinity_}, {nullptr, nullptr}};
  // The central tile of this fresh plane is reused for the first copied tile.
  Tile* central = hint_;
  for (std::size_t i = 0; i < tiles.size(); ++i) {
    map.emplace(tiles[i], i == 0 ? central : alloc());
  }
  for (const Tile* src : tiles) {
    Tile* dst = map.at(src);
    *dst = Tile{src->body, map.at(src->lb), map.at(src->bl), map.at(src->tr), map.at(src->rt), src->x, src->y};
  }
  for (const Tile* boundary : {other.left_, other.top_, other.right_, other.bottom_}) {
    Tile* dst = map.at(boundary);
    dst->lb = map.at(boundary->lb);
    dst->bl = map.at(boundary->bl);
    dst->tr = map.at(boundary->tr);
    dst->rt = map.at(boundary->rt);
  }
  hint_ = map.at(tiles.front());
}

namespace {

// GOTOPOINT, first half: the tile row containing p.y.
Tile* goto_row(Tile* tp, Point p) noexcept {
  if (p.y < bottom(tp)) {
    do {
      tp = tp->lb;
    } while (p.y < bottom(tp));
    return tp;
  }
  while (p.y >= top(tp)) {
    tp = tp->rt;
  }
  return tp;
}

// GOTOPOINT, moving left until the tile contains p.
Tile* goto_left(Tile* tp, Point p) noexcept {
  do {
    do {
      tp = tp->bl;
    } while (p.x < left(tp));
    if (p.y < top(tp)) {
      break;
    }
    do {
      tp = tp->rt;
    } while (p.y >= top(tp));
  } while (p.x < left(tp));
  return tp;
}

// GOTOPOINT, moving right until the tile contains p.
Tile* goto_right(Tile* tp, Point p) noexcept {
  while (p.x >= right(tp)) {
    do {
      tp = tp->tr;
    } while (p.x >= right(tp));
    if (p.y >= bottom(tp)) {
      break;
    }
    do {
      tp = tp->lb;
    } while (p.y < bottom(tp));
  }
  return tp;
}

}  // namespace

Tile* goto_point(Tile* tp, Point p) noexcept {
  tp = goto_row(tp, p);
  return p.x < left(tp) ? goto_left(tp, p) : goto_right(tp, p);
}

Tile* area_first(TilePlane& plane, const Rect& area) noexcept {
  Tile* tile = goto_point(plane.hint(), Point{area.x0, area.y1 - 1});
  return top(tile) > area.y0 ? tile : nullptr;
}

Tile* area_next(Tile* tile, const Rect& area, Coord clip_top) noexcept {
  Tile* tpnew = tile->tr;
  if (left(tpnew) < area.x1) {
    while (bottom(tpnew) >= clip_top) {
      tpnew = tpnew->lb;
    }
    if (bottom(tpnew) >= bottom(tile) || bottom(tile) <= area.y0) {
      return tpnew;
    }
  }
  while (left(tile) > area.x0) {
    if (bottom(tile) <= area.y0) {
      return nullptr;
    }
    tpnew = tile->lb;
    tile = tile->bl;
    if (bottom(tpnew) >= bottom(tile) || bottom(tile) <= area.y0) {
      return tpnew;
    }
  }
  for (tile = tile->lb; right(tile) <= area.x0; tile = tile->tr) {
  }
  return top(tile) > area.y0 ? tile : nullptr;
}

namespace {

constexpr std::int64_t kDlongMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kDlongMin = std::numeric_limits<std::int64_t>::min();

// The split-tile part of DBSrPaintArea: which sides are reported.
bool visit_split_sides(Tile* tp, const Rect& rect, const tech::TypeMask& mask,
                       const std::function<bool(Tile*, Body)>& visit) {
  const std::int64_t theight = std::int64_t{top(tp)} - bottom(tp);
  const std::int64_t twidth = std::int64_t{right(tp)} - left(tp);
  const std::int64_t f1 = rect.y0 > kMinf + 2 ? (std::int64_t{top(tp)} - rect.y0) * twidth : kDlongMax;
  const std::int64_t f2 = rect.y1 < kInf - 2 ? (std::int64_t{rect.y1} - bottom(tp)) * twidth : kDlongMax;
  const int dir = direction(tp->body);
  if (mask.test(static_cast<tech::TileType>(left_type(tp->body)))) {
    const std::int64_t f4 = rect.x0 > kMinf + 2 ? (std::int64_t{rect.x0} - left(tp)) * theight : kDlongMin;
    if ((dir != 0 ? f1 > f4 : f2 > f4) && visit(tp, kDiagonalBit)) {
      return true;
    }
  }
  if (mask.test(static_cast<tech::TileType>(right_type(tp->body)))) {
    const std::int64_t f3 = rect.x1 < kInf - 2 ? (std::int64_t{right(tp)} - rect.x1) * theight : kDlongMin;
    if ((dir != 0 ? f2 > f3 : f1 > f3) && visit(tp, kDiagonalBit | kSideBit)) {
      return true;
    }
  }
  return false;
}

// dbEvalCorner: sign of the position of `p` relative to the diagonal of `r`.
int eval_corner(Point p, const Rect& r, Body di) noexcept {
  const std::int64_t w = r.width();
  const std::int64_t h = r.height();
  const std::int64_t dx = (di & kDirectionBit) != 0 ? std::int64_t{r.x1} - p.x : std::int64_t{p.x} - r.x0;
  const std::int64_t d = (std::int64_t{p.y} - r.y0) * w - dx * h;
  return d > 0 ? 1 : (d < 0 ? -1 : 0);
}

bool side_matches_direction(Body t) noexcept {
  return ((t & kSideBit) != 0) == ((t & kDirectionBit) != 0);
}

// Outside-of-triangle check of DBTestNMInteract (sub-integer slivers ignored).
bool outside_triangle(const Rect& r1, Body tt1, const Rect& r2) noexcept {
  const std::int64_t rheight = r1.height();
  const std::int64_t rwidth = r1.width();
  const std::int64_t rmax = std::max(rheight, rwidth);
  const std::int64_t f1 = r2.y0 > kMinf + 2 ? (std::int64_t{r1.y1} - r2.y0) * rwidth : kDlongMax;
  const std::int64_t f2 = r2.y1 < kInf - 2 ? (std::int64_t{r2.y1} - r1.y0) * rwidth : kDlongMax;
  const bool dir = (tt1 & kDirectionBit) != 0;
  if ((tt1 & kSideBit) != 0) {
    const std::int64_t f3 = r2.x1 < kInf - 2 ? (std::int64_t{r1.x1} - r2.x1) * rheight + rmax : kDlongMin;
    return dir ? f2 < f3 : f1 < f3;
  }
  const std::int64_t f4 = r2.x0 > kMinf + 2 ? (std::int64_t{r2.x0} - r1.x0) * rheight + rmax : kDlongMin;
  return dir ? f1 < f4 : f2 < f4;
}

struct CornerCounts {
  int pos = 0;
  int neg = 0;
  int touch = 0;
  void add(int sign) noexcept { sign == 1 ? ++pos : (sign == -1 ? ++neg : ++touch); }
};

// The three corners of triangle r2/tt2 relative to the diagonal of r1/tt1.
CornerCounts count_corners(const Rect& r1, Body tt1, const Rect& r2, Body tt2) noexcept {
  CornerCounts c;
  const bool dir2 = (tt2 & kDirectionBit) != 0;
  const bool side2 = (tt2 & kSideBit) != 0;
  if (!dir2 || !side2) {
    c.add(eval_corner(Point{r2.x0, r2.y0}, r1, tt1));
  }
  if (!dir2 || side2) {
    c.add(eval_corner(Point{r2.x1, r2.y1}, r1, tt1));
  }
  if (dir2 || !side2) {
    c.add(eval_corner(Point{r2.x0, r2.y1}, r1, tt1));
  }
  if (dir2 || side2) {
    c.add(eval_corner(Point{r2.x1, r2.y0}, r1, tt1));
  }
  if (side_matches_direction(tt1)) {
    std::swap(c.pos, c.neg);
  }
  return c;
}

// The "both triangles have the same side bit" case of DBTestNMInteract.
bool same_side_enclosed(const Rect& r1, Body tt1, const Rect& r2, Body tt2) noexcept {
  const bool side1 = (tt1 & kSideBit) != 0;
  const bool dir1 = (tt1 & kDirectionBit) != 0;
  Point p{};
  if (!side1 && dir1) {
    p = Point{r1.x0, r1.y0};
  } else if (!side1) {
    p = Point{r1.x0, r1.y1};
  } else if (!dir1) {
    p = Point{r1.x1, r1.y0};
  } else {
    p = Point{r1.x1, r1.y1};
  }
  int sign = eval_corner(p, r2, tt2);
  if (side_matches_direction(tt2)) {
    sign = -sign;
  }
  return sign == -1;  // overlap_only: touching is not overlapping
}

// DBTestNMInteract with overlap_only = TRUE.
bool test_nm_interact(const Rect& r1, Body tt1, const Tile* t2, Body di2) noexcept {
  const Rect r2 = tile_rect(t2);
  if (outside_triangle(r1, tt1, r2)) {
    return false;
  }
  if (!is_split(t2->body)) {
    return true;
  }
  const Body tt2 = t2->body | di2;
  if ((tt1 & kDirectionBit) != (tt2 & kDirectionBit)) {
    return true;
  }
  const CornerCounts c = count_corners(r1, tt1, r2, tt2);
  if (c.pos == 3) {
    return false;
  }
  if (c.neg == 3) {
    return (tt1 & kSideBit) != (tt2 & kSideBit) || same_side_enclosed(r1, tt1, r2, tt2);
  }
  return !(c.touch > 0 && c.neg + c.touch != 3 && c.pos + c.touch == 3);
}

bool visit_nm_tile(Tile* tp, Body ttype, const Rect& rect, const tech::TypeMask& mask,
                   const std::function<bool(Tile*, Body)>& visit) {
  if (!is_split(tp->body)) {
    return mask.test(static_cast<tech::TileType>(tp->body)) && test_nm_interact(rect, ttype, tp, 0) &&
           visit(tp, 0);
  }
  if (mask.test(static_cast<tech::TileType>(left_type(tp->body))) &&
      test_nm_interact(rect, ttype, tp, tp->body) && visit(tp, kDiagonalBit)) {
    return true;
  }
  return mask.test(static_cast<tech::TileType>(right_type(tp->body))) &&
         test_nm_interact(rect, ttype, tp, tp->body | kSideBit) && visit(tp, kDiagonalBit | kSideBit);
}

}  // namespace

bool search_area(TilePlane& plane, const Rect& area, const tech::TypeMask& mask,
                 const std::function<bool(Tile*, Body)>& visit) {
  Tile* tp = area_first(plane, area);
  while (tp != nullptr) {
    plane.set_hint(tp);
    if (is_split(tp->body)) {
      if (visit_split_sides(tp, area, mask, visit)) {
        return true;
      }
    } else if (mask.test(static_cast<tech::TileType>(tp->body)) && visit(tp, 0)) {
      return true;
    }
    tp = area_next(tp, area, area.y1);
  }
  return false;
}

bool search_nm_area(TilePlane& plane, Body exact, const Rect& area, const tech::TypeMask& mask,
                    const std::function<bool(Tile*, Body)>& visit) {
  if ((exact & kDiagonalBit) == 0) {
    return search_area(plane, area, mask, visit);
  }
  Tile* tp = area_first(plane, area);
  while (tp != nullptr) {
    plane.set_hint(tp);
    if (visit_nm_tile(tp, exact, area, mask, visit)) {
      return true;
    }
    tp = area_next(tp, area, area.y1);
  }
  return false;
}

}  // namespace hefesto::db::detail
