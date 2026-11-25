// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SpiceInput: everything the SPICE netlister reads from a flattened netlist
// and the extraction style (capabilities netlist-spice-options,
// netlist-spice-devices; openspec/changes/m7-extraction/design.md D5, D14).
// It holds Magic's EFNode, Dev, DevParam and esFetInfo data in the visit
// orders of extflat::Netlist (EFVisitDevs, EFVisitCaps, EFVisitResists,
// EFVisitNodes). from_netlist() builds it from an extflat::Netlist and the
// current style's device_info(); the writer (writer.hpp) never reads the
// session, so tests and M9 can hand it a value.
//
// Owner: SPICE.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::netlist::spice {

/// The `.ext` device classes (DEV_* of extflat.h).
enum class DeviceClass : std::uint8_t {
  fet,
  mosfet,
  asymmetric,
  bjt,
  diode,
  pdiode,
  ndiode,
  res,
  cap,
  caprev,
  volt,
  subckt,
  rsubckt,
  csubckt,
  dsubckt,
  msubckt,
  veriloga,
};

/// The class of a `device` record keyword (`mosfet`, `devres`, ...) or of
/// an old `fet` record; nullopt for an unknown keyword.
[[nodiscard]] std::optional<DeviceClass> device_class_from(std::string_view keyword) noexcept;

/// Area and perimeter of a node for one resistance class (EFPerimArea).
struct AreaPerim {
  std::int64_t area = 0;
  std::int64_t perim = 0;

  friend bool operator==(const AreaPerim&, const AreaPerim&) = default;
};

/// One flat node (EFNode).
struct InputNode {
  /// The canonical name as its components from the root side.
  std::vector<std::string> name;
  /// Every name the node answers to, each `/`-joined (the canonical one
  /// included); terminals, caps and resistors refer to nodes by these.
  std::vector<std::string> aliases;
  float cap = 0.0F;               // efnode_cap, attofarads
  std::vector<AreaPerim> pa;      // efnode_pa, one per resistance class
  std::vector<std::string> attributes;  // efnode_attrs, in list order
  bool substrate = false;         // EF_GLOB_SUBS_NODE
  bool port = false;              // EF_PORT
};

/// One device terminal (DevTerm).
struct InputTerminal {
  std::string node;                       // a node alias
  std::int64_t length = 0;                // dterm_length
  std::optional<std::string> attributes;  // dterm_attrs (nullopt: none)
};

/// One flat device (Dev).
struct InputDevice {
  DeviceClass device_class = DeviceClass::mosfet;
  std::string type;  // the model name (EFDevTypes[dev_type])
  std::vector<InputTerminal> terminals;
  std::optional<std::string> substrate;  // dev_subsnode, a node alias
  /// The substrate node's name inside the device's own cell (the suffix
  /// spcdevSubstrate compares with the default substrate name).
  std::string substrate_local;
  /// The instance path of the cell the device came from, root side first
  /// (empty at the top level).
  std::vector<std::string> prefix;
  std::int64_t length = 0;  // EFGetLengthAndWidth
  std::int64_t width = 0;
  std::int64_t area = 0;    // dev_area
  std::int64_t perim = 0;   // dev_perim
  db::Rect rect;            // dev_rect
  float resistance = 0.0F;  // dev_res, ohms
  float capacitance = 0.0F; // dev_cap, femtofarads
  std::vector<std::string> params;  // dev_params: verbatim `name=value`, stored order
  float scale = 1.0F;       // the visit's scale (1 unless .ext scales differ)
};

/// One declared parameter of a device type or a cell (DevParam, from the
/// `parameters` records), in stored order (Q-NETLIST-9: reversed).
struct ParamDecl {
  std::string key;   // parm_type: `a`, `p1`, `l`, `w2`, `s`, `x`, `r`, `c`, ...
  std::string name;  // parm_name, the text written before `=`
  double scale = 1.0;
  int offset = 0;
};

/// One entry of the root cell's node-name table whose node is a port, in
/// the table's iteration order (extflat::PortEntry): the entry's name and
/// port index (-1: none given), the names after it in the node's list, and
/// the node's canonical name when it is another name.
struct RootPort {
  std::string node;  // the entry's name, a node alias
  int index = -1;
  std::vector<extflat::PortName> rest{};
  std::optional<extflat::PortName> canonical{};
};

/// Per device type: esFetInfo (ExtGetDevInfo through ExtStyle::device_info()).
struct DeviceTypeInfo {
  std::int32_t source_class = -1;
  std::int32_t drain_class = -1;
  std::int32_t substrate_class = -1;
  std::optional<std::string> default_substrate;
};

struct SpiceInput {
  std::string cell;       // the root cell
  std::string tech;       // EFTech
  float scale = 1.0F;     // EFScale (S_out)
  /// FALSE when the `.ext` style does not match the current style (EFStyle
  /// NULL): areas, perimeters and resistance classes are then zero.
  bool style_matches = true;
  /// TRUE when no `substrate` record was read (EFCompat).
  bool compat_substrate = false;
  std::size_t resist_classes = 0;         // efNumResistClasses
  std::vector<InputNode> nodes;           // EFVisitNodes order
  std::vector<InputDevice> devices;       // EFVisitDevs order
  std::vector<extflat::FlatCap> caps;     // EFVisitCaps order, attofarads
  std::vector<extflat::FlatResist> resists;  // EFVisitResists order, milliohms
  /// Declared parameters by device type, or by `:<cell>` for a cell.
  std::map<std::string, std::vector<ParamDecl>, std::less<>> params;
  std::vector<RootPort> ports;
  bool root_abstract = false;
  /// esFetInfo by device type name (the last device_info() entry of a name
  /// wins, as efBuildAddStr returns the same index).
  std::map<std::string, DeviceTypeInfo, std::less<>> types;
  /// EFDevTypes: the device type names in index order (the types of the
  /// `.ext` devices as read, then the style's); error messages name a type
  /// by its index.
  std::vector<std::string> type_names;
  /// A device_info() entry is named "None" (esNoModelType >= 0).
  bool has_none_type = false;
  /// The global node names of `$GND` in compat substrate mode (EFHNIsGND):
  /// `GND!` and the project's gnd name.
  std::vector<std::string> gnd_names{"GND!"};
};

/// Appends `type` to SpiceInput::type_names unless it is there.
void add_type_name(SpiceInput& input, const std::string& type);

/// esFetInfo from the current style's ExtGetDevInfo table.
void apply_device_info(SpiceInput& input, std::span<const extract::DeviceInfo> table);

/// The writer's input from a flattened netlist: nodes, devices, couplings
/// and resistors in the netlist's visit orders.
[[nodiscard]] SpiceInput from_netlist(const extflat::Netlist& netlist, std::string_view cell);

}  // namespace hefesto::netlist::spice
