// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.ext` read-and-flatten subset that `def write` needs (capabilities
// ext-flattening-read, ext-flattening-hierarchy, ext-flattening-names;
// openspec/changes/m6-lef-def-oasis/design.md D4, D9, D10; Open Question 1,
// decided (a)): EFReadFile from the root cell down through its `use`
// records, then EFFlatBuild with EF_FLATNODES | EF_NOFLATSUBCKT, unscaled,
// with resistance unused. The result is the flat node list with each node's
// preferred hierarchical name, its layer and location, and the timestamp of
// every `.ext` file read (Q-LEFDEF-110 OFF). It is built to the M7 specs in
// M7's area layout, so M7 extends it rather than replacing it.
//
// `.ext` input is untrusted: the read is bounded by ReadLimits (design D21,
// D23 M6-S8), and a use cycle or a cap that is reached fails the read.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); EXTSUB
// fills in the implementation and the fields in src/core/src/extflat/
// (design D2, "As-landed scaffold contracts").

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extflat/records.hpp"

namespace hefesto::extflat {

/// Resource caps of one read (design D21). Magic has none: a use cycle
/// recurses until the stack overflows, and a huge array or a deep
/// hierarchy runs out of memory. Each cap fails the read with
/// EXTFLAT-READ-009 (native) when it is reached.
struct ReadLimits {
  std::uint64_t max_file_bytes = std::uint64_t{1} << 30;  // one `.ext` file
  std::size_t max_line_bytes = std::size_t{16} << 20;     // one logical line
  std::size_t max_cells = std::size_t{1} << 20;           // `.ext` files read
  std::size_t max_depth = 1000;                           // use nesting below the root
  std::uint64_t max_instances = std::uint64_t{1} << 22;   // flattened instances (array elements count)
  std::uint64_t max_flat_names = std::uint64_t{1} << 24;  // flat node names
  std::uint64_t max_connections = std::uint64_t{1} << 24; // merge-record elements (array subscripts expanded)
};

/// The options of one read.
struct ReadOptions {
  /// Record each `.ext` file's `timestamp` (Q-LEFDEF-110 OFF compares them
  /// with the layout's).
  bool timestamps = false;
  /// The session's magic-compat setting.
  bool magic_compat = true;
  /// The `-p` search path of the netlisters (EFSearchPath), unexpanded
  /// entries; empty when none was given (`def write` gives none).
  std::vector<std::string> search_path;
  /// The directory of the file a loaded cell was read from (DBCellLookDef's
  /// cd_file), searched second; nullopt when the cell is not loaded or has
  /// no file. Unset: no cell is loaded.
  std::function<std::optional<std::string>(std::string_view cell)> cell_directory;
  /// The current technology's name. A `tech` record naming another
  /// technology fails the read, as with layout loaded in Magic. Empty: no
  /// check.
  std::string tech_name;
  /// The project setting "global nets and power names" (`globalNets`,
  /// hefesto-settings; Magic's Tcl array `globals`): the leaf names that
  /// make a node global (ext-flattening-names "Global and ground
  /// detection").
  std::vector<std::string> global_nets;
  /// The current extraction style's units per lambda (exts_unitsPerLambda),
  /// against which the `scale` record's lscale is compared. Nullopt: the
  /// file's lscale is taken as the style's (no coordinate rescale).
  std::optional<float> units_per_lambda;
  /// The units per lambda of a style of the technology by name, nullopt
  /// when the technology has no such style (ExtCompareStyle). A `style`
  /// record naming a known style selects it for the rest of the read and
  /// is reported in Netlist::selected_style. Unset: every style is unknown.
  std::function<std::optional<float>(std::string_view style)> style_units;
  ReadLimits limits;
};

/// One saved node location (EFNode efnode_disjoint, EFSaveLocs): the
/// lower-left corner of a 1x1 rectangle and its layer. These are the
/// coordinates of the `.ext` file that defined the node, not root
/// coordinates (EFflat.c copies them untransformed).
struct NodeLocation {
  db::Point location;
  std::string layer;

