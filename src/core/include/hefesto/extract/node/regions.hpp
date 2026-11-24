// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RegionFinder: ExtFindRegions over a cell's planes (capability
// extract-nodes-and-labels "Node connectivity";
// openspec/changes/m7-extraction/design.md D5, D6, D8). It walks the
// planes in plane-number order (Magic's `pNum = PL_TECHDEPBASE ..
// DBNumPlanes`; the style's `planeorder` does not change the scan) and
// each plane in db::Plane's tile order, keeps the region of each visited
// tile in the tile's client slot (db::Plane::client, D6) and, unless
// keep_clients is set, resets the slots before it returns.
//
// The client-slot encoding (read by EXTCAP and EXTDEV during a pass, that
// is between find()/find_nodes() with keep_clients and reset()):
//   - db::kClientDefault: the tile has not been visited (Magic's
//     CLIENTDEFAULT);
//   - 0: the tile is on the pass's stack (Magic's VISITPENDING);
//   - 2 * (d + 1): the tile belongs to region d, the region's discovery
//     number (0 = the first region the pass found);
//   - 2 * k + 1: a split tile with material on both sides (Magic's
//     ExtSplitRegion): entry k of the finder's split table holds one of
//     the three states above per side.
// Use region_of() rather than decoding the slot. Regions are prepended to
// the list as they are found (and sticky-label nodes after them), so the
// NodeRegionList index of region d is `list.size() - 1 - d` (node_index()).
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::extract {
class ExtStyle;
}

namespace hefesto::db {
class CellDef;
class Plane;
struct TileView;
}  // namespace hefesto::db

namespace hefesto::tech {
class Technology;
}

namespace hefesto::extract {

/// Per-type connectivity (exts_nodeConn or exts_deviceConn): index =
/// tech::TileType, value = the types a tile of that type connects to.
struct ConnectTable {
  std::vector<tech::TypeMask> connects;
};

/// Area and perimeter of a region in one resistance class.
struct ClassArea {
  std::int64_t area = 0;
  std::int64_t perimeter = 0;

  friend bool operator==(const ClassArea&, const ClassArea&) = default;
};

/// A label attached to a region ("Label attachment").
struct LabelRegion {
  std::string text;
  db::Point position;
  tech::TileType type = 0;
  bool port = false;
  /// Added in M7 R3: the label's rectangle (`port` and `attr` records),
  /// its port number (lab_port) and its index in the cell's label list
  /// (kNoLabel for a label that is not the cell's).
  db::Rect rect;
  std::uint32_t port_index = 0;
  std::size_t label = std::numeric_limits<std::size_t>::max();

  friend bool operator==(const LabelRegion&, const LabelRegion&) = default;
};

/// An electrical node region (NodeRegion): its reference tile (the
/// lowest-leftmost of the first plane), its per-class area and perimeter
/// and the labels attached to it, in Magic's order.
struct NodeRegion {
  db::Point reference;
  tech::TileType type = 0;
  tech::PlaneId plane = 0;
  std::vector<ClassArea> classes;
  std::vector<LabelRegion> labels;
  double substrate_cap = 0.0;
  /// Added in M7 R3: the reference tile is the right side of a split tile
  /// (TT_SIDE in lreg_type; the generated name ends in `r#`).
  bool right_side = false;

  friend bool operator==(const NodeRegion&, const NodeRegion&) = default;
};

/// A device region (TransRegion): the gate region of one device.
struct DeviceRegion {
  db::Point reference;
  tech::TileType type = 0;
  tech::PlaneId plane = 0;
  std::int64_t area = 0;

  friend bool operator==(const DeviceRegion&, const DeviceRegion&) = default;
};

using NodeRegionList = std::vector<NodeRegion>;

/// The region callbacks of ExtFindRegions (first: a region starts; each:
/// a tile joins the current region). Either may be empty.
struct RegionCallbacks {
  std::function<void(const db::Rect& tile, tech::TileType type, tech::PlaneId plane)> first;
  std::function<void(const db::Rect& tile, tech::TileType type, tech::PlaneId plane)> each;
};

/// Added in M7 R3: a tile side of a pass's seed, named by a point inside
/// it (the tile is the one db::Plane::tile_at(point) finds).
struct RegionSeed {
  tech::PlaneId plane = 0;
  db::Point point;
  bool right_side = false;  // the right side of a split tile
};

/// Added in M7 R3: the inputs of find_nodes() besides the cell.
struct NodePassOptions {
  /// extNodeClipArea: area and perimeter outside it are not counted.
  std::optional<db::Rect> clip;
  /// EXT_DORESISTANCE: NodeRegion::classes is filled (all zero otherwise,
  /// as Magic writes them).
  bool resistance = true;
  /// The substrate tiles of the first pass (extSubsFunc's pushes, in push
  /// order; EXTDEV finds them): one region flooded from all of them, found
  /// first. Empty: no substrate region from tiles.
  std::vector<RegionSeed> substrate_seeds;
  /// With no seeds, a tile-less substrate node on this plane (type space,
  /// reference (MINFINITY+3, MINFINITY+3), extFindNodes' fallback).
  std::optional<tech::PlaneId> substrate_without_tiles;
  /// extFindNodes' `subonly`: stop after the substrate region (the unique
  /// pass marks the substrate this way).
  bool substrate_only = false;
  /// The magic-compat setting: ON starts a region only at a tile whose
  /// slot is unvisited, as DBSrPaintClient does, so the second side of a
  /// split tile is never a seed; OFF also starts one at an unvisited side.
  bool magic_compat = true;
};

class RegionFinder {
 public:
  RegionFinder() = default;
  /// Added in M7 R3: a finder that follows contacts and the cross-plane
  /// halo of `technology` (without one, regions stay on one plane).
  explicit RegionFinder(const tech::Technology& technology) : technology_(&technology) {}

