// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extresist network construction and reduction (capability
// extresist-network; openspec/changes/m7-extraction/design.md D5, D16):
// a net's geometry is yanked into a scratch cell, fractured with the
// restricted stitch editor (db/plane_stitch.hpp, design D6), turned into a
// resistor network with devices and breakpoints, and reduced (delta-star,
// series and parallel merges, Elmore pruning, simplification). HeapState
// is the path-resistance heap kept across nets with magic-compat ON
// (Q-NETLIST-46).
//
// As landed (M7 R4, tasks.md 13.1-13.6): the scaffold's declarations stay
// source-compatible. The fields and overloads marked "Added in M7 R4" carry
// what Magic's ResExtractNet and ResDoSimplify need besides the scaffold's
// arguments (the technology, the driver, the ports, the magic-compat
// setting) and what the output writers need afterwards (node kinds and
// path resistances, per-node link order, layout devices, Tdi, maxres, the
// messages Magic prints). The scaffold's 4-argument yank_net and
// 3-argument build_network have no technology and fail with
// EXTRESIST-NET-001; callers use the overloads below.
//
// Owner: INTEGRATION (this header); RESNET (the implementation).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::db {
class StitchEditor;  // db/plane_stitch.hpp (tasks.md 3.4)
}
namespace hefesto::mask {
class OutputStyle;
}
namespace hefesto::tech {
class Technology;
}

