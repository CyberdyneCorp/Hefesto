// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extresist input model and output (capabilities extresist-command "Input
// model", extresist-output; openspec/changes/m7-extraction/design.md D16):
// ResReadExt's input model over EXTFLAT's tokenizer, and the `.res.ext`,
// `.res.lump`, FastHenry and geometry writers. The three files are written
// through extract::AtomicOutput (Q-EXTRACT-27).
//
// The input model keeps what ResReadExt keeps: the nets in order of first
// mention (ResOriginalNodes is built by prepending, so nets are processed
// in reverse), their lumped resistance and capacitance, the status flags,
// drive and sink points, and the device records with their terminal
// nets. The decisions applied here:
// - Q-NETLIST-29 (compat): an `attr` record applies to the most recently
//   read node record with magic-compat ON, to the named node with it OFF;
//   in both modes an `attr` before any node record is ignored with a
//   warning, and `res:min=` values of any length are accepted.
// - Q-NETLIST-39 (compat): resistor and capacitor device records of the
//   overlay carry no value fields with magic-compat ON, the extractor's
//   fields with it OFF (format_device()).
//
// Owner: RESCMD (this header and the implementation).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/extresist/network/network.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::extresist {

struct FastHenryIds;

/// A drive or sink point (ResConnect): its rectangle and tile type
/// (nullopt when the record names no known type).
struct ConnectPoint {
  db::Rect rect;
  std::optional<tech::TileType> type;

  friend bool operator==(const ConnectPoint&, const ConnectPoint&) = default;
};

/// The terminal by which a device touches a net (GATE, SOURCE, DRAIN, SUBS).
enum class Terminal : std::uint8_t { gate, source, drain, subs };

/// One device on a net (devPtr): the device's index in InputModel::devices
/// and the terminal.
struct NodeDevice {
  std::size_t device = 0;
  Terminal terminal = Terminal::gate;

  friend bool operator==(const NodeDevice&, const NodeDevice&) = default;
};

/// The status flags of a net (ResExtNode status).
struct NodeStatus {
  bool node_record = false;    // NODERECORD: a `node` or `substrate` record
  bool skip = false;           // SKIP: `res:skip`
  bool force = false;          // FORCE: `res:force`, a port or a connection
  bool minsize = false;        // MINSIZE: `res:min=`
  bool drive_located = false;  // DRIVELOC
  bool port = false;           // PORTNODE
  /// Added in M7 R5: REDUNDANT, set when a FastHenry node list names the
  /// net (ResPrintFHNodes); the net is not processed afterwards.
  bool redundant = false;

  friend bool operator==(const NodeStatus&, const NodeStatus&) = default;
};

/// One net of the `.ext` input model (ResExtNode).
struct InputNode {
  std::string name;
  /// The `node` record's location; Magic's (INFINITY, INFINITY) until one is read.
  std::optional<db::Point> location;
  std::string type;                       // the record's layer name
  std::optional<tech::TileType> tile_type;  // nullopt: an unknown layer
  double resistance = 0.0;  // lumped, milliohms (+inf for a negative record value)
  double cap = 0.0;         // attofarads: the node's own and, in signal mode, its couplings
  double node_cap = 0.0;    // the `node` record's capacitance alone
  double coupling = 0.0;    // cap_couple outside signal mode
  double min_resistance = 0.0;  // `res:min=`
  NodeStatus status;
  std::vector<ConnectPoint> drive_points;  // newest first (Magic prepends)
  std::vector<ConnectPoint> sink_points;   // newest first
  std::vector<NodeDevice> devices;         // newest first
};

/// Added in M7 R5: the layout device a device record was matched to
/// (RDev layout): rd_inside's lower-left corner, rd_length, rd_width,
/// rd_perim and rd_area, which the overlay's device records print.
struct LayoutRecord {
  db::Point location;
  std::int32_t length = 0;
  std::int32_t width = 0;
  std::int64_t perimeter = 0;
  std::int64_t area = 0;

  friend bool operator==(const LayoutRecord&, const LayoutRecord&) = default;
};

/// One device record (RDev).
struct InputDevice {
  std::string device_class;  // the `device` record's class ("fet" for a `fet` record)
  std::string model;
  db::Point location;
  /// The fields between the rectangle and the terminals, verbatim (L W, a
  /// value, parameters): the extractor's leading fields.
  std::vector<std::string> leading;
  std::optional<std::size_t> gate;
  std::optional<std::size_t> source;
  std::optional<std::size_t> drain;
  std::optional<std::size_t> subs;
  std::string gate_attr = "0";
  std::string source_attr = "0";
  std::string drain_attr = "0";
  std::int32_t length = 0;  // the gate terminal's length
  std::int32_t width = 0;   // the larger source/drain terminal length
  float wl = 0.0F;          // rs_wl: W/L, 0 when L is 0
  std::optional<tech::TileType> tile_type;  // extGetDevType of the model
  /// The class of the style's device line for the model (exts_deviceClass),
  /// nullopt when the style has none.
  std::optional<extract::DeviceClass> style_class;
  /// exts_deviceSDCount of that line (2 without one).
  std::int32_t sd_count = 2;
  /// Added in M7 R5: ResFixUpConnections' doPermute (more than one S/D
  /// terminal, the second class empty or equal to the first).
  bool permute_sd = false;
  /// Added in M7 R5: RDev status TRUE (an output net fixed up one of its
  /// terminals, so the overlay rewrites the record) and the layout record
  /// of the first net that did.
  bool touched = false;
  std::optional<LayoutRecord> layout;
};

