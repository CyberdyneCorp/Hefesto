// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Restricted stitch editing on a scratch plane (openspec/changes/m7-extraction
// design D6, Open Question 1 (a)). extresist fractures a scratch copy of a net
// with TiSplitX and TiJoinY1 (resis/ResFract.c:64-251, 436-488) and walks the
// corner stitches directly; this header gives it those primitives. They break
// the canonical maximal-horizontal-strip decomposition on purpose, so only
// extract/ and extresist/ may include this header
// (scripts/check_core_includes.py), and only on a plane no cell owns (a yank
// buffer): every other caller stays on the paint API.
// (Magic ref: tiles/tile.c:251-330 TiSplitX, :827-832 TiJoinY1, tiles/tile.h
// GOTOPOINT, LB/BL/TR/RT)
//
// Owner: INTEGRATION with the MODEL review (m7 design D3, D6).

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {

/// A tile of the edited plane, valid until the editor releases the tiles
/// joins removed (release() or the editor's end). An empty handle stands for
/// "no tile": the result of an invalid edit or of a stitch beyond the plane.
struct StitchTile {
  const void* tile = nullptr;

  [[nodiscard]] explicit operator bool() const noexcept { return tile != nullptr; }
  friend bool operator==(const StitchTile&, const StitchTile&) = default;
};

/// The four corner stitches of a tile (tiles/tile.h LB, BL, TR, RT).
enum class Stitch : std::uint8_t {
  lb,  // the leftmost tile below
  bl,  // the lowest tile to the left
  tr,  // the topmost tile to the right
  rt,  // the rightmost tile above
};

/// Split, join, type and stitch access on one scratch plane. The editor
/// does not own the plane; the plane must outlive it and must not be
/// painted while the editor holds tiles a join removed.
class StitchEditor {
 public:
  explicit StitchEditor(Plane& scratch) noexcept;
  ~StitchEditor();
  StitchEditor(const StitchEditor&) = delete;
  StitchEditor& operator=(const StitchEditor&) = delete;
  StitchEditor(StitchEditor&&) = delete;
  StitchEditor& operator=(StitchEditor&&) = delete;

  /// The plane's search hint (PlaneGetHint / PlaneSetHint).
  [[nodiscard]] StitchTile hint() const noexcept;
  void set_hint(StitchTile t) noexcept;

  /// GOTOPOINT from `near` (the hint when empty): the tile whose area holds
  /// the unit square at `p`. Empty when that square is outside the usable
  /// plane.
  [[nodiscard]] StitchTile tile_at(Point p, StitchTile near = {}) const noexcept;

  /// The tile across corner stitch `s` of `t`. The four boundary tiles
  /// around the plane are reported (their coordinates are Magic's, out to
  /// +-kInfinity); the stitches beyond them are empty.
  [[nodiscard]] StitchTile stitch(StitchTile t, Stitch s) const noexcept;

  /// LEFT, BOTTOM, RIGHT, TOP of `t` (a null Rect for an empty handle).
  [[nodiscard]] Rect area(StitchTile t) const noexcept;

  /// TiGetType: the left type of a split tile, the type of any other tile,
  /// and 0x3fff for a boundary tile (as Magic's body -1 masked).
  [[nodiscard]] tech::TileType type(StitchTile t) const noexcept;

  /// Whether `t` is one of the four boundary tiles around the plane.
  [[nodiscard]] bool boundary(StitchTile t) const noexcept;

  /// TiSetBody with a Manhattan type. Boundary tiles and empty handles are
  /// left alone.
  void set_type(StitchTile t, tech::TileType type) noexcept;

  /// The tile's client slot (Plane::client).
  [[nodiscard]] std::uintptr_t client(StitchTile t) const noexcept;
  void set_client(StitchTile t, std::uintptr_t value) noexcept;

  /// TiSplitX: cuts `t` at `x` and returns the new right part, of type
  /// space (body 0) with a kClientDefault slot; `t` keeps its body and its
  /// slot. Empty, and nothing changes, unless `t` is an internal tile with
  /// LEFT < x < RIGHT.
  StitchTile split_x(StitchTile t, Coord x);

  /// TiJoinY: merges `gone` into `keep`, which must be internal tiles
  /// stacked one directly on the other with the same LEFT and RIGHT. `gone`
  /// stays readable until release(). Returns false, and nothing changes,
  /// when they are not joinable. The bodies are not compared (ResSplitX
  /// checks the types first).
  bool join_y(StitchTile keep, StitchTile gone);

  /// Frees the tiles the joins removed (TiFreeIf); handles to them become
  /// invalid.
  void release() noexcept;

  /// The tiles across side `side` of internal tile `t`, along the whole
  /// side, ordered by increasing y (left, right) or x (bottom, top)
  /// (Plane::neighbors). Empty for a boundary tile.
  void neighbors(StitchTile t, Side side, std::vector<StitchTile>& out) const;

  /// The plane's view of internal tile `t` (for split tiles, its left side);
  /// a default view for an empty handle or a boundary tile.
  [[nodiscard]] TileView view(StitchTile t) const;

  /// The handle of a view the edited plane reported since its last change.
  [[nodiscard]] static StitchTile handle(const TileView& v) noexcept { return StitchTile{v.tile}; }

 private:
  Plane* plane_;
};

}  // namespace hefesto::db
