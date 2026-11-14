// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the operator kernels (capability mask-output-operators;
// design D6): Magic's tile side types and corner-stitch neighbour walks over
// db::Plane, tile marks (Magic's client fields), the layout and mask-layer
// searches of cifSrTiles, coordinate scaling, the CIF grid-limit helpers
// and generation error reporting. Private to src/core/src/mask/ops/.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c and tiles/tile.h,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "hefesto/db/plane.hpp"
#include "hefesto/mask/ops/compat.hpp"
#include "hefesto/mask/ops/operators.hpp"

namespace hefesto::mask::detail {

/// The type Magic's boundary tiles report on every side (TT_LEFTMASK of a
/// body of -1): neither space nor solid.
inline constexpr tech::TileType kBoundaryType = 0x3fff;

/// Magic's CLIENTDEFAULT/CIF_PENDING/CIF_PROCESSED/CIF_IGNORE tile marks.
enum class Mark : std::uint8_t { unprocessed, pending, processed, ignore };

/// Tile marks keyed by db::TileView::tile (valid while the plane is not
/// changed). A missing entry is unprocessed.
class Marks {
 public:
  [[nodiscard]] Mark get(const db::TileView& t) const {
    const auto it = marks_.find(t.tile);
    return it == marks_.end() ? Mark::unprocessed : it->second;
  }
  void set(const db::TileView& t, Mark m) {
    if (m == Mark::unprocessed) {
      marks_.erase(t.tile);
    } else {
      marks_[t.tile] = m;
    }
  }
  /// Every tile whose mark is not unprocessed.
  template <typename Fn>
  void for_each(Fn&& fn) const {
    for (const auto& [tile, mark] : marks_) {
      fn(tile, mark);
    }
  }
  void clear() noexcept { marks_.clear(); }