/// ResReadExt's view of a cell's `.ext` file.
struct InputModel {
  std::string cell;
  std::vector<InputNode> nodes;      // in order of first mention
  std::vector<InputDevice> devices;  // in read order

  /// The nets in processing order (ResOriginalNodes: the reverse of first mention).
  [[nodiscard]] std::vector<std::size_t> net_order() const;
  /// The net named `name`, if any.
  [[nodiscard]] std::optional<std::size_t> find(std::string_view name) const;
  /// The devices on net `node` as RESNET's annotations (terminals are net indices).
  [[nodiscard]] DeviceAnnotations annotations(std::size_t node) const;
};

/// What the read needs.
struct InputOptions {
  const tech::Technology* tech = nullptr;
  const extract::ExtStyle* style = nullptr;
  /// ResOpt_Signal: a run (not `extresist box`); couplings add to both
  /// nets' capacitance and `res:drive` adds drive points.
  bool signal = true;
  bool debug = false;  // `extresist debug`: the "Added ..." lines
  bool magic_compat = true;
  /// The text of `<cell>.ext` as ExtFileOpen finds it; nullopt when it cannot be opened.
  std::function<std::optional<std::string>(std::string_view cell)> load;
  /// The parent cells whose `.ext` files are read for `connect` records into this cell.
  std::vector<std::string> parents;
  /// The console (TxPrintf and TxError lines); unset: silent.
  std::function<void(diag::Diagnostic)> emit;
};

/// A centerline segment of the geometry output (a DBWElementAddLine line
/// element: its name `N<a>_<b>`, its canonical rectangle's corners).
struct GeometryElement {
  db::Point from;
  db::Point to;
  std::int32_t width = 0;
  std::string layer;
  std::string name;
};

/// ResReadExt and ResReadParentExt. Fails with EXTRESIST-OUT-001 when the
/// cell's `.ext` file cannot be opened.
[[nodiscard]] diag::Outcome<InputModel> read_input(std::string_view cell, const InputOptions& options);

/// The scale line of the overlay: `scale <resistScale> <capScale> <%g unitsPerLambda>`.
[[nodiscard]] std::string res_ext_scale(const extract::ExtStyle& style);
/// The lump line `R <net> <ohms>`.
[[nodiscard]] std::string lump_line(std::string_view net, std::int32_t ohms);
/// One overlay device record (ResPrintExtDev): the class, the model, the
/// layout record's corner and dimensions (the input record's when the
/// device has none), the current terminal nets (renamed by
/// fix_up_connections) and the attributes; `default_substrate` is the
/// style's substrate node name for the model.
[[nodiscard]] std::string format_device(const InputModel& input, const InputDevice& device,
                                        std::string_view default_substrate, const compat::CompatSettings& compat);

/// Added in M7 R5: ResFixUpConnections over the devices of an output net
/// (extresist-output "Naming of network nodes"). Each device of `net` (in
/// the net's device order, sorted by ResSortByGate) that has a layout
/// device (Network::input_devices) gets the network node of each of its
/// terminals on the net named `<net'>.t<k>` (k from 0, the counter going
/// back one step when the node already has a name, as Magic's resNodeNum
/// does), and its terminal moved to the input net of that name, which is
/// created when missing. A symmetric device (InputDevice::permute_sd) whose
/// source and drain are both on the net shares one node between them; a
/// layout device whose two S/D nodes differ while only one ext terminal is
/// on the net has them merged first. A terminal with no layout node prints
/// `Missing <role> connection of device at (x y) on net <net'>` and marks
/// the net DONTKILL. Returns DONTKILL; the messages are appended.
[[nodiscard]] bool fix_up_connections(InputModel& input, std::size_t net, Network& network,
                                      const compat::CompatSettings& compat, std::vector<std::string>& messages);

/// One net block of the overlay (ResPrintExtNode and ResPrintExtRes): the
/// `killnode` line unless a network node keeps the net's exact name or the
/// net is DONTKILL, an `rnode` line per network node (unnamed nodes named
/// `<net'>.n<k>` in node order; the node keeping the net, or with an
/// orphan patch the first node, has the net's own capacitance subtracted),
/// the orphan patch `resist "<net>" "<first node>" 0.0` with its warning
/// when a DONTKILL net would have been killed, then the `resist` lines.
[[nodiscard]] std::string net_block(const InputNode& node, Network& network, bool dont_kill,
                                    const extract::ExtStyle& style, std::vector<std::string>& messages);

