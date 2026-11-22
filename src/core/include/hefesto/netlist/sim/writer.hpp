// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.sim`, `.al` and `.nodes` writer of `ext2sim` (capability
// netlist-sim "File structure and order", "Device lines", "Transistor
// attributes", "Parasitic and node records", "Name conversion state";
// openspec/changes/m7-extraction/design.md D15).
//
// SimInput is the flattened circuit as ext2sim's visitors see it: the
// devices, coupling capacitors, resistors and nodes in Magic's visit orders
// (EFVisitDevs, EFVisitCaps, EFVisitResists, EFVisitNodes), with the values
// each visitor receives. It is filled from extflat::Netlist by
// from_netlist() (netlist/sim/sim.hpp), and built by hand in unit tests.
// write_sim() is the port of simdevVisit, simcapVisit, simresistVisit,
// simnodeVisit and the merge pre-pass (simmergeVisit); it touches no file
// and no session state other than the name-conversion flags it is given.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2sim/ext2sim.c, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: SIM (design D3).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extflat/names.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/netlist/sim/sim.hpp"

namespace hefesto::netlist::sim {

/// A hierarchical name as its components from the root side (`x1`, `out`).
using HierName = std::vector<std::string>;

/// Magic's device classes (extract/extract.h DEV_*), as the `.ext` class
/// keyword names them.
enum class DeviceClass : std::uint8_t {
  fet,
  mosfet,
  asymmetric,
  bjt,
  res,      // devres
  cap,      // devcap
  caprev,   // devcaprev
  vsource,
  diode,
  pdiode,
  ndiode,
  subckt,
  rsubckt,
  msubckt,
  csubckt,
  dsubckt,
  veriloga,
};

/// The class of a `.ext` class keyword; nullopt for an unknown keyword.
[[nodiscard]] std::optional<DeviceClass> device_class(std::string_view keyword) noexcept;

/// One area and perimeter (EFPerimArea) of a node, per resistance class,
/// in the units of the `.ext` file.
struct AreaPerim {
  std::int64_t area = 0;
  std::int64_t perim = 0;

  friend bool operator==(const AreaPerim&, const AreaPerim&) = default;
};

/// One name of a node (EFNodeName) and whether it is a global name
/// (EFHNIsGlob).
struct NodeName {
  HierName name;
  bool global = false;

  friend bool operator==(const NodeName&, const NodeName&) = default;
};

/// One flat node as EFVisitNodes passes it to simnodeVisit.
struct SimNode {
  std::vector<NodeName> names;          // the canonical name first (efnode_name list order)
  double cap = 0.0;                     // aF, the value EFVisitNodes passes (0 for GND / the substrate)
  std::int64_t resist = 0;              // mΩ, EFNodeResist
  std::vector<std::string> attributes;  // efnode_attrs, in list order
  std::vector<AreaPerim> area_perim;    // efnode_pa, one per resistance class
  db::Point location;                   // efnode_loc lower-left
  std::string layer;                    // EFLayerNames[efnode_type]

  friend bool operator==(const SimNode&, const SimNode&) = default;
};

/// One terminal of a visited device (DevTerm).
struct SimTerminal {
  /// The flat node (index into SimInput::nodes) EFHNConcatLook finds;
  /// nullopt when the name does not resolve (written as ` GND`).
  std::optional<std::size_t> node;
  /// dterm_attrs, the comma-joined attribute text; empty for none.
  std::string attributes;
  /// The terminal's node in its own cell (dterm_node), for the
  /// hierarchical area/perimeter of simnAPHier: a key unique per cell
  /// definition and node, and that node's per-class area and perimeter.
  std::string local_node;
  std::vector<AreaPerim> local_area_perim;

  friend bool operator==(const SimTerminal&, const SimTerminal&) = default;
};

/// One device as EFVisitDevs passes it to simdevVisit.
struct SimDevice {
  DeviceClass device_class = DeviceClass::mosfet;
  std::string type;                    // EFDevTypes[dev_type]
  std::vector<SimTerminal> terminals;  // gate, source, drain, then the rest
  /// The substrate (dev_subsnode): absent when the device has none. The
  /// local name (EFHNToStr of its canonical name in its own cell), the
  /// flat node EFHNConcatLook finds (nullopt: `errGnd!`), a key that
  /// identifies the local node (the merge test compares it) and the local
  /// node's per-class area and perimeter (the bipolar L and W).
  struct Substrate {
    std::string local_name;
    std::optional<std::size_t> node;
    std::string local_node;
    std::vector<AreaPerim> local_area_perim;

