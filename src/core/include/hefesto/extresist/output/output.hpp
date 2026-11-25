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
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/extresist/network/network.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extresist {

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
/// One overlay device record (ResPrintExtDev) with the device's terminal
/// nets renamed by `names` (an empty name keeps the input net's name);
/// `default_substrate` is the style's substrate node name for the model.
[[nodiscard]] std::string format_device(const InputModel& input, const InputDevice& device,
                                        const std::vector<std::string>& names, std::string_view default_substrate,
                                        const compat::CompatSettings& compat);

/// A device terminal on a processed net and the network node it lands on.
struct TerminalNode {
  std::size_t device = 0;  // InputModel::devices index
  Terminal terminal = Terminal::gate;
  std::size_t node = 0;  // Network::nodes index
};

/// One processed net that is output: its InputModel index, its reduced
/// network, its device terminals in gate-sorted order, and its lumped
/// resistance for `.res.lump`.
struct OutputNet {
  std::size_t net = 0;
  Network network;
  std::vector<TerminalNode> terminals;
  std::int32_t lump_ohms = 0;
};

/// "Naming of network nodes": names the unnamed nodes of `net` (terminal
/// nodes `<net'>.t<k>` in `terminals` order, then `<net'>.n<k>` in node
/// order) and returns whether the original net is killed (no node keeps
/// its exact name).
[[nodiscard]] bool name_nodes(std::string_view net, Network& network, const std::vector<TerminalNode>& terminals);

/// One net block of the overlay (killnode, rnode and resist lines) of a
/// network already named; `node_cap` is the net's `node` record capacitance.
[[nodiscard]] std::string net_block(std::string_view net, const Network& network, bool kill, double node_cap,
                                    const extract::ExtStyle& style);

/// The `.res.ext` text: the scale line, the net blocks (naming each
/// network), then the device records of the devices whose terminals were
/// renamed, in reverse read order (Q-NETLIST-39 per mode).
[[nodiscard]] diag::Outcome<std::string> write_res_ext(const InputModel& input, std::vector<OutputNet>& nets,
                                                       const extract::ExtStyle& style,
                                                       const compat::CompatSettings& compat);
/// The `.res.lump` text: `R <net> <ohms>` per output net.
[[nodiscard]] std::string write_res_lump(const InputModel& input, const std::vector<OutputNet>& nets);
[[nodiscard]] diag::Outcome<std::string> write_fasthenry(const InputModel& input, const std::vector<OutputNet>& nets,
                                                         double frequency, const compat::CompatSettings& compat);
/// ResCreateCenterlines: one element per resistor, named from its end
/// nodes (an unnamed end takes the next id of `next_id`, at every visit, as
/// Magic does). The network does not mark contact resistors yet, so none
/// is skipped.
[[nodiscard]] std::vector<GeometryElement> geometry_elements(const Network& net, std::int32_t& next_id);

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
