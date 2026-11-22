// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RegionFinder: ExtFindRegions over a cell's planes (capability
// extract-nodes-and-labels "Node connectivity";
// openspec/changes/m7-extraction/design.md D5, D6, D8). It walks the planes
// in the style's plane order and each plane in db::Plane's tile order,
// keeps the region of each visited tile in the tile's client slot
// (db::Plane::client, D6) and resets the slots before it returns.
//
// Scaffold state (tasks.md 3.3): find() reports CORE-NOTIMPL-008 until
// EXTNODE lands it (tasks.md 5.1).
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
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

class RegionFinder {
 public:
  /// Finds the regions of `cell` within `area` over the types of `mask`
  /// connected by `connect`, calling `callbacks` for each region and tile.
  [[nodiscard]] diag::Outcome<NodeRegionList> find(const db::CellDef& cell, const db::Rect& area,
                                                   const tech::TypeMask& mask, const ConnectTable& connect,
                                                   RegionCallbacks& callbacks);
};

/// ExtResetTiles: every client slot of `cell`'s planes back to
/// db::kClientDefault.
void reset(const db::CellDef& cell);

}  // namespace hefesto::extract
