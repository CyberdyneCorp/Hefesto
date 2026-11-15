// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A corner-stitched tile plane (capability layout-geometry-model,
// requirements "Plane model", "Canonical maximal-horizontal-strip
// decomposition", "Tile enumeration order", "Non-Manhattan (split tile)
// geometry"; decision X-1). The stitch structure is private; callers see
// tiles as TileView values, enumerated in Magic's exact order.
// (Magic ref: tiles/tile.c, database/DBpaint.c, database/DBtiles.c:521-640)
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/util/function_ref.hpp"

namespace hefesto::db {

/// Diagonal of a split (triangle) tile. `nw_se` (`\`): the diagonal runs
/// from the upper-left to the lower-right corner (Magic's TT_DIRECTION set;
/// the left part is the lower-left triangle). `ne_sw` (`/`): from the
/// lower-left to the upper-right corner (TT_DIRECTION clear; the left part
/// is the upper-left triangle).
enum class Diagonal : std::uint8_t { nw_se, ne_sw };

/// Compass corner of a triangle's right angle, as used by `.mag` `tri`
/// records ("Non-Manhattan (split tile) geometry" corner table).
enum class TriangleCorner : std::uint8_t { nw, sw, se, ne };

/// left `/` = nw, left `\` = sw, right `/` = se, right `\` = ne.
[[nodiscard]] constexpr TriangleCorner triangle_corner(Diagonal diagonal, bool right_side) noexcept {
  if (diagonal == Diagonal::ne_sw) {
    return right_side ? TriangleCorner::se : TriangleCorner::nw;
  }
  return right_side ? TriangleCorner::ne : TriangleCorner::sw;
}

/// Inverse of triangle_corner(): the diagonal and side of a corner.
struct TriangleSide {
  Diagonal diagonal = Diagonal::ne_sw;
  bool right_side = false;

  friend constexpr bool operator==(const TriangleSide&, const TriangleSide&) = default;
};
[[nodiscard]] constexpr TriangleSide triangle_side(TriangleCorner corner) noexcept {
  switch (corner) {
    case TriangleCorner::nw:
      return {Diagonal::ne_sw, false};
    case TriangleCorner::se:
      return {Diagonal::ne_sw, true};
    case TriangleCorner::sw:
      return {Diagonal::nw_se, false};
    case TriangleCorner::ne:
      return {Diagonal::nw_se, true};
  }
  return {};
}

/// The diagonal and side a triangle has after a Manhattan transform
/// (DBTransformDiagonal, database/DBconnect.c:73-92). Its bounding
/// rectangle transforms as a rectangle (apply()).
[[nodiscard]] constexpr TriangleSide transform_triangle(const Transform& t, Diagonal diagonal,
                                                        bool right_side) noexcept {
  const int o1 = (t.e > 0 || t.d > 0) ? 1 : 0;
  const int o2 = (t.a > 0 || t.b > 0) ? 1 : 0;
  const int o3 = t.a != 0 ? 1 : 0;
  int dir = diagonal == Diagonal::nw_se ? 1 : 0;
  const int side = (right_side ? 1 : 0) ^ o2 ^ (dir | o3);
  dir ^= o1 ^ o2;
  return TriangleSide{dir != 0 ? Diagonal::nw_se : Diagonal::ne_sw, side != 0};
}

/// A tile as seen from outside the plane.
struct TileView {
  Rect area;
  tech::TileType type = tech::kSpace;  // for split tiles: the reported side's type
  bool split = false;
  Diagonal diagonal = Diagonal::nw_se;  // split only
  bool right_side = false;              // split only: which triangle is reported
  tech::TileType left_type = tech::kSpace;   // split only
  tech::TileType right_type = tech::kSpace;  // split only
  /// The plane's tile behind this view (opaque), set by the Plane that
  /// reported it and valid until that plane next changes; a start for the
  /// stitch walks of Plane::tile_at() and Plane::neighbors(). Not part of
  /// the view's value (operator== ignores it).
  const void* tile = nullptr;

  friend bool operator==(const TileView& a, const TileView& b) noexcept {
    return a.area == b.area && a.type == b.type && a.split == b.split && a.diagonal == b.diagonal &&
           a.right_side == b.right_side && a.left_type == b.left_type && a.right_type == b.right_type;
  }
};

/// A side of a tile (Plane::neighbors).
enum class Side : std::uint8_t { left, right, bottom, top };

/// Result of painting type `paint` over type `have`, for every `have`
/// (one row of a tech paint/erase table, already selected for a plane).
using PaintRow = std::array<tech::TileType, tech::kMaxTileTypes>;

/// Return false to stop an enumeration early.
using TileVisitor = std::function<bool(const TileView&)>;
/// The visitor parameter of Plane::enumerate(): a non-owning reference
/// (no allocation per search), bound to any callable such as a lambda or a
/// TileVisitor for the duration of the call.
using TileVisitorRef = util::FunctionRef<bool(const TileView&)>;

/// A split-tile structure change made by a paint (Magic's splitUE): a split
/// tile cut into smaller split tiles (`join` false: the tile before the
/// cut) or two split tiles merged back into one (`join` true: the merged
/// tile). The geometry is unchanged either way.
struct SplitEdit {
  bool join = false;
  Rect area;
  Diagonal diagonal = Diagonal::nw_se;
  tech::TileType left_type = tech::kSpace;
  tech::TileType right_type = tech::kSpace;