  friend bool operator==(const NodeLocation&, const NodeLocation&) = default;
};

/// One flat node (EFNode after EFFlatBuild with EF_FLATNODES).
struct FlatNode {
  /// The preferred hierarchical name (ext-flattening-names "Name preference
  /// order"), with `/` separating the use path.
  std::string name;
  /// The node's layer as the `.ext` file names it (the `node` record's
  /// type); "space" for a node created without one (EFLayerNames[0]).
  std::string layer;
  /// The lower-left corner of the node's 1x1 location rectangle in root
  /// coordinates (efnode_loc: the `node` record's x, y transformed).
  db::Point location;
  /// True for the global substrate node (the `substrate` record;
  /// EF_GLOB_SUBS_NODE).
  bool substrate = false;
  /// EF_PORT: the node holds a `port` of some cell.
  bool port = false;
  /// EF_TOP_PORT: the node holds a `port` of the root cell.
  bool top_port = false;
  /// Every name of the node in list order (the first is `name`), each as
  /// its components from the root side (a use id may contain `/`).
  std::vector<std::vector<std::string>> names;
  /// The saved locations (efnode_disjoint) in list order.
  std::vector<NodeLocation> locations;
};

/// The timestamp of one `.ext` file read (its `timestamp` record).
struct FileStamp {
  std::string cell;
  std::int64_t timestamp = 0;
};

/// The flattened netlist of one root cell.
class Netlist {
 public:
  Netlist() = default;
  Netlist(std::vector<FlatNode> nodes, std::vector<FileStamp> stamps)
      : nodes_(std::move(nodes)), stamps_(std::move(stamps)) {}

  /// Calls `visitor` for each node in EFVisitNodes order (killed nodes are
  /// not in the list).
  void visit_nodes(const std::function<void(const FlatNode&)>& visitor) const {
    for (const FlatNode& node : nodes_) {
      visitor(node);
    }
  }

  /// The device, coupling-capacitor and resistor visits in Magic's orders
  /// (EFVisitDevs, EFVisitCaps, EFVisitResists; M7 design D5, D13). The
  /// M6 reader fills none of them; EXTFLAT does (M7 tasks.md 10.x).
  void visit_devices(const std::function<void(const FlatDevice&)>& visitor) const {
    for (const FlatDevice& device : devices) {
      visitor(device);
    }
  }
  void visit_caps(const std::function<void(const FlatCap&)>& visitor) const {
    for (const FlatCap& cap : caps) {
      visitor(cap);
    }
  }
  void visit_resists(const std::function<void(const FlatResist&)>& visitor) const {
    for (const FlatResist& resist : resists) {
      visitor(resist);
    }
  }

  [[nodiscard]] const std::vector<FlatNode>& nodes() const noexcept { return nodes_; }
  [[nodiscard]] const std::vector<FileStamp>& stamps() const noexcept { return stamps_; }

  /// The flat devices, coupling capacitors and resistors, in visit order
  /// (M7 design D13; filled by extflat::flatten, M7 EXTFLAT).
  std::vector<FlatDevice> devices;
  std::vector<FlatCap> caps;
  std::vector<FlatResist> resists;

  /// The style a `style` record selected (ExtCompareStyle loads it in
  /// Magic, a session side effect the caller applies), if any.
  std::optional<std::string> selected_style;

 private:
  std::vector<FlatNode> nodes_;    // EFVisitNodes order
  std::vector<FileStamp> stamps_;  // in read order
};

/// Reads `<root>.ext` and the `.ext` files of every cell it uses, found
/// through `ctx` as EFReadFile finds them, and flattens the nodes.
/// Diagnostics of the read go to `ctx.sink`. Fails (EXTFLAT-READ-001 and
/// the others of docs/diagnostics/codes/extflat.md) when EFReadFile would
/// return FALSE, when a use cycle is found, or when a cap is reached.
[[nodiscard]] diag::Outcome<Netlist> read_flat(cell::LookupContext& ctx, std::string_view root,
                                               const ReadOptions& options);

}  // namespace hefesto::extflat
