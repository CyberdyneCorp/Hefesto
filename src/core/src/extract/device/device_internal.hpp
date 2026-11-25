// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the per-device state of extOutputDevices (Magic's extTransRec)
// and the tile helpers the device sources share (capability
// extract-devices; openspec/changes/m7-extraction/design.md D10).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c, ExtPerim.c and
// ExtNghbors.c, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/extract/device/devices.hpp"

namespace hefesto::extract::device_detail {

inline constexpr std::size_t kMaxTerms = 10;  // MAXSD
inline constexpr int kNoAttr = -1;            // LL_NOATTR
inline constexpr int kGateAttr = -2;          // LL_GATEATTR
inline constexpr int kSortAttr = -3;          // LL_SORTATTR

inline constexpr unsigned kMatchId = 0x01;
inline constexpr unsigned kMatchSub = 0x04;
inline constexpr unsigned kMatchTerm = 0x08;

using NodeRef = std::optional<std::size_t>;
using PlaneTile = std::pair<tech::PlaneId, db::TileView>;

// --- Tile sides (Magic's TiGet{Top,Bottom,Left,Right}Type) ---

/// SplitDirection: true for `\` (nw_se).
[[nodiscard]] inline bool split_direction(const db::TileView& t) noexcept {
  return t.diagonal == db::Diagonal::nw_se;
}
[[nodiscard]] inline tech::TileType left_type(const db::TileView& t) noexcept {
  return t.split ? t.left_type : t.type;
}
[[nodiscard]] inline tech::TileType right_type(const db::TileView& t) noexcept {
  return t.split ? t.right_type : t.type;
}
[[nodiscard]] inline tech::TileType top_type(const db::TileView& t) noexcept {
  return t.split ? (split_direction(t) ? t.right_type : t.left_type) : t.type;
}
[[nodiscard]] inline tech::TileType bottom_type(const db::TileView& t) noexcept {
  return t.split ? (split_direction(t) ? t.left_type : t.right_type) : t.type;
}
/// The view of one side of a split tile (type and right_side set).
[[nodiscard]] db::TileView side_view(const db::TileView& t, bool right_side);

/// The sides of a split tile beyond its diagonal, which add no perimeter.
struct SkippedSides {
  bool left = false;
  bool right = false;
  bool top = false;
  bool bottom = false;
};
[[nodiscard]] SkippedSides skipped_sides(const db::TileView& t);

/// One perimeter segment (Magic's Boundary).
struct Boundary {
  db::TileView inside;
  db::TileView outside;  // == inside for a diagonal
  db::Rect segment;
  BoundaryDir dir = BoundaryDir::left;
  tech::PlaneId plane = 0;
  [[nodiscard]] std::int64_t length() const noexcept {
    return segment.width() > segment.height() ? segment.width() : segment.height();
  }
  [[nodiscard]] bool manhattan() const noexcept { return dir <= BoundaryDir::bottom; }
};

/// extEnumTilePerim: calls `fn` for every boundary of `tile` against a
/// type of `mask` (top right to left, bottom left to right, left bottom to
/// top, right top to bottom; the diagonal first); returns the diagonal
/// perimeter correction (int)sqrt(w^2 + h^2) when the other side's type is
/// in `mask`.
std::int64_t enum_tile_perim(const db::Plane& plane, tech::PlaneId pnum, const db::TileView& tile,
                             const tech::TypeMask& mask, const std::function<void(const Boundary&)>& fn);

/// The type of `tile` on the side a walk reached (TiGetTypeExact or the
/// split side).
[[nodiscard]] inline tech::TileType tile_type(const db::TileView& t) noexcept { return t.type; }

/// The visited tiles of a walk: (plane, tile, side).
using TileKey = std::tuple<tech::PlaneId, const void*, bool>;
using TileSet = std::set<TileKey>;
[[nodiscard]] inline TileKey key_of(tech::PlaneId plane, const db::TileView& t) noexcept {
  return {plane, t.tile, t.split && t.right_side};
}

/// ExtFindNeighbors' walk from `start` over unvisited tiles accepted by
/// `accept` (its plane and view) and connected under `conn`, in Magic's
/// stack order; marks them in `visited` and returns them in visit order.
std::vector<PlaneTile> walk_region(const db::CellDef& cell, const tech::Technology& technology,
                                   const std::vector<tech::TypeMask>& conn, const PlaneTile& start,
                                   TileSet& visited,
                                   const std::function<bool(tech::PlaneId, const db::TileView&)>& accept);

/// A terminal's position (TermTilePos).
struct TermPos {
  tech::PlaneId plane = 0;       // DBPlane(type) of the terminal type
  tech::PlaneId tile_plane = 0;  // the plane `tile` is on
  db::Point pt;
  std::optional<db::TileView> tile;
  tech::TileType type = tech::kSpace;
};

struct Terminal {
  NodeRef node;
  std::int64_t len = 0;
  std::int64_t area = 0;
  std::int64_t perim = 0;
  std::int64_t shared = 0;
  db::Point vector;
  TermPos pos;
};

/// extTransRec.
struct TransRec {
  const DevicePattern* rec = nullptr;
  unsigned match = 0;
  std::size_t nterm = 0;
  std::int64_t gatelen = 0;
  std::int64_t perim = 0;
  tech::PlaneId plane = 0;
  NodeRef subs_node;
  bool subs_set = false;  // tr_subsnode is non-NULL (a node or glob_subsnode)
  NodeRef gate_node;
  std::array<Terminal, kMaxTerms> terms{};
};

/// One label of the gate node with its attribute state (LabelList).
struct GateLabel {
  std::string text;
  db::Rect rect;
  int attr = kNoAttr;
};

/// Everything one cell's device pass shares.
struct Context {
  const DeviceInputs& in;
  const db::CellDef& cell;
  const tech::Technology& tech;
  const ExtStyle& style;
  DeviceNodeLookup node_of;
  DeviceResult& out;
  /// Per gate node, its labels with their attribute state (persists over
  /// the cell's devices, as Magic's label lists do).
  std::vector<std::optional<std::vector<GateLabel>>> labels;