    friend bool operator==(const Substrate&, const Substrate&) = default;
  };
  std::optional<Substrate> substrate;
  std::int64_t length = 0;     // EFGetLengthAndWidth
  std::int64_t width = 0;
  db::Point lower_left;        // the transformed dev_rect's lower-left corner
  double resistance = 0.0;     // dev_res (devres)
  double capacitance = 0.0;    // dev_cap (devcap, devcaprev)
  float scale = 1.0F;          // the visit's scale
  std::string prefix;          // the hierarchical prefix of the cell it came from (hc_hierName)

  friend bool operator==(const SimDevice&, const SimDevice&) = default;
};

/// One coupling capacitor (EFVisitCaps), in aF.
struct SimCap {
  HierName a;
  HierName b;
  double cap = 0.0;
};

/// One explicit resistor (EFVisitResists), in mΩ.
struct SimResist {
  HierName a;
  HierName b;
  float resistance = 0.0F;
};

/// The flattened circuit in visit order.
struct SimInput {
  float scale = 0.0F;     // EFScale (the header's units)
  std::string tech;       // EFTech
  std::vector<SimDevice> devices;
  std::vector<SimCap> caps;
  std::vector<SimResist> resists;
  std::vector<SimNode> nodes;
};

/// The options of one write: the run's settings after its flags, the
/// thresholds in force and whether caps and resistors were flattened.
struct WriteOptions {
  SimSettings settings;            // format, merge, attrs, alias, labels, ...
  float cap_threshold = 2.0F;      // EFCapThreshold (fF, an EFCapValue); kInfiniteCap for `-C`/`inf`
  std::int64_t resist_threshold = 10;  // EFResistThreshold (Ω); kInfiniteResist for `-R`/`inf`
  bool flat_caps = true;           // EF_FLATCAPS: write the coupling lines
  /// The extraction style's device table (ExtGetDevInfo), indexed by model
  /// name: the source, drain and substrate resistance classes, the default
  /// substrate name and the tile type's long name (for an n/p FET class
  /// letter). Empty when the `.ext` style does not match (EFStyle NULL).
  std::vector<extract::DeviceInfo> device_info;
  /// DBTypeLongNameTbl of each DeviceInfo::type (same order as device_info).
  std::vector<std::string> type_long_names;
};

/// INFINITE_THRESHOLD and INFINITE_THRESHOLD_F of extflat/EFtypes.h.
inline constexpr std::int64_t kInfiniteResist = 0x7fffffff;
inline constexpr float kInfiniteCap = 1.0E38F;

/// One console line of a write: TxPrintf (`error` false) or TxError.
struct ConsoleLine {
  bool error = false;
  std::string text;

  friend bool operator==(const ConsoleLine&, const ConsoleLine&) = default;
};

/// What one write produced: the `.sim` text, the `.al` and `.nodes` texts
/// (present only when alias / labels are on, Q-NETLIST-52) and the console
/// lines in the order Magic prints them (`Devices merged: <n>`, a FET type
/// that starts with neither n nor p, ...).
struct SimOutput {
  std::string sim;
  std::optional<std::string> alias;
  std::optional<std::string> labels;
  std::vector<ConsoleLine> console;
};

/// What a run reads: the root cell, the read options and the flatten flags
/// (EF_FLATNODES, plus EF_FLATCAPS and EF_FLATRESISTS as Q-NETLIST-18
/// decides).
struct ReadRequest {
  std::string cell;
  extflat::ReadOptions options;
  extflat::FlatFlags flags = extflat::FlatFlags::flat_nodes;
};

/// Reads and flattens a root cell into the visit data. The default source
/// is extflat::read, extflat::flatten and from_netlist().
using SimSource = std::function<diag::Outcome<SimInput>(const ReadRequest&)>;

/// run() with the given source instead of the `.ext` files (unit tests and
/// hosts that hold the flattened netlist already).
[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const SimRun& request, const SimSource& source);

/// The visit data of a flattened netlist. extflat::Netlist does not carry
/// every value ext2sim reads yet (node capacitance, lumped resistance,
/// attributes and per-class area/perimeter; device L, W and values; the
/// local terminal nodes; EFScale and EFTech): those are left at their
/// defaults until EXTFLAT exposes them (tasks.md 10.2). Names are split at
/// `/` into components.
[[nodiscard]] SimInput from_netlist(const extflat::Netlist& netlist);

/// simdevVisit, simcapVisit, simresistVisit and simnodeVisit over `input`,
/// after the merge pre-pass when merging is on. Names are converted with
/// extflat::name_to_sim and `flags`.
[[nodiscard]] SimOutput write_sim(const SimInput& input, const WriteOptions& options, extflat::NameFlags& flags,
                                  const compat::CompatSettings& compat);

}  // namespace hefesto::netlist::sim
