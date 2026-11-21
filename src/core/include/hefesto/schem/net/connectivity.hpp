// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Connectivity and net naming of one schematic (openspec/changes/m8-schematic
// design.md D9; capability schematic-netlisting "Connection geometry", "Net
// naming order", "Unnamed nets", "Pass-through and shorted symbols", "Bus
// taps", "Global nets").
//
// The input is a schematic already reduced to what connectivity needs: the
// non-ignored wires and instances with their transformed pin points and
// their final `lab` values (after parent substitution and `expr`). Building
// it from a Document and its symbols is the netlister's job; this stage is
// pure and does not read properties.
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

// Pin positions use the "Orientation transform" of FORMAT,
// hefesto/schem/file/transform.hpp.

/// The connection point of a pin box: its centre.
[[nodiscard]] Point pin_point(const BoxRecord& box) noexcept;

/// "Connection geometry": whether `p` touches the segment `a`-`b` (the
/// endpoints in either order).
[[nodiscard]] bool touches(Point p, Point a, Point b) noexcept;

struct NetWire {
  Point p1;
  Point p2;
  bool ignored = false;  // spice_ignore (or lvs_ignore) true or open
};

struct NetPin {
  std::string name;   // the pin's `name`, after parent substitution
  Point at;           // transformed connection point
  std::size_t width = 1;  // bus width of `name`
};

struct NetInstance {
  std::string name;  // for messages
  std::string type;  // the symbol type
  bool ignored = false;  // not netlisted and not connected
  bool shorted = false;  // spice_ignore=short (or lvs_ignore=short)
  /// `lab` of a label, port or bus tap; nullopt when the instance has none.
  std::optional<std::string> lab;
  bool global = false;  // `global` true or `ground` (labels only)
  std::size_t vector_width = 1;  // bus width of the instance name
  std::vector<NetPin> pins;      // in symbol pin (file) order
};

struct SchematicNets {
  std::vector<NetWire> wires;          // wire order
  std::vector<NetInstance> instances;  // instance order
};

/// One short ("Net naming order").
struct NetShort {
  std::string tag;       // `Net`, `Bus tap`, ...
  std::string existing;  // the kept name
  std::string incoming;  // the name that lost
};

struct NetNaming {
  /// Net of each wire (unnamed nets keep their `#`); empty for ignored wires.
  std::vector<std::string> wire_nets;
  /// Net of each pin, per instance; empty for ignored instances.
  std::vector<std::vector<std::string>> pin_nets;
  /// Multiplicity of each automatic net ("Unnamed nets").
  std::map<std::string, std::size_t> multiplicity;
  /// Global net bits declared by labels, in first-seen order.
  std::vector<std::string> globals;
  std::vector<NetShort> shorts;
  /// SCHEM-NET-001 for each short, and SCHEM-NET-002 for global names that
  /// are not valid bus notation.
  std::vector<diag::Diagnostic> diagnostics;
  /// `lab` values to store (Q-SCHEM-29): the instance index and its new
  /// `lab`, for labels and ports that had none.
  std::vector<std::pair<std::size_t, std::string>> label_labs;

  /// A short was found: the run fails ("Errors and warnings").
  [[nodiscard]] bool failed() const noexcept { return !shorts.empty(); }
};

/// Names the nets of one schematic ("Net naming order").
[[nodiscard]] NetNaming name_nets(const SchematicNets& schematic);

/// Whether `type` names a net from its `lab` (label and port types).
[[nodiscard]] bool is_naming_type(std::string_view type) noexcept;

/// The comma-separated netlist text of `net` ("Unnamed nets", "Bus and
/// vector notation"): the `#` of an automatic name removed, an automatic
/// net of multiplicity m > 1 expanded as `net<N>_[m-1..0]`, any other name
/// bus-expanded, and `[`/`]` replaced by `bus_replacement` when it has two
/// characters.
[[nodiscard]] std::string net_text(std::string_view net, const NetNaming& naming, std::string_view bus_replacement);

}  // namespace hefesto::schem