  friend bool operator==(const SplitEdit&, const SplitEdit&) = default;
};

/// Whether a paint merges split tiles afterwards: `split_tiles` is the
/// cell paint (DBPaint / DBErase: the plane paint, then DBMergeNMTiles over
/// the area grown by 1); `none` is the bare plane paint (DBPaintPlane /
/// DBNMPaintPlane), which leaves the split tiles as the paint cut them, as
/// mask generation's planes do in Magic.
enum class PaintMerge : std::uint8_t { split_tiles, none };

class Plane {
 public:
  Plane();  // the whole plane is space
  ~Plane();
  Plane(Plane&&) noexcept;
  Plane& operator=(Plane&&) noexcept;
  Plane(const Plane&) = delete;
  Plane& operator=(const Plane&) = delete;

  /// Deep copy (used by flatten, copy and undo snapshots).
  [[nodiscard]] Plane clone() const;

  /// Replaces every type t under `area` by row[t] (DBPaintPlane), then
  /// (with PaintMerge::split_tiles, the default) merges split tiles over
  /// `area` expanded by 1 unless `area` is the whole-plane rectangle
  /// (DBPaint / DBErase). Geometry outside the usable plane is rejected
  /// with DB-GEOM-001 (Q-MODEL-24); a null area is a no-op.
  [[nodiscard]] diag::Status paint(const Rect& area, const PaintRow& row,
                                   PaintMerge merge = PaintMerge::split_tiles);

  /// Applies `row` to one side of the triangle obtained by splitting `area`
  /// along `diagonal` ("Non-Manhattan painting", DBNMPaintPlane), then
  /// merges split tiles as paint() does. `magic_compat` false rounds both
  /// diagonals half up (Q-MODEL-3). `off_grid`, when given, is set to
  /// whether a 1x1 area had to be filled because the triangle could not be
  /// split on the grid (DBNMPaintPlane0 returning 1; the DRC flattening
  /// reports it, why 5).
  [[nodiscard]] diag::Status paint_triangle(const Rect& area, Diagonal diagonal, bool right_side,
                                            const PaintRow& row, bool magic_compat = true,
                                            bool* off_grid = nullptr,
                                            PaintMerge merge = PaintMerge::split_tiles);

  /// DBMergeNMTiles ("Split tile merging"): walks the tiles overlapping
  /// `area` in Magic's area-enumeration order and merges each split tile
  /// with the split tile of the same split type at its diagonal corner
  /// below while the merge criteria hold, repeating on the merged tile.
  /// Only the decomposition changes, never the type of a point; nothing is
  /// recorded for undo beyond what record_splits() logs. `area` is clamped
  /// to the usable plane; an area without extent merges nothing. Run after
  /// the cell paints (above) and by mask generation's `cif unfracture`
  /// (Q-MASK-413).
  void merge_split_tiles(const Rect& area);

  /// While `log` is set, paint() and paint_triangle() append the split-tile
  /// fractures and joins they make to it (dbSplitUndo / dbJoinUndo, for the
  /// undo recorder); nullptr stops recording.
  void record_splits(std::vector<SplitEdit>* log) noexcept;

  /// Visits the tiles overlapping `area` whose type is in `types`, in
  /// Magic's area-enumeration order ("Tile enumeration order",
  /// DBSrPaintArea). A split tile is reported once per matching side (left
  /// first) unless `area` lies entirely beyond its diagonal. Returns false
  /// when the visitor stopped early. A zero-width or zero-height `area`
  /// runs Magic's algorithm unchanged (used for point and line labels); an
  /// inverted area visits nothing; an area beyond the plane (out to
  /// +-kInfinity) is clamped to whole_plane().
  bool enumerate(const Rect& area, const tech::TypeMask& types, TileVisitorRef visit) const;

  /// Like enumerate() but restricted to the triangle of `area` given by
  /// `diagonal`/`right_side` (DBSrPaintNMArea, overlap only).
  bool enumerate_triangle(const Rect& area, Diagonal diagonal, bool right_side, const tech::TypeMask& types,
                          TileVisitorRef visit) const;

  /// The tile containing the unit square whose lower-left corner is `p`,
  /// exactly as enumerate() over that square reports it first (for a split
  /// tile, the first side it reports). The point location (GOTOPOINT) walks
  /// the stitches from `near`'s tile when given (a view this plane reported
  /// since its last change), else from the plane's hint; it does not move
  /// the hint. The DRC kernel's corner probes (Magic's stitch steps).
  [[nodiscard]] TileView tile_at(Point p, const TileView* near = nullptr) const;

  /// The tiles across side `side` of `t` (a view this plane reported since
  /// its last change) whose extent along that side overlaps [lo, hi), a
  /// sub-range of the side: each exactly as enumerate() over the one-unit
  /// strip beyond that part of the side reports it first, ordered by
  /// increasing y (left, right) or x (bottom, top). Walks the corner
  /// stitches (Magic's BL/RT, TR/LB, LB/TR, RT/BL loops); `out` is cleared
  /// first. Empty when lo >= hi.
  void neighbors(const TileView& t, Side side, Coord lo, Coord hi, std::vector<TileView>& out) const;

  /// The type of the unit square whose lower-left corner is `p` (for a
  /// split tile: the side of the diagonal holding the square's center).
  [[nodiscard]] tech::TileType type_at(Point p) const;

  /// Bounding box of all non-space tiles; a null Rect when empty.
  [[nodiscard]] Rect bounding_box() const;

  [[nodiscard]] bool empty() const;  // nothing but space

  /// Number of tiles, space included (for tests and statistics).
  [[nodiscard]] std::size_t tile_count() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::db
