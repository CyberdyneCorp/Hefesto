// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private corner-stitched tile plane behind db::Plane (decision X-1;
// capability layout-geometry-model). A faithful port of Magic's tile module
// and of the paint-plane algorithms that the exact tile decomposition and
// enumeration order depend on.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e): tiles/tile.h, tiles/tile.c,
// tiles/search.c, database/DBpaint.c, database/DBtiles.c. Magic is
// Copyright (C) 1985, 1990 Regents of the University of California, with a
// permissive license (see THIRD_PARTY_NOTICES.md, capability
// hefesto-licensing).
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"

namespace hefesto::db::detail {

/// Magic's TileType word stored in a tile body (database/database.h.in:139-143).
using Body = std::uint32_t;
inline constexpr Body kDiagonalBit = 0x40000000U;
inline constexpr Body kSideBit = 0x20000000U;
inline constexpr Body kDirectionBit = 0x10000000U;
inline constexpr Body kLeftMask = 0x00003fffU;
inline constexpr Body kRightMask = 0x0fffc000U;
inline constexpr Body kBoundaryBody = 0xffffffffU;  // TiSetBody(tile, -1)
inline constexpr unsigned kRightShift = 14;

[[nodiscard]] constexpr bool is_split(Body b) noexcept { return (b & kDiagonalBit) != 0; }
[[nodiscard]] constexpr Body left_type(Body b) noexcept { return b & kLeftMask; }
[[nodiscard]] constexpr Body right_type(Body b) noexcept { return is_split(b) ? (b & kRightMask) >> kRightShift : (b & kLeftMask); }
[[nodiscard]] constexpr int direction(Body b) noexcept { return (b & kDirectionBit) != 0 ? 1 : 0; }
[[nodiscard]] constexpr Body split_body(Body left, Body right, int dir) noexcept {
  return left | (right << kRightShift) | kDiagonalBit | (dir != 0 ? kDirectionBit : 0U);
}

struct Tile {
  Body body = 0;
  Tile* lb = nullptr;  // left-bottom corner stitch (below, leftmost)
  Tile* bl = nullptr;  // bottom-left corner stitch (left, lowest)
  Tile* tr = nullptr;  // top-right corner stitch (right, topmost)
  Tile* rt = nullptr;  // right-top corner stitch (above, rightmost)
  Coord x = 0;         // LEFT
  Coord y = 0;         // BOTTOM
};

[[nodiscard]] inline Coord left(const Tile* t) noexcept { return t->x; }
[[nodiscard]] inline Coord bottom(const Tile* t) noexcept { return t->y; }
[[nodiscard]] inline Coord right(const Tile* t) noexcept { return t->tr->x; }
[[nodiscard]] inline Coord top(const Tile* t) noexcept { return t->rt->y; }
[[nodiscard]] inline Rect tile_rect(const Tile* t) noexcept { return Rect{left(t), bottom(t), right(t), top(t)}; }

/// CANMERGE_X / CANMERGE_Y of DBpaint.c.
[[nodiscard]] inline bool can_merge_y(const Tile* a, const Tile* b) noexcept {
  return left(a) == left(b) && a->body == b->body && !is_split(a->body) && right(a) == right(b);
}
[[nodiscard]] inline bool can_merge_x(const Tile* a, const Tile* b) noexcept {
  return bottom(a) == bottom(b) && a->body == b->body && !is_split(a->body) && top(a) == top(b);
}

/// One paint/erase result row indexed by the existing type.
using ResultRow = PaintRow;

/// The tile plane: four boundary tiles at infinity around the internal
/// tiles (TiNewPlane), with Magic's split/join primitives. Tiles freed by
/// joins stay allocated until end_operation(), mirroring Magic's delayed
/// frees, so pointers read after a join remain valid.
class TilePlane {
 public:
  TilePlane();  // DBNewPlane(TT_SPACE)
  ~TilePlane() = default;
  TilePlane(const TilePlane&) = delete;
  TilePlane& operator=(const TilePlane&) = delete;
  TilePlane(TilePlane&&) = delete;
  TilePlane& operator=(TilePlane&&) = delete;

