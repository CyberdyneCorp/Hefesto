// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The route writer of DEF write's net sections (capability def-write "Wire
// segment derivation", "Route record assembly", "Net record header and
// connections"; Q-LEFDEF-95, 96, 97): defNetGeometryFunc with its DefData
// state, defCheckForBreak, defWriteCoord and defWriteRouteWidth.
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "def/write/internal.hpp"
#include "hefesto/db/plane.hpp"

namespace hefesto::def::write_detail {

/// GEO_CENTER, GEO_NORTH, GEO_EAST: the orientation of one route piece.
enum class Orient : std::uint8_t { center, north, east };

/// One tile (or contact region) after wire segment derivation.
struct Segment {
  tech::TileType type = tech::kSpace;
  db::Rect r;      // the piece (a sliver grown vertically)
  db::Rect rorig;  // the tile, or the contact region
  db::Coord w = 0;
  db::Coord h = 0;
  db::Coord width = 0;  // routeWidth
  db::Coord midline = 0;
  Orient orient = Orient::east;
  float x1 = 0;
  float y1 = 0;
  float x2 = 0;
  float y2 = 0;
  tech::TileType rtype = tech::kSpace;
  tech::TileType r2type = tech::kSpace;
};

class RouteWriter {
 public:
  RouteWriter(const Context& c, NetMode mode, const ViaPositions& vias, RuleBook& rules, std::string& out);

  /// Starts a net record: the column after `   - <name>`.
  void begin_net(std::size_t name_length);
  /// defCheckForBreak.
  void check_break(std::size_t add);
  /// Starts one connectivity search (clears the exempt tiles).
  void begin_search() { exempt_.clear(); }
  /// defNetGeometryFunc for one tile reached on `plane`.
  void tile(const db::TileView& tile, tech::PlaneId plane);
  /// A tile has been written for this net.
  [[nodiscard]] bool wrote() const noexcept { return has_prev_; }
  /// The pending end point of the last piece, and the column reset.
  void end_net();

 private:
  void contact_regions(const db::TileView& tile, tech::PlaneId plane);
  void process(const db::TileView& tile, tech::PlaneId plane, tech::TileType type, const db::Rect& area);
  void set_width(Segment& s, const LefMapping& m) const;
  bool wire(Segment& s, tech::PlaneId plane, const std::string& lef_name);
  bool grow_sliver(Segment& s, tech::PlaneId plane);
  void wire_endpoints(Segment& s) const;
  static void via_endpoints(Segment& s, float scale);
  void residues(Segment& s) const;
  [[nodiscard]] bool same_route(const Segment& s) const;
  void emit(const Segment& s, bool same, const std::string& lef_name);
  void continue_through_via(const Segment& s, const std::string& lef_name);
  void new_record(const Segment& s, const std::string& lef_name);
  void continue_route(const Segment& s, const std::string& lef_name);
  void restart(const Segment& s, const std::string& lef_name, bool column_after_width);
  void via_name(const Segment& s, const std::string& lef_name);
  void route_layer(const Segment& s);
  void coord(float x, float y, Orient o);
  void route_width(db::Coord width);
  void update(const Segment& s, const db::TileView& tile);

  const Context& c_;
  NetMode mode_;
  const ViaPositions& vias_;
  RuleBook& rules_;
  std::string& out_;
  std::multimap<std::tuple<tech::PlaneId, db::Coord, db::Coord>, lef::ContactRegion> regions_;
  std::set<const void*> exempt_;
  std::set<std::pair<tech::PlaneId, const void*>> seen_;  // Q-LEFDEF-112 OFF

  // DefData.
  std::size_t column_ = 0;
  bool has_prev_ = false;
  db::Rect prev_area_;
  tech::TileType prev_type_ = tech::kSpace;
  float px_ = 0;
  float py_ = 0;
  Orient prev_orient_ = Orient::east;
  std::optional<RuleBook::Selected> rule_;
};

}  // namespace hefesto::def::write_detail
