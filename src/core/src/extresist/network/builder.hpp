// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResExtractNet after the copy (capability extresist-network, tasks.md
// 13.1-13.4): the device areas, the contact points, contact dissolving,
// fracturing, the per-tile records (resInfo, kept in the scratch planes'
// client slots), the ports, the start tile and the tile queue. The
// resistor routines of ResMakeRes.c are in resistors.cpp, the tile
// routines of ResBasic.c and ResJunct.c in tiles.cpp, the preprocessing of
// ResMain.c and ResUtils.c in prepare.cpp and devices.cpp.
//
// Every stitch walk goes through a db::StitchEditor on a plane of the
// scratch cell; no cell plane reaches an editor (design D6, R6; the check
// is ScratchCheck in network.cpp).
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "extresist/network/graph.hpp"
#include "hefesto/db/plane_stitch.hpp"
#include "hefesto/extresist/network/network.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extresist::detail {

/// A tile of a scratch plane.
struct TileRef {
  tech::PlaneId plane = 0;
  db::StitchTile tile;

  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(tile); }
  friend bool operator==(const TileRef&, const TileRef&) = default;
};

/// The types of a tile (TiGetTypeExact and the split-tile accessors).
struct TileTypes {
  bool split = false;
  int direction = 0;  // SplitDirection: 1 for the nw_se diagonal
  tech::TileType left = 0;
  tech::TileType right = 0;

  [[nodiscard]] tech::TileType top() const noexcept { return split ? (direction != 0 ? right : left) : left; }
  [[nodiscard]] tech::TileType bottom() const noexcept { return split ? (direction != 0 ? left : right) : left; }
};

/// ResDevTile.
struct DevTile {
  db::Rect area;
  tech::TileType type = 0;
  const extract::DevicePattern* record = nullptr;
  std::int32_t perim = 0;
  std::int32_t overlap = 0;
};

/// A resDevTerm of the device-node table.
struct DevTerm {
  std::int32_t term = -1;  // -1: substrate
  TileRef device_tile;
};

class Builder {
 public:
  Builder(const NetGeometry& geometry, const extract::ExtStyle& style, const DeviceAnnotations& devices,
          const BuildInput& input);

  /// Runs the construction; false when ResExtractNet fails (the messages
  /// say why).
  bool run();
  [[nodiscard]] Network finish();

  // --- the scratch planes (builder.cpp) ---
  db::StitchEditor& ed(tech::PlaneId plane) { return *editors_[plane]; }
  [[nodiscard]] db::Rect area(const TileRef& t) { return ed(t.plane).area(t.tile); }
  [[nodiscard]] TileTypes types(const TileRef& t);
  [[nodiscard]] TileRef stitch(const TileRef& t, db::Stitch s) {
    return TileRef{t.plane, ed(t.plane).stitch(t.tile, s)};
  }
  [[nodiscard]] TileRef goto_point(tech::PlaneId plane,
                                   db::Point p);  // GOTOPOINT from the hint, then PlaneSetHint
  [[nodiscard]] Id info(const TileRef& t);        // the resInfo, or kNil
  Id add_field(const TileRef& t);                 // resAddField
  [[nodiscard]] TileRef tile_of(Id info) const { return TileRef{g.tiles[info].plane, g.tiles[info].tile}; }
  /// The tiles of `plane` over `area` whose side types are in `mask`, in
  /// area-enumeration order (DBSrPaintArea); `right_side` is TT_SIDE.
  void search(tech::PlaneId plane, const db::Rect& area, const tech::TypeMask& mask,
              const std::function<bool(const TileRef&, bool right_side)>& visit);
  /// The four sides' neighbours in Magic's walk orders: top RT..BL, bottom
  /// LB..TR, right TR..LB, left BL..RT. The visitor returns true to stop.
  bool each_top(const TileRef& t, const std::function<bool(const TileRef&)>& visit);
  bool each_bottom(const TileRef& t, const std::function<bool(const TileRef&)>& visit);
  bool each_right(const TileRef& t, const std::function<bool(const TileRef&)>& visit);
  bool each_left(const TileRef& t, const std::function<bool(const TileRef&)>& visit);

  // --- style queries ---
  [[nodiscard]] std::vector<const extract::DevicePattern*> records(tech::TileType type) const {
    return style_.records_for(type);
  }
  [[nodiscard]] static tech::TypeMask sd_types(const extract::DevicePattern& d, std::int32_t i);
  [[nodiscard]] std::string type_name(tech::TileType type) const { return std::string(tech_.type_name(type)); }
  [[nodiscard]] std::vector<tech::PlaneId> user_planes() const;

  // --- prepare.cpp: devices, contacts, dissolving, fracturing ---
  void copy_devices();
  void shave_contacts();
  void find_contacts();
  void dissolve_contacts();
  bool fracture_planes();
  // --- devices.cpp: plumbing and preprocessing ---
  void add_device_plumbing(const DevTile& dev);
  void add_plumbing();
  void make_ports();
  void make_label_ports();
  void find_new_contact_tiles();
  void preprocess_devices();
  // --- tiles.cpp: the queue ---
  bool process_tiles();
  // --- resistors.cpp ---
  bool calc_tile_resistance(Id tile);
  bool near_device(Id tile);
  void do_contacts(Id contact);
  /// The cut counts of a contact (squares x and y), 1 x 1 with the
  /// once-per-session warning when it is smaller than a cut allows.
  std::pair<std::int32_t, std::int32_t> via_counts(Id contact, std::int64_t minside, std::int64_t pitch,
                                                   std::int64_t cscale);

  Graph g;
  const NetGeometry& geo;
  const extract::ExtStyle& style_;
  const DeviceAnnotations& in;
  const BuildInput& input;
  const tech::Technology& tech_;
  db::CellDef& def;
  db::Rect bbox;
  std::vector<DevTile> dev_tiles;  // DevTiles, head first
  std::vector<std::size_t> dev_tile_input;
  std::unordered_map<const void*, std::vector<DevTerm>> dev_nodes;  // per tile, head first
  tech::TileType start_type = 0;                                    // rg_ttype
  db::Point start;
  std::vector<NetPort> added_sinks;

  void open_editors();

 private:
  std::vector<std::unique_ptr<db::StitchEditor>> editors_;
};

}  // namespace hefesto::extresist::detail