  [[nodiscard]] NodeRef node(tech::PlaneId plane, const db::TileView& t) const { return node_of(plane, t); }
  [[nodiscard]] std::vector<GateLabel>& gate_labels(NodeRef gate);
  [[nodiscard]] tech::PlaneId home(tech::TileType t) const;
  [[nodiscard]] std::string type_name(tech::TileType t) const;
  [[nodiscard]] std::size_t pattern_index(const DevicePattern* p) const;
  void message(std::string_view code, diag::Severity severity, std::string text);
  void bad(const db::TileView& tile, std::string text);  // extTransBad
};

/// exts_deviceSDTypes[i] (zero past the line's masks).
[[nodiscard]] const tech::TypeMask& sd_types(const DevicePattern& p, std::size_t i);
/// The record after `p` in its type's list (exts_next), nullptr at the end.
[[nodiscard]] const DevicePattern* next_record(const ExtStyle& style, const DevicePattern* p, tech::TileType type);
/// extDevFindMatch.
[[nodiscard]] const DevicePattern* find_match(const Context& ctx, const TransRec& tr, const DevicePattern* entry,
                                              tech::TileType type);

/// extTransFindSubs: the first node of a `mask` type under `tile` grown by
/// 1 (planes in order, the first tile with a node), with its type.
struct Found {
  NodeRef node;
  tech::TileType layer = tech::kSpace;
};
[[nodiscard]] Found find_subs(const Context& ctx, const db::TileView& tile, const tech::TypeMask& mask);

/// extLabType: the label's role by its last character.
enum class LabelRole : std::uint8_t { name, node_attr, gate_attr, term_attr };
[[nodiscard]] LabelRole label_role(std::string_view text) noexcept;

// --- The passes of one device (device_scan.cpp, device_terms.cpp) ---

/// extTransTileFunc over every tile: terminals, gate length, perimeter,
/// substrate and identifier selection.
void scan_tiles(Context& ctx, const DeviceRegionTiles& reg, const std::vector<PlaneTile>& tiles, TransRec& tr);
/// The area, perimeter and sharing of the terminals found on the device
/// plane (extTermAPFunc over extEnumTerminal).
void terminal_areas(Context& ctx, const std::vector<PlaneTile>& tiles, TransRec& tr);
/// The terminals off the device plane; returns the selected record (nullptr
/// when none) and sets tr.nterm.
const DevicePattern* off_plane_terminals(Context& ctx, const DeviceRegionTiles& reg,
                                         const std::vector<PlaneTile>& tiles, TransRec& tr, std::size_t& nsd);

// --- Dimensions (device_dims.cpp) ---

/// The boundary lists of extAnnularTileFunc (sense true) or
/// extResistorTileFunc (sense false) for `n` terminals.
BoundaryLists special_bounds(Context& ctx, const std::vector<PlaneTile>& tiles, TransRec& tr, std::size_t n,
                             bool sense);

// --- Output (device_output.cpp) ---

/// extOutputDevParams: the parameter texts of `devptr` (the record after
/// range matching; `c` and `r` read tr.rec, as Magic's tr_devrec).
std::vector<std::string> device_params(Context& ctx, const DeviceRegionTiles& reg, const TransRec& tr,
                                       const DevicePattern& devptr, std::int64_t length, std::int64_t width);
/// extTransOutTerminal's values for one terminal (attributes consumed).
DeviceTerminal out_terminal(std::vector<GateLabel>& labels, NodeRef node, int which, std::int64_t len,
                            std::int64_t area, std::int64_t perim, std::int64_t shared);
/// ExtSortTerminals.
void sort_terminals(Context& ctx, TransRec& tr, std::vector<GateLabel>& labels);

/// printf("%g") in the C locale.
[[nodiscard]] std::string format_g(double value);

/// `int *= float`: the product in float truncated to int, saturated to the
/// int range and NaN as 0 (Q-EXTRACT-30).
[[nodiscard]] std::int64_t mul_trunc(std::int64_t value, float by) noexcept;

/// Proposed Q-EXTRACT-41 (M7 round 4, EX-P03, two device types touching in one
/// device region): with magic-compat OFF each device type is its own device,
/// so the device connectivity keeps only a type's own tiles; ON returns the
/// style's table unchanged (Magic's one region and one record).
[[nodiscard]] std::vector<tech::TypeMask> device_connectivity(const std::vector<tech::TypeMask>& conn,
                                                              bool magic_compat);

}  // namespace hefesto::extract::device_detail