namespace hefesto::extresist {

/// Added in M7 R4: "no node" in NetDevice::terminals.
inline constexpr std::size_t kNoNode = std::numeric_limits<std::size_t>::max();
/// Added in M7 R4: RES_INFINITY, the path resistance of a node the origin
/// has not reached.
inline constexpr double kResInfinity = 0x3FFFFFFF;

/// Added in M7 R4: a drive or sink point (ResConnect) of a node of the
/// input. Magic attaches the points of every node of the cell that touch
/// the copied net (ResMakeDriverSinkPorts). A sink point of zero width or
/// height is grown by one on each side, as Magic does.
struct NetPort {
  std::string name;
  db::Rect rect;
  tech::TileType type = 0;
  bool sink = false;
};

/// A net's geometry in a scratch cell (never registered in the cell table).
struct NetGeometry {
  std::string net;
  std::unique_ptr<db::CellDef> scratch;
  db::Point origin;
  /// Added in M7 R4: the cell the net was yanked from (devices are found in
  /// it), and the labels copied with the net (SEL_DO_LABELS) in the scratch
  /// cell's coordinates, in copy order.
  const db::CellDef* source = nullptr;
  std::vector<db::Label> labels;
};

/// Added in M7 R4: resNode rn_why bits (NetNode::kind).
enum NodeKind : std::uint32_t {
  kNodeJunction = 0x01,
  kNodeDevice = 0x02,
  kNodeContact = 0x04,
  kNodeOrigin = 0x08,
  kNodeSink = 0x10,
  kNodeTriangle = 0x20,  // the star centre of a delta-to-star conversion
};

/// Added in M7 R4: rn_status and rr_status bits with Magic's values
/// (resis/resis.h), so that reduce() of an exported network resumes
/// exactly. RES_DEADEND shares RES_MAXTDI's bit: one is a resistor bit, the
/// other a node bit.
enum StatusBit : std::uint32_t {
  kStatusTrue = 0x00000001,       // RES_TRUE: the node is ready
  kStatusFinished = 0x00000004,   // RES_FINISHED
  kStatusMarked = 0x00000100,     // RES_MARKED (nodes and resistors)
  kStatusEW = 0x00000200,         // RES_EW (resistors)
  kStatusNS = 0x00000400,         // RES_NS (resistors)
  kStatusDiagonal = 0x00000800,   // RES_DIAGONAL (resistors)
  kStatusMaxTdi = 0x00001000,     // RES_MAXTDI (nodes)
  kStatusDeadEnd = 0x00001000,    // RES_DEADEND (resistors)
  kStatusDoneOnce = 0x00002000,   // RES_DONE_ONCE (nodes and resistors)
  kStatusTdiIgnore = 0x00010000,  // RES_TDI_IGNORE (resistors)
  kStatusHeap = 0x00200000,       // RES_HEAP (resistors)
};

/// A node of the network (resNode).
struct NetNode {
  db::Point location;
  std::string name;
  double cap = 0.0;  // attofarads
  /// Added in M7 R4: rn_why (NodeKind bits), rn_status (StatusBit bits),
  /// the path resistance from the origin in milliohms (rn_noderes; whole
  /// milliohms with magic-compat ON, Q-NETLIST-25), whether `name` is set,
  /// and the node's resistor and device links in Magic's list order
  /// (indices into Network::resistors and Network::devices). Before
  /// reduce() `cap` holds the node's area (rn_float), after it the
  /// distributed capacitance.
  std::uint32_t kind = 0;
  std::uint32_t status = 0;
  double path_resistance = kResInfinity;
  bool named = false;
  std::vector<std::size_t> resistors;
  std::vector<std::size_t> devices;
};

/// A resistor between two nodes (resResistor), in milliohms.
struct NetResistor {
  std::size_t a = 0;
  std::size_t b = 0;
  double resistance = 0.0;
  /// Added in M7 R4: rr_status (StatusBit bits), rr_area, the geometry
  /// fields (rr_cl: the centerline, or the via count in y of a contact;
  /// rr_width: the width, or the via count in x) and the type (rr_tt).
  /// `resistance` is a single-precision value, as in Magic.
  std::uint32_t status = 0;
  double area = 0.0;
  std::int32_t centerline = 0;
  std::int32_t width = 0;
  tech::TileType type = 0;
};

/// A device terminal attached to a node (resDevice).
struct NetDevice {
  std::string name;
  std::vector<std::size_t> terminals;
  /// Added in M7 R4. As an input device (DeviceAnnotations): the device
  /// line's model name, the lower-left point and tile type the `.ext` file
  /// records (RDev location, rs_ttype). As a layout device
  /// (Network::devices, ResDevList order): `terminals` are gate, substrate,
  /// then one per S/D class (kNoNode when unset), `type` is rd_devtype,
  /// `inside` the lower-leftmost device tile, the dimensions are rd_length,
  /// rd_width, rd_perim, rd_area and rd_tiles, and gate_cap and sd_cap are
  /// the device record's exts_deviceGateCap and exts_deviceSDCap.
  std::string model;
  db::Point location;
  tech::TileType type = 0;
  db::Rect inside;
  /// length and width are clamped into int32; perimeter and area are
  /// summed in 64 bits (a device tile can exceed 46341 units square).
  std::int32_t length = 0;
  std::int32_t width = 0;
  std::int64_t perimeter = 0;
  std::int64_t area = 0;
  std::int32_t tiles = 0;
  double gate_cap = 0.0;
  double sd_cap = 0.0;
};

/// The R-C network of one net.
struct Network {
  std::vector<NetNode> nodes;
  std::vector<NetResistor> resistors;
  std::vector<NetDevice> devices;
  std::vector<db::Point> breakpoints;
  /// Added in M7 R4. `nodes` are in the done-list order (ResNodeList, head
  /// first) and `resistors` in the resistor-list order (ResResList).
  /// origin: ResNodeAtOrigin. driver_source: the source terminal of the
  /// layout device at the driver (ResGetDevice), the origin of last resort.
  /// node_cap: rg_nodecap (the net capacitance in attofarads; reduce()
  /// replaces it by Cdown(origin) when it computes the delay).
  /// max_resistance and tdi: rg_maxres and rg_Tdi after reduce() (tdi -1:
  /// undefined, a loop). input_devices: the layout device at each input
  /// device's location (ResGetDevice), in DeviceAnnotations order.
  /// added_sinks: the label sink points ResMakeLabelPorts added (the input
  /// model keeps them, as Magic's node table does). messages: the lines
  /// Magic prints with TxError while building and reducing, in order.
  /// failed: ResExtractNet failed (messages say why); nothing else is set.
  std::optional<std::size_t> origin;
  std::optional<std::size_t> driver_source;
  double node_cap = 0.0;
  double max_resistance = 0.0;
  double tdi = 0.0;
  std::vector<std::optional<std::size_t>> input_devices;
  std::vector<NetPort> added_sinks;
  std::vector<std::string> messages;
  bool failed = false;
};

/// The devices on the net, from the `.ext` input model.
struct DeviceAnnotations {
  std::vector<NetDevice> devices;
};

/// Reduction tolerances (`minresist`, `mindelay`, `simplify`).
struct Tolerances {
  double min_resistance = 1000.0;  // milliohms
  double min_delay = 1.0e9;        // zeptoseconds (1 ps)
  bool simplify = true;
  /// Added in M7 R4: `lumped on` (ResOpt_DoLumpFile): the delay is computed
  /// even without simplification.
  bool lumped = false;
};

/// The path-resistance heap state (Q-NETLIST-25, Q-NETLIST-46).
struct HeapState {
  std::vector<std::int64_t> keys;
  /// Added in M7 R4: Magic's he_built, kept across the nets of a session
  /// with magic-compat ON (0 until a net first removed an entry).
  std::int32_t built = 0;
};

/// Added in M7 R4: what yank_net copies.
struct YankRequest {
  /// The start area (the node location, or the first drive point's
  /// rectangle, grown by 2) and the start types (DBConnectTbl of the node's
  /// type).
  db::Rect area;
  tech::TypeMask start_types;
  /// The `skip` types (Q-NETLIST-23): each is removed from every copy mask
  /// and its own mask is cleared (ResCopyMask).
  tech::TypeMask skip;
  bool magic_compat = true;
};

/// Added in M7 R4: the driver the net selection chose (extresist-command
/// "Net selection and first cutoff"): rg_devloc, rg_ttype and DRIVEONLY.
struct NetDriver {
  db::Point location;
  tech::TileType type = 0;
  bool drive_only = false;
};

/// Added in M7 R4: everything build_network needs besides the geometry,
/// the style and the devices.
struct BuildInput {
  const tech::Technology* technology = nullptr;
  /// The CIF output style (CIFCurStyle) for the contact cut sizes; nullptr:
  /// no cut is known, and every contact joins its planes without resistance.
  const mask::OutputStyle* cif = nullptr;
  /// The start point (the node location, or the first drive point's
  /// centre) and the driver.
  db::Point start;
  NetDriver driver;
  /// The drive and sink points, in the order Magic attaches them.
  std::vector<NetPort> ports;
  /// The drive points of each node (by name), for ResMakeLabelPorts' check
  /// that a label already has a drive point.
  std::function<bool(const std::string& node, const db::Rect& label)> has_drive_point;
  /// Names of the input nodes flagged PORTNODE (a merged node takes a port
  /// node's name).
  std::set<std::string, std::less<>> port_nodes;
  /// rg_nodecap, copied to Network::node_cap.
  double node_cap = 0.0;
  /// The once-per-session flag of the undersized-contact warning
  /// (ResDoContacts' static too_small); nullptr: warn for every net.
  bool* small_contact_warned = nullptr;
  /// `extresist geometry` or `fasthenry`: no construction-time reductions.
  bool geometry = false;
  bool magic_compat = true;
};

/// Yanks `net` (the net through `at` on `type`) into a scratch cell, minus
/// the `skip` types. The scaffold form has no technology and fails with
/// EXTRESIST-NET-001; use the overload below.
[[nodiscard]] diag::Outcome<NetGeometry> yank_net(const db::CellDef& cell, db::Point at, tech::TileType type,
                                                  const tech::TypeMask& skip);
/// Added in M7 R4: DBTreeCopyConnect of the net into a scratch cell, with
/// its labels ("Net geometry collection" step 1).
[[nodiscard]] diag::Outcome<NetGeometry> yank_net(const db::CellDef& cell, const tech::Technology& technology,
                                                  const YankRequest& request);
/// ResFracture over `area` of the scratch plane the editor wraps, with
/// magic-compat ON.
[[nodiscard]] diag::Status fracture(db::StitchEditor& editor, const db::Rect& area);
/// Added in M7 R4: ResFracture with the magic-compat setting
/// (Q-NETLIST-53: ON cuts a split tile as ResSplitX does, the left part
/// keeping its diagonal and the new part getting the left type, and joins
/// split tiles; OFF never cuts nor joins a split tile).
[[nodiscard]] diag::Status fracture(db::StitchEditor& editor, const db::Rect& area, bool magic_compat);
/// The scaffold form has no technology and fails with EXTRESIST-NET-001.
[[nodiscard]] diag::Outcome<Network> build_network(const NetGeometry& geometry, const extract::ExtStyle& style,
                                                   const DeviceAnnotations& devices);
/// Added in M7 R4: ResExtractNet after the copy: devices, contacts,
/// fracturing, ports, tile processing, resistors and the construction-time
/// reductions. The scratch cell is consumed (its planes are re-tiled). A
/// net Magic cannot extract gives Network::failed.
[[nodiscard]] diag::Outcome<Network> build_network(const NetGeometry& geometry, const extract::ExtStyle& style,
                                                   const DeviceAnnotations& devices, const BuildInput& input);
/// ResDoSimplify: path resistance (Q-NETLIST-25, Q-NETLIST-46),
/// capacitance distribution, Elmore delay and pruning, simplification and
/// the final scrunch (Q-NETLIST-26, Q-NETLIST-47).
[[nodiscard]] diag::Status reduce(Network& network, const Tolerances& tolerances, HeapState& heap,
                                  const compat::CompatSettings& compat);

}  // namespace hefesto::extresist
