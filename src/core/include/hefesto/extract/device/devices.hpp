// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device recognition and substrate nodes (capabilities extract-devices,
// extract-substrate; openspec/changes/m7-extraction/design.md D10).
//
// The extractor runs in Magic's order (extBasic, ExtBasic.c:241-560):
//   1. find_device_regions() before the node pass (ExtFindRegions over the
//      device types with the device connectivity);
//   2. the node pass (EXTNODE's RegionFinder), which leaves each node tile's
//      region index in its plane client slot; substrate_seeds() gives the
//      tiles that form the substrate node, which the node pass makes first;
//   3. find_devices() for every kept region: terminals, record selection,
//      dimensions, parameters, as data (DeviceRecord) that EXTHIER hands to
//      ExtWriter::device (the record's text fields are ready to write);
//   4. parameter_records() and device_property() for the `parameters`
//      records and the `device` cell property.
//
// Nodes are read through DeviceNodeLookup: by default the plane client slot of
// the tile (EXTNODE's region index, db::kClientDefault = no node).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the contract); EXTDEV (this header's types and the
// implementation, tasks.md 7.1-7.6).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/app/project_file.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {

/// The node of a tile (the region index into the cell's NodeRegionList), or
/// nullopt when the tile belongs to no node. For a split tile `tile`
/// reports the side asked about.
using DeviceNodeLookup = std::function<std::optional<std::size_t>(tech::PlaneId plane, const db::TileView& tile)>;

/// The default DeviceNodeLookup: the tile's plane client slot as EXTNODE leaves
/// it (the region index; db::kClientDefault = no node).
[[nodiscard]] DeviceNodeLookup client_slot_lookup(const db::CellDef& cell);

/// The DeviceNodeLookup of a production pass (added at the M7 R3
/// integration): RegionFinder::node_of over the slots `finder` kept
/// (keep_clients) for the pass whose NodeRegionList has `list_size`
/// entries. client_slot_lookup() reads a plain index and serves fixtures
/// that write it themselves; RegionFinder encodes its slots (regions.hpp).
[[nodiscard]] DeviceNodeLookup region_slot_lookup(const RegionFinder& finder, const db::CellDef& cell,
                                                  std::size_t list_size);

/// One device region (TransRegion): the connected device-type paint of
/// one device, with its tiles in discovery order.
struct DeviceRegionTiles {
  DeviceRegion region;        // reference point, type, plane, area
  db::TileView first;         // the representative tile (treg_tile)
  tech::PlaneId first_plane = 0;
  std::vector<std::pair<tech::PlaneId, db::TileView>> tiles;
  bool disabled = false;      // "Subcell device priority" (task 7.7) sets it
};

/// ExtFindRegions over the device types (exts_deviceMask) with the device
/// connectivity (exts_deviceConn), in region-list order: the reverse of the
/// discovery order (planes in increasing order, tiles in enumeration order).
/// Reads no client slot.
[[nodiscard]] std::vector<DeviceRegionTiles> find_device_regions(const db::CellDef& cell,
                                                                 const tech::Technology& technology,
                                                                 const ExtStyle& style);

/// One terminal of a recognised device, with the values written for it.
struct DeviceTerminal {
  /// Index into the cell's NodeRegionList; nullopt is written `(none)`
  /// (extNodeName of a NULL node: a substrate terminal without substrate node).
  std::optional<std::size_t> node;
  std::int64_t length = 0;
  std::vector<std::string> attributes;  // label texts without their last character
  std::int64_t area = 0;       // written area (area / sharing); 0 with perimeter 0 = none written
  std::int64_t perimeter = 0;  // written perimeter (perimeter / sharing)

  friend bool operator==(const DeviceTerminal&, const DeviceTerminal&) = default;
};

/// How the substrate field of a device record is written.
enum class SubstrateField : std::uint8_t {
  none,  // not written
  node,  // the name of node `DeviceRecord::substrate`
  name,  // the text `DeviceRecord::substrate_name`
};

/// One recognised device, ready for ExtWriter::device:
///   <keyword> <class> <model> <x> <y> <x+1> <y+1>{ fields}{ params} [substrate]
///   gate terminals...
/// (`fet` records: `fet <model> ... <area> <perim> "<substrate>"`).
struct DeviceRecord {
  std::size_t pattern = 0;  // index into ExtStyle::devices of the selected line
  db::Rect area;            // (x, y)-(x+1, y+1) of the region reference point
  std::optional<std::size_t> substrate;  // node index when `substrate_field` is node
  std::string substrate_name;            // the text when `substrate_field` is name
  SubstrateField substrate_field = SubstrateField::none;
  DeviceTerminal gate;
  std::vector<DeviceTerminal> terminals;
  std::int64_t length = 0;
  std::int64_t width = 0;
  std::vector<std::string> fields;  // the class-specific fields after the header, as written
  std::vector<std::string> params;  // `name=value`, in written order (gate attributes last)