/// One processed net that is output: its InputModel index, its reduced
/// network, its overlay block (net_block) and its lumped resistance.
struct OutputNet {
  std::size_t net = 0;
  Network network;
  std::string block;
  std::int32_t lump_ohms = 0;
};

/// The `.res.ext` text: the scale line, the net blocks, then the device
/// records of the touched devices in reverse read order (ResRDevList;
/// Q-NETLIST-39 per mode).
[[nodiscard]] diag::Outcome<std::string> write_res_ext(const InputModel& input, const std::vector<OutputNet>& nets,
                                                       const extract::ExtStyle& style,
                                                       const compat::CompatSettings& compat);
/// The `.res.lump` text: `R <net> <ohms>` per output net.
[[nodiscard]] std::string write_res_lump(const InputModel& input, const std::vector<OutputNet>& nets);
/// ResCreateCenterlines: one element per non-contact resistor (contacts
/// are skipped when `technology` is given), named from its end nodes (an
/// unnamed end takes the next id of `next_id`, at every visit, as Magic
/// does), its rectangle the canonical one between the two node locations.
[[nodiscard]] std::vector<GeometryElement> geometry_elements(const Network& net, std::int32_t& next_id,
                                                             const tech::Technology* technology = nullptr);

/// Added in M7 R5: what the FastHenry and geometry writers read besides the
/// network: the contact types and planes, the heights, thicknesses and
/// plane order of the extraction style, and the CIF output style's scale
/// and contact cuts (nullptr: none, a cut of size 0).
struct GeometryContext {
  const tech::Technology* technology = nullptr;
  const extract::ExtStyle* style = nullptr;
  const mask::OutputStyle* cif = nullptr;
  bool magic_compat = true;
};

/// Added in M7 R5: ResAlignNodes. A node at either end of an east-west
/// (north-south) resistor moves to the resistor's centerline y (x); moving
/// a node twice prints `Warning: contention over node Y position` (X).
/// Diagonal resistors are skipped. With magic-compat ON the last resistor
/// of the list is skipped too, as Magic's loop stops before it; with it OFF
/// every resistor is aligned (Q-NETLIST-74).
void align_nodes(Network& net, bool magic_compat, std::vector<std::string>& messages);

/// Added in M7 R5: one net's node list of the FastHenry file
/// (ResPrintFHNodes up to `.external`): `N<name>` (or `N<id>` from
/// `ids.node` for an unnamed node) with x, y and z in microns, z the
/// height of the node's last non-contact resistor's type (with
/// magic-compat OFF its first: Q-NETLIST-73), or 0.1 times its
/// plane order when the type has no height; a node that touches only
/// contacts takes the contact's upper residue's height (Q-NETLIST-27) and
/// a node with no resistor height 0. A node on a contact with more than
/// one cut lists each cut as `N<name>_<i>_<j>` and an `.equiv` line.
/// `node_ids` receives each node's id (-1 when named) for the segments.
[[nodiscard]] std::string fasthenry_nodes(const Network& net, const GeometryContext& ctx, FastHenryIds& ids,
                                          std::vector<std::int32_t>& node_ids);
/// Added in M7 R5: the segments of one net (ResPrintFHRects): `E<k>`
/// between the two end nodes with w and h in microns (a contact: its cut
/// edge, one segment per cut when it has several; a cut the CIF style does
/// not define has edge 0 in both modes, an uninitialised read in Magic).
[[nodiscard]] std::string fasthenry_segments(const Network& net, const GeometryContext& ctx, FastHenryIds& ids,
                                             const std::vector<std::int32_t>& node_ids);
/// Added in M7 R5: the file's end: `.freq`, the comment lines of the port
/// order (`* <port index> <text>` per port label) and `.end`.
[[nodiscard]] std::string fasthenry_trailer(double frequency, const std::vector<std::pair<std::uint32_t, std::string>>& ports);

/// The FastHenry header and reference plane (ResPrintReference) of a cell
/// named `cell` with bounding box `bbox`, at `oscale` microns per unit.
[[nodiscard]] std::string fasthenry_reference(std::string_view cell, const db::Rect& bbox, float oscale);

/// The FastHenry node and segment counters (Q-NETLIST-40, compat): they
/// restart for each net with magic-compat ON and run across the file with
/// it OFF.
struct FastHenryIds {
  std::int32_t node = 1;
  std::int32_t segment = 1;

  /// The start of a net's block.
  void next_net(bool magic_compat) noexcept {
    if (magic_compat) {
      node = 1;
      segment = 1;
    }
  }
};

}  // namespace hefesto::extresist