  /// Added in M7 R3: leave the region slots set after find()/find_nodes()
  /// (until reset()), for the passes that read them.
  bool keep_clients = false;

  /// Finds the regions of `cell` within `area` over the types of `mask`
  /// connected by `connect`, calling `callbacks` for each region and tile.
  /// Each region's reference is its lowest-leftmost tile on its lowest
  /// plane (extSetNodeNum on every tile); classes stay empty.
  [[nodiscard]] diag::Outcome<NodeRegionList> find(const db::CellDef& cell, const db::Rect& area,
                                                   const tech::TypeMask& mask, const ConnectTable& connect,
                                                   RegionCallbacks& callbacks);

  /// Added in M7 R3: extFindNodes. The substrate region first (options),
  /// then a region per unvisited active tile of `style.active` connected
  /// by `style.node_conn`, with the per-class area and perimeter of
  /// extNodeAreaFunc (NodeRegion::classes, one entry per resistance class)
  /// and the reference of extSetNodeNum. Capacitance stays 0 (EXTCAP).
  [[nodiscard]] diag::Outcome<NodeRegionList> find_nodes(const db::CellDef& cell, const ExtStyle& style,
                                                         const NodePassOptions& options);

  /// Added in M7 R3: the discovery number of the region holding the side
  /// of `tile` that `tile.right_side` names (the left side for a split
  /// view's left part), or nullopt when that side is in no region.
  [[nodiscard]] std::optional<std::size_t> region_of(const db::Plane& plane, const db::TileView& tile) const;

  /// Added in M7 R3: the discovery number of the substrate region of the
  /// last find_nodes(), if it made one.
  [[nodiscard]] std::optional<std::size_t> substrate_region() const noexcept { return substrate_region_; }

  /// Added in M7 R3: the discovery number of the first region of the last
  /// pass. Passes run while the slots of an earlier pass are kept
  /// (keep_clients) continue its numbering, so the slots of both passes
  /// stay distinct; otherwise each pass starts at 0. The NodeRegionList
  /// index of region d of the last pass is node_index(list.size(),
  /// d - first_region()).
  [[nodiscard]] std::size_t first_region() const noexcept { return first_region_; }

  /// Added in M7 R3: one past the last discovery number handed out.
  [[nodiscard]] std::size_t region_count() const noexcept { return region_count_; }

  /// Added in M7 R3: the NodeRegionList index (in `list`, a list of the
  /// last pass) of the region holding the side `tile` names, or nullopt.
  [[nodiscard]] std::optional<std::size_t> node_of(const db::Plane& plane, const db::TileView& tile,
                                                   std::size_t list_size) const;

 private:
  const tech::Technology* technology_ = nullptr;
  std::vector<std::array<std::uintptr_t, 2>> split_;  // per-side states of two-sided split tiles
  std::optional<std::size_t> substrate_region_;
  std::size_t first_region_ = 0;
  std::size_t region_count_ = 0;
  bool clients_live_ = false;  // the slots of the last pass were kept

  friend class RegionPass;
};

/// Added in M7 R3: the NodeRegionList index of region `d` (see above).
[[nodiscard]] constexpr std::size_t node_index(std::size_t list_size, std::size_t d) noexcept {
  return list_size - 1 - d;
}

/// ExtResetTiles: every client slot of `cell`'s planes back to
/// db::kClientDefault.
void reset(const db::CellDef& cell);

/// Added in M7 R3: the debug check of design D6: true when every client
/// slot of `cell`'s planes is db::kClientDefault.
[[nodiscard]] bool clients_clear(const db::CellDef& cell);

}  // namespace hefesto::extract