 private:
  std::unordered_map<const void*, Mark> marks_;
};

// --- Tile side types (tiles/tile.h) ----------------------------------------

[[nodiscard]] inline bool direction(const db::TileView& t) noexcept { return t.diagonal == db::Diagonal::nw_se; }
[[nodiscard]] inline tech::TileType left_type(const db::TileView& t) noexcept { return t.split ? t.left_type : t.type; }
[[nodiscard]] inline tech::TileType right_type(const db::TileView& t) noexcept {
  return t.split ? t.right_type : t.type;
}
[[nodiscard]] inline tech::TileType top_type(const db::TileView& t) noexcept {
  return t.split ? (direction(t) ? t.right_type : t.left_type) : t.type;
}
[[nodiscard]] inline tech::TileType bottom_type(const db::TileView& t) noexcept {
  return t.split ? (direction(t) ? t.left_type : t.right_type) : t.type;
}
/// TiGetType: the left type of a split tile.
[[nodiscard]] inline tech::TileType body_type(const db::TileView& t) noexcept { return left_type(t); }

/// The type of the reported side (TiGetTypeExact | dinfo resolved).
[[nodiscard]] inline tech::TileType side_type(const db::TileView& t) noexcept { return t.type; }

// --- Neighbour walks ---------------------------------------------------------

/// Every tile across side `side` of `t`, ordered by increasing y (left,
/// right) or x (bottom, top); empty at the plane's edge (Magic's boundary
/// tile). Magic's walks: left BL→RT (ascending), top RT→BL (descending),
/// right TR→LB (descending), bottom LB→TR (ascending).
[[nodiscard]] std::vector<db::TileView> side_tiles(const db::Plane& plane, const db::TileView& t, db::Side side);

/// The facing types of a side's end tiles (kBoundaryType when the side has
/// no neighbour): `first` is the lowest (left, right sides) or left-most
/// (top, bottom sides) neighbour, `last` the other end.
[[nodiscard]] std::optional<db::TileView> first_neighbor(const db::Plane& plane, const db::TileView& t, db::Side side);
[[nodiscard]] std::optional<db::TileView> last_neighbor(const db::Plane& plane, const db::TileView& t, db::Side side);
/// The type a neighbour shows towards `t` across `side` of `t` (its right
/// type for a left neighbour, and so on); kBoundaryType for none.
[[nodiscard]] tech::TileType facing_type(const std::optional<db::TileView>& n, db::Side side) noexcept;

// --- Scaling -----------------------------------------------------------------

[[nodiscard]] db::Coord clamp_coord(std::int64_t v) noexcept;
/// Every coordinate times `s` (cifPaintFunc's scaling of a layout tile).
[[nodiscard]] db::Rect scale_rect(const db::Rect& r, std::int32_t s) noexcept;
/// Times `s`, but sides at the plane's edge stay (cifGrowFunc's "watch out
/// for infinities").
[[nodiscard]] db::Rect scale_finite(const db::Rect& r, std::int32_t s) noexcept;

[[nodiscard]] inline std::int32_t scale_of(const OpArgs& args) noexcept {
  return args.style.scale.scale_factor > 0 ? args.style.scale.scale_factor : 1;
}
[[nodiscard]] inline std::int32_t grid_limit_of(const OpArgs& args) noexcept { return args.style.grid_limit; }
[[nodiscard]] inline bool euclidean(const OpArgs& args) noexcept {
  constexpr std::uint32_t kGrowEuclidean = 0x08;  // CWF_GROW_EUCLIDEAN
  return (args.style.options & kGrowEuclidean) != 0;
}
/// The client value `i` of `op` (LayerOp::values: squares_value,
/// slots_value, bridge_value, or close's Q-MASK-2 OFF record), `fallback`
/// when the style did not set it.
[[nodiscard]] inline std::int32_t value32(const LayerOp& op, std::size_t i, std::int32_t fallback = 0) noexcept {
  return i < op.values.size() ? op.values[i] : fallback;
}
/// bl_distance[t] of a bloat operator (0 when the operator has no table).
[[nodiscard]] inline std::int32_t bloat_distance(const LayerOp& op, std::size_t t) noexcept {
  return op.bloat && t < op.bloat->distance.size() ? op.bloat->distance[t] : 0;
}
/// bl_plane of a bloat operator: a tech::PlaneId, or -1 for a bloat-all over
/// mask layers.
[[nodiscard]] inline std::int32_t bloat_plane(const LayerOp& op, std::int32_t fallback = 0) noexcept {
  return op.bloat ? op.bloat->plane : fallback;
}

// --- Painting ----------------------------------------------------------------

inline void paint(MaskPlane& p, const db::Rect& r) { (void)p.paint(r); }
inline void erase(MaskPlane& p, const db::Rect& r) { (void)p.erase(r); }
/// DBNMPaintPlane(type | dinfo): the reported triangle of a split tile over
/// `area`, else the rectangle.
void paint_shape(MaskPlane& p, const db::TileView& t, const db::Rect& area, bool magic_compat = true);
void erase_shape(MaskPlane& p, const db::TileView& t, const db::Rect& area, bool magic_compat = true);

// --- Searches (cifSrTiles, cifSrTiles2) ------------------------------------

const tech::TypeMask& solid_mask();
const tech::TypeMask& space_mask();

/// One searched plane: a layout plane (scaled by the style's scale factor)
/// or a mask layer (scale 1).
struct Source {
  const db::Plane* plane = nullptr;
  bool layout = false;
  tech::PlaneId plane_id = 0;  // layout only
};

/// The layout planes cifSrTiles searches for `types`, in plane order (from
/// the DRC check plane on): those whose types (DBPlaneTypes) meet `types`.
[[nodiscard]] std::vector<Source> layout_sources(const LayerInputs& inputs, const tech::TypeMask& types);
/// The mask layers of `cif_layers` that were evaluated, in layer order.
[[nodiscard]] std::vector<const MaskPlane*> layer_sources(const LayerInputs& inputs,
                                                          const std::vector<std::size_t>& cif_layers);

/// cifSrTiles: visits the layout tiles of op.types overlapping `area` on
/// each layout plane (with `scale` the style's), then every solid tile of
/// each mask layer of op.cif_layers (scale 1). The visitor gets the tile and
/// the scale to apply; it returns false to stop.
using ScaledVisitor = util::FunctionRef<bool(const db::TileView&, std::int32_t scale)>;
bool search_op_tiles(const OpArgs& args, const db::Rect& area, ScaledVisitor visit);

/// cifSrTiles2: true when a layout tile of op.types overlaps `mask_area`
/// converted to internal units (Q-MASK-21) or a solid tile of op.cif_layers
/// (or of `current` when the operator lists no layer) overlaps `mask_area`.
/// Stops at the first tile the visitor accepts (returns true for).
using FoundVisitor = util::FunctionRef<bool(const db::TileView&, const Source&)>;
bool search_op_tiles2(const OpArgs& args, const OperatorContext& ctx, const db::Rect& mask_area,
                      const MaskPlane* current, FoundVisitor visit);

// --- Grid limit (SetBoxGrid, SetValueGrid, SetMinBoxGrid) --------------------

void set_box_grid(db::Rect& r, std::int32_t limit) noexcept;
[[nodiscard]] std::int32_t set_value_grid(std::int32_t v, std::int32_t limit) noexcept;
void set_min_box_grid(db::Rect& r, std::int32_t width, std::int32_t limit) noexcept;

// --- Errors (CIFError) -------------------------------------------------------

// The generation errors of the kernels (src/core/src/mask/ops/codes.inc).
inline constexpr std::string_view kTileInverted = "MASK-OPER-001";       // tile inverted by shrink
inline constexpr std::string_view kNoRoomForCuts = "MASK-OPER-002";      // no room for contact cuts in area!
inline constexpr std::string_view kStripWithoutCuts = "MASK-OPER-003";   // contact strip with no room for cuts!
inline constexpr std::string_view kNoRoomForContacts = "MASK-OPER-004";  // no room for contacts in area!

/// Reports "CIF error in cell C, layer L: message" for `mask_area` (mask
/// units) to ctx.sink, unless the style says `options no-errors` or no cell
/// is being generated (Magic's CIFErrorDef).
void cif_error(const OpArgs& args, const OperatorContext& ctx, const db::Rect& mask_area, std::string_view code,
               std::string_view message);

/// Counts one painted tile (CIFTileOps).
inline void count_tile(const OperatorContext& ctx) noexcept {
  if (ctx.statistics != nullptr) {
    ++ctx.statistics->flat_tiles;
  }
}

// --- Regions -----------------------------------------------------------------

/// One region of a mask plane (4-connected solid tiles), collected as
/// Magic's region walks do (a LIFO stack; neighbours pushed top right to
/// left, left bottom to top, bottom left to right, right top to bottom),
/// with the tiles in visiting order.
struct Region {
  std::vector<db::TileView> tiles;
  db::Rect bbox;
  bool simple = true;  // one non-split tile without solid neighbours
};

/// Collects the region of `start`, marking its tiles processed. `on_tile`
/// sees each tile as it is visited.
Region collect_region(const db::Plane& plane, const db::TileView& start, Marks& marks,
                      util::FunctionRef<void(const db::TileView&)> on_tile);

/// Calls `fn` with the first tile of each region that has a solid tile
/// overlapping `area`, in the enumeration order of `plane` (Magic restarts
/// cifSquaresInitFunc's search after each region): a solid tile still
/// unprocessed in `marks`. `fn` must mark the region (collect_region()).
void for_each_region_start(const db::Plane& plane, const db::Rect& area, Marks& marks,
                           util::FunctionRef<void(const db::TileView&)> fn);

}  // namespace hefesto::mask::detail
