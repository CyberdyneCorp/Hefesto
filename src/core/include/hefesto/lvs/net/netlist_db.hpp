// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs::NetlistDb: the netlist database the LVS readers fill and the
// comparison reads (openspec/changes/m9-lvs-simulation design.md D6, "Area
// contracts" LVSREAD; capability lvs-netlist-input "Netlist database model",
// "Device classes").
//
// A cell is identified by (name, file number). It holds its ports, its nets,
// its instances, its device class, the property definitions ("properties of
// interest"), the permutable pin pairs and the flags of the spec. Nets are
// numbered from 1 (NetId); kNoNet marks an unconnected port or connection.
// Cell and net lookups go through name_key() (the bracket-folding key of
// Q-LVS-3) and then names_equal() with the cell's case sensitivity.
//
// Owner: LVSREAD.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hefesto/lvs/net/device_class.hpp"

namespace hefesto::lvs {

/// Index of a cell in NetlistDb::cells.
using CellId = std::uint32_t;

/// A net of a cell: 1 + its index in Cell::nets; kNoNet is "no net".
using NetId = std::uint32_t;
inline constexpr NetId kNoNet = 0;

/// The number of one netlist read: 0, 1, ... in order of reading.
using FileNumber = std::int32_t;

/// Devices per run (design D18, Q-LVS-36): a setting may lower it and never
/// raise it past twice this value.
inline constexpr std::uint64_t kMaxDevicesPerRun = 50'000'000;

/// Property types of lvs-property-matching.
enum class PropertyType : std::uint8_t { string, integer, real, value, expression };

/// One property of interest of a cell: its key, type, tolerance and default
/// as written (e.g. "1").
struct PropertyDef {
  std::string key;
  PropertyType type = PropertyType::string;
  double tolerance = 0.0;
  std::string default_value;

  friend bool operator==(const PropertyDef&, const PropertyDef&) = default;
};

/// One key/value pair of an instance, stored as read (unconverted).
struct Property {
  std::string key;
  std::string value;

  friend bool operator==(const Property&, const Property&) = default;
};

/// An ordered list of key/value pairs; an instance has zero or more.
using PropertyRecord = std::vector<Property>;

/// The value of `key` in `record` (first match by name_key), or nullptr.
[[nodiscard]] const Property* find_property(const PropertyRecord& record, std::string_view key) noexcept;

struct Net {
  std::string name;
  bool global = false;
};

struct Port {
  std::string name;  // empty for the proxy port of a port-less cell
  NetId net = kNoNet;
  bool proxy = false;
};

struct Instance {
  CellId cell = 0;   // the class: a cell of the same database
  std::string name;  // e.g. "sky130_fd_pr__nfet_01v8:1", "/inv_1"
  std::vector<NetId> connections;  // one per port of `cell`, in port order
  std::vector<PropertyRecord> properties;
};

struct PinPair {
  std::string first;
  std::string second;
};

struct CellFlags {
  bool top = false;
  bool placeholder = false;
  bool case_insensitive = false;
  bool duplicate = false;
  bool matched = false;
  bool verilog = false;
  bool series_combinable = false;
  bool no_parallel = false;

  [[nodiscard]] bool any() const noexcept;
};

/// One cell definition. Add nets with add_net() so that find_net() sees them.
struct Cell {
  Cell() = default;
  explicit Cell(std::string cell_name, FileNumber cell_file = 0) : name(std::move(cell_name)), file(cell_file) {}

  std::string name;
  FileNumber file = 0;
  DeviceClass device_class = DeviceClass::undefined;
  std::vector<Port> ports;
  std::vector<Net> nets;  // every net, in order of creation
  std::vector<Instance> instances;
  std::vector<PropertyDef> properties;  // the properties of interest
  std::vector<PinPair> permutes;
  CellFlags flags;

  /// The net named `net_name` under the cell's case sensitivity, or kNoNet.
  [[nodiscard]] NetId find_net(std::string_view net_name) const;
  /// Appends a net (no check for an existing one) and indexes it.
  NetId add_net(std::string net_name, bool global = false);
  /// find_net(), else add_net().
  NetId net_named(std::string_view net_name);
  [[nodiscard]] const Net& net(NetId id) const { return nets.at(id - 1); }
  [[nodiscard]] Net& net(NetId id) { return nets.at(id - 1); }
  /// The nets that are no port's net, in order.
  [[nodiscard]] std::vector<NetId> internal_nets() const;
  /// Keeps the nets for which keep[id - 1] is true, renumbers the rest and
  /// every port and instance connection (a dropped net becomes kNoNet).
  void compact_nets(const std::vector<bool>& keep);
  /// Drops every net and the net index (ports and instances are kept).
  void clear_nets();
  /// The property of interest named `key`, or nullptr.
  [[nodiscard]] const PropertyDef* find_property_def(std::string_view key) const noexcept;

 private:
  void reindex_nets();
  std::unordered_map<std::string, std::vector<NetId>> net_index_;
};

/// The .PARAM values of one file: keys by name_key(), values as written.
using ParameterTable = std::map<std::string, std::string, std::less<>>;

/// One file read into the database: its name, number and top cell.
struct LoadedFile {
  std::string name;
  FileNumber number = 0;
  CellId top = 0;
};

struct NetlistDb {
  std::vector<Cell> cells;
  /// The files read, in order (includes are part of their top file).
  std::vector<LoadedFile> files;
  /// The next file number a new read receives.
  FileNumber next_file = 0;
  /// A case-sensitive format (.sim, .ext, Verilog) was read into this
  /// database; later SPICE reads stay case-sensitive.
  bool case_sensitive_read = false;
  /// .PARAM values per file number.
  std::map<FileNumber, ParameterTable> parameters;

  /// Appends `cell` and indexes it by name; returns its id.
  CellId add_cell(Cell cell);
  /// The cell named `name` in `file` (names_equal() with that cell's case
  /// sensitivity); the most recently added one if several match.
  [[nodiscard]] std::optional<CellId> find_cell(std::string_view name, FileNumber file) const;
  /// The cell named exactly `name` (byte equality) in `file`.
  [[nodiscard]] std::optional<CellId> find_cell_exact(std::string_view name, FileNumber file) const;
  /// Renames a cell and keeps the index current.
  void rename_cell(CellId id, std::string name);
  /// Added in M9 R1: drops the cells from `count` on and rebuilds the name
  /// index (the rollback of a failed read).
  void truncate_cells(std::size_t count);
  /// The number of instances in all cells.
  [[nodiscard]] std::uint64_t instance_count() const noexcept;

 private:
  std::unordered_map<std::string, std::vector<CellId>> cell_index_;
};

/// Matching between two compared top cells is case-insensitive only when
/// both were read case-insensitively ("Name matching and case sensitivity").
[[nodiscard]] bool compare_case_insensitive(const Cell& first, const Cell& second) noexcept;

/// Q-LVS-24 (`Resolution: compat`, design D5): whether the size-1-array
/// pin-name equivalence treats `cell` as a placeholder. netgen-compat ON
/// keeps netgen's logical test (any flag set); OFF tests only the
/// placeholder flag. The one branch point of Q-LVS-24.
[[nodiscard]] bool placeholder_pin_equivalence(const Cell& cell, bool netgen_compat) noexcept;

}  // namespace hefesto::lvs