  /// Deep copy of `other`'s tiles into this (fresh) plane.
  void copy_from(const TilePlane& other);

  [[nodiscard]] Tile* hint() const noexcept { return hint_; }
  void set_hint(Tile* t) noexcept { hint_ = t; }

  Tile* split_x(Tile* tile, Coord x);  // TiSplitX: returns the new right tile
  Tile* split_y(Tile* tile, Coord y);  // TiSplitY: returns the new top tile
  void join_x(Tile* keep, Tile* gone);  // TiJoinX (+ delayed free)
  void join_y(Tile* keep, Tile* gone);  // TiJoinY (+ delayed free)

  /// Releases the tiles freed during the operation.
  void end_operation();

  /// Visits every internal tile (for copy, statistics and tests).
  void for_each_tile(const std::function<void(const Tile*)>& visit) const;

 private:
  Tile* alloc();
  void free_later(Tile* t) { pending_.push_back(t); }

  std::deque<Tile> storage_;
  std::vector<Tile*> free_;
  std::vector<Tile*> pending_;
  Tile* left_ = nullptr;
  Tile* top_ = nullptr;
  Tile* right_ = nullptr;
  Tile* bottom_ = nullptr;
  Tile* infinity_ = nullptr;
  Tile* hint_ = nullptr;
};

/// GOTOPOINT: the tile containing `p` (bottom and left edges inclusive).
[[nodiscard]] Tile* goto_point(Tile* tp, Point p) noexcept;

/// The area-enumeration step shared by DBSrPaintArea and the paint loops:
/// from the tile just processed (`clip_top` is min(TOP at entry, area.y1)),
/// the next tile to enumerate, or nullptr when the enumeration is complete.
[[nodiscard]] Tile* area_next(Tile* tile, const Rect& area, Coord clip_top) noexcept;

/// The first tile of an area enumeration, or nullptr if none.
[[nodiscard]] Tile* area_first(TilePlane& plane, const Rect& area) noexcept;

// --- Paint algorithms (paint_plane.cpp, nm_split.cpp, nm_paint.cpp) ---

/// DBPaintPlane0 with method PAINT_NORMAL.
void paint_plane(TilePlane& plane, const Rect& area, const ResultRow& row);

/// DBNMPaintPlane0: `exact` carries the diagonal, direction and side bits
/// (0 = a Manhattan rectangle). `magic_compat` false rounds both diagonals
/// half up (Q-MODEL-3).
void nm_paint_plane(TilePlane& plane, Body exact, const Rect& area, const ResultRow& row, bool magic_compat);

/// DBMergeNMTiles0 with mergeOnce = FALSE.
void merge_nm_tiles(TilePlane& plane, const Rect& area);

/// TiNMSplitY / TiNMSplitX (no undo); `dir` 1 keeps the new tile on top/right.
void nm_split_y(TilePlane& plane, Tile*& oldtile, Tile*& newtile, Coord y, int dir);
void nm_split_x(TilePlane& plane, Tile*& oldtile, Tile*& newtile, Coord x, int dir);
Tile* nm_merge_right(TilePlane& plane, Tile* tile);
Tile* nm_merge_left(TilePlane& plane, Tile* tile);

/// DBSrPaintArea: visits tiles overlapping `area` whose type (either side
/// for split tiles, with Magic's outside-of-triangle test) is in `mask`.
/// The visitor gets the tile and 0, kDiagonalBit or kDiagonalBit|kSideBit.
/// Return true from the visitor to stop. Returns true when stopped.
bool search_area(TilePlane& plane, const Rect& area, const tech::TypeMask& mask,
                 const std::function<bool(Tile*, Body)>& visit);

/// DBSrPaintNMArea: like search_area but restricted to the triangle
/// described by `exact` (falls back to search_area when not diagonal).
bool search_nm_area(TilePlane& plane, Body exact, const Rect& area, const tech::TypeMask& mask,
                    const std::function<bool(Tile*, Body)>& visit);

}  // namespace hefesto::db::detail