  friend bool operator==(const DeviceRecord&, const DeviceRecord&) = default;
};

/// A feedback area (extTransBad: DBWFeedbackAdd with the message).
struct DeviceFeedback {
  db::Rect area;
  std::string text;

  friend bool operator==(const DeviceFeedback&, const DeviceFeedback&) = default;
};

/// What find_devices() produced for a cell.
struct DeviceResult {
  std::vector<DeviceRecord> devices;  // in region-list order
  std::vector<DeviceFeedback> feedback;
  std::vector<diag::Diagnostic> messages;  // Magic's console messages (EXTRACT-DEV-*)
  std::int32_t warnings = 0;               // extNumWarnings added by this cell
};

/// The inputs of find_devices().
struct DeviceInputs {
  const db::CellDef* cell = nullptr;
  const tech::Technology* technology = nullptr;
  const ExtStyle* style = nullptr;
  const NodeRegionList* nodes = nullptr;
  /// The cell's substrate node (glob_subsnode), nullopt when none.
  std::optional<std::size_t> substrate_node;
  compat::CompatSettings compat;
  bool warn_fets = true;   // ExtDoWarn & EXTWARN_FETS (default on)
  bool warn_any = true;    // ExtDoWarn != 0
  DeviceNodeLookup node_of;      // empty: client_slot_lookup(*cell)
  /// Variable lookup for `$` substrate names in backward-compatible mode
  /// (Q-EXTRACT-9); empty: no variable is defined.
  std::function<std::optional<std::string>(std::string_view)> variable;
};

/// extOutputDevices over `regions` (find_device_regions() of the same cell,
/// after the node pass).
[[nodiscard]] DeviceResult find_devices(const DeviceInputs& inputs, std::vector<DeviceRegionTiles>& regions);

/// The same, finding the regions itself.
[[nodiscard]] diag::Outcome<DeviceResult> find_devices(const DeviceInputs& inputs);

/// Gate (L, W) and resistor or capacitor dimensions.
struct Dimensions {
  std::int64_t length = 0;
  std::int64_t width = 0;

  friend bool operator==(const Dimensions&, const Dimensions&) = default;
};

/// The per-terminal values extDevFindParamMatch reads (index = terminal).
struct TerminalValues {
  std::vector<std::int64_t> length;
  std::vector<std::int64_t> area;
  std::vector<std::int64_t> perimeter;
};

/// extDevFindParamMatch: starting at `pattern` (an index into
/// ExtStyle::devices), the first record of `type`'s list, in list order,
/// whose parameter ranges accept `dims` (Q-EXTRACT-21: internal units with
/// magic-compat ON, milli-internal-units with it OFF). With magic-compat
/// ON, as Magic, a range with only a lower limit is not checked and the
/// last record tried is returned when none passes; with it OFF lower-only
/// ranges apply and the starting record is returned. `mismatch` is set
/// when none passed.
[[nodiscard]] std::size_t select_pattern(const ExtStyle& style, tech::TileType type, std::size_t pattern,
                                         const Dimensions& dims, const TerminalValues& terminals,
                                         const compat::CompatSettings& compat, bool* mismatch = nullptr);

/// One segment of a terminal boundary list (LinkedBoundary).
enum class BoundaryDir : std::uint8_t { left, right, top, bottom, nw, ne, sw, se };
struct BoundarySegment {
  db::Rect r;
  BoundaryDir dir = BoundaryDir::left;

  friend bool operator==(const BoundarySegment&, const BoundarySegment&) = default;
};
/// One list per terminal, each in Magic's list order (the newest first).
using BoundaryLists = std::vector<std::vector<BoundarySegment>>;

/// extSeparateBounds(n - 1): when the last list is empty, moves the
/// segments of list 0 that chain with its first segment into it.
void separate_bounds(BoundaryLists& lists, std::size_t n);

/// extComputeEffectiveLW over the first `n` lists (ExtBasic.c:1259-1579):
/// nullopt when the result is not L > 0 and W > 0. `multiple_lengths` is
/// set when the segments gave differing lengths.
[[nodiscard]] std::optional<Dimensions> effective_lw(const BoundaryLists& lists, std::size_t n, float chop,
                                                     bool* multiple_lengths = nullptr);

/// extComputeCapLW: the bounding box of list 0 (L = height, W = width), or
/// nullopt when it is empty.
[[nodiscard]] std::optional<Dimensions> cap_lw(const BoundaryLists& lists);

/// The equivalent-square fallback of capacitors and diodes when cap_lw()
/// fails: (int)(sqrt(area) + 0.5) (ExtBasic.c:2948, 3206).
[[nodiscard]] std::int64_t equivalent_square(std::int64_t area) noexcept;

/// The width of a `w<n>` parameter from terminal n's area and perimeter:
/// (int)(0.25 * (sqrt(p*p - 16a) + p)) (ExtBasic.c:1916-1920).
[[nodiscard]] std::int64_t terminal_width(std::int64_t area, std::int64_t perimeter) noexcept;

/// The `parameters` records (extOutputParameters): one line text per
/// record (`parameters <model> k=name...`), for the types of the kept
/// regions and the type named by the `device` cell property.
[[nodiscard]] std::vector<std::string> parameter_records(const ExtStyle& style,
                                                         const std::vector<DeviceRegionTiles>& regions,
                                                         const db::CellDef& cell);

/// The `device` cell property ("Device cell property"; Q-EXTRACT-16).
struct DeviceProperty {
  bool found = false;
  bool primitive = false;
  std::string parameters_line;  // `parameters :<cell> <rest>` for a primitive
  std::string device_line;      // the replacement device line; empty on a parse error
  std::vector<diag::Diagnostic> messages;
};
[[nodiscard]] DeviceProperty device_property(const db::CellDef& cell, const ExtStyle& style);

/// The substrate regions of a cell and the global substrate node's name
/// (`$NAME` substitution, Q-EXTRACT-9).
struct SubstrateNodes {
  std::string global_name;            // the name of a tile-less substrate node; empty = none
  std::vector<std::size_t> isolated;  // unused until EXTHIER (kept for the contract)
};

/// The global substrate name from the style's declaration: NAME, or for
/// `$VAR` the variable's value (`SUB`, `VDD`, `GND` from the project's
/// global nets; others undefined), or VAR when undefined.
[[nodiscard]] diag::Outcome<SubstrateNodes> prepare_substrate(const db::CellDef& cell, const ExtStyle& style,
                                                             const app::GlobalNets& nets);

/// extSubsName: the substrate name for a `$`-prefixed declaration with an
/// arbitrary variable lookup.
[[nodiscard]] std::string substrate_name(std::string_view declared,
                                         const std::function<std::optional<std::string>(std::string_view)>& lookup);

/// The tiles that form the cell's substrate node before every other node
/// (extFindNodes' first pass, ExtBasic.c:5085-5170), in push order; empty
/// tiles with `tileless` set when the node has no tile (reference
/// (-1073741817, -1073741817), plane P_sub, type space).
struct SubstrateSeeds {
  std::vector<std::pair<tech::PlaneId, db::TileView>> tiles;
  bool tileless = false;  // a substrate node with no tiles
  bool none = false;      // no substrate declaration: no substrate node
};
[[nodiscard]] SubstrateSeeds substrate_seeds(const db::CellDef& cell, const tech::Technology& technology,
                                             const ExtStyle& style, bool full_rc = false);

/// "Substrate record": the substrate node is written as a `substrate`
/// record unless the cell has the `LEFview` property (then it is an
/// ordinary `node` and the cell has no substrate node designation).
[[nodiscard]] bool writes_substrate_record(const db::CellDef& cell);

/// extRelocateSubstrateCoupling: every non-zero coupling between a node
/// and `substrate` becomes substrate capacitance of the other node; the
/// returned table holds the remaining pairs.
[[nodiscard]] CouplingTable relocate_substrate_coupling(const CouplingTable& table, std::size_t substrate,
                                                        NodeRegionList& nodes);

/// The reference point of a tile-less substrate node: MINFINITY + 3.
inline constexpr db::Coord kTilelessSubstrateCoord = db::kMinusInfinity + 3;

/// extPrepSubstrate: the substrate plane with isolated substrate areas
/// painted with the default substrate type (DBCellGenerateSubstrate over
/// the flattened hierarchy), or nullopt when the style needs none (no
/// substrate plane, no default type, or no shield paint).
[[nodiscard]] std::optional<db::Plane> isolated_substrate_plane(const db::CellDef& cell,
                                                                const tech::Technology& technology,
                                                                const ExtStyle& style);

}  // namespace hefesto::extract
