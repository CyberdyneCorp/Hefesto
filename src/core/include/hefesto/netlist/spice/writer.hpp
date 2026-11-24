// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SPICE text writer (capabilities netlist-spice-options,
// netlist-spice-devices; openspec/changes/m7-extraction/design.md D14):
// write_flat() is ext2spice's flat run from the header to the HSPICE
// dictionary over a SpiceInput; global_card() and instance_line() are the
// `.global` card and the subcircuit call of hierarchical output. Nothing
// here reads or writes the session: the caller passes the settings, the
// run flags and the name-conversion flags (Q-NETLIST-16: the caller keeps
// them across runs with magic-compat ON).
//
// Owner: SPICE.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/extflat/names.hpp"
#include "hefesto/netlist/spice/input.hpp"
#include "hefesto/netlist/spice/settings.hpp"

namespace hefesto::netlist::spice {

/// The flags of one run that do not persist (spcParseArgs).
struct RunFlags {
  bool device_nodes_only = false;  // -F (esDevNodesOnly)
};

/// The SPICE name conversion of a canonical name (EFHNSprintf with the
/// run's flags; extflat::name_to_spice by default).
using NameConverter = std::function<std::string(std::span<const std::string> components, Dialect format,
                                                extflat::NameFlags& flags)>;

/// What a write produced besides the text: the informative and warning
/// lines Magic prints while writing, in order.
struct WriteMessage {
  bool warning = false;
  std::string text;
};

struct WriteResult {
  std::string text;
  std::vector<WriteMessage> messages;
};

/// The flat netlist of `input` (ext2spice.c CmdExtToSpice from the header
/// on, flat branch). `names` are updated with the run's flags as Magic
/// updates EFOutputFlags.
[[nodiscard]] WriteResult write_flat(const SpiceInput& input, const SpiceSettings& settings, const RunFlags& flags,
                                     const compat::CompatSettings& compat, extflat::NameFlags& names,
                                     const NameConverter& convert = {});

/// The project's power names for the CDL `.global` card (`$SUB`, `$VDD`,
/// `$GND`).
struct PowerNames {
  std::optional<std::string> sub;
  std::optional<std::string> vdd;
  std::optional<std::string> gnd;
};

/// The `.global` card of hierarchical output, with its empty line; empty
/// when no name qualifies.
[[nodiscard]] std::string global_card(std::span<const extract::DeviceInfo> table, Dialect format,
                                      const PowerNames& power, const extflat::NameFlags& names);

/// One subcircuit call (subcktVisit).
struct InstanceCall {
  std::vector<std::string> path;  // the instance path (its last element is the use id); empty: no id
  std::vector<std::string> nodes; // the SPICE names of the ports, in port order
  std::string cell;               // the definition's name
  std::vector<std::string> params;  // the cell's `parameters :<cell>` entries
};

/// The call line with its newline; `counter` is the subcircuit counter,
/// advanced when the instance is numbered.
[[nodiscard]] std::string instance_line(const InstanceCall& call, Dialect format, bool renumber, int& counter,
                                        const extflat::NameFlags& names, const NameConverter& convert);

/// One subcircuit call of a hierarchical section (subcktHierVisit), for
/// one use or array element.
struct HierCall {
  std::string cell;                       // the used definition
  std::vector<std::string> path;          // the use id with the element's subscript (`r[0]`, `a2[1,0]`)
  std::shared_ptr<const std::vector<extflat::PortEntry>> ports;  // the definition's port entries, shared by a use's elements
  bool devices_absent = false;            // DEF_NODEVICES: absorbed, no call unless a stub
  bool stub = false;                      // abstract or primitive with blackbox on
};

/// One section of hierarchical output: a definition's one-level netlist
/// (`input.cell` is its name, `input.ports` its port entries), whether it
/// is written as a black-box stub, whether it opens with a `.subckt` line
/// (else `* Top level circuit`), and its calls in use order.
struct HierSection {
  SpiceInput input;
  bool stub = false;
  bool wrapped = true;
  std::vector<HierCall> calls;
};

/// What the hierarchical file starts with: the header of `root`, and the
/// `.global` card of the style's substrate names (`table`) and the power
/// names.
struct HierHead {
  SpiceInput root;
  std::vector<extract::DeviceInfo> table;
  PowerNames power;
};

/// The hierarchical netlist (ext2spice.c CmdExtToSpice with `hierarchy
/// on`): the header and `.global` card, then the sections in order
/// (children first, the root last).
[[nodiscard]] WriteResult write_hier(const HierHead& head, const std::vector<HierSection>& sections,
                                     const SpiceSettings& settings, const RunFlags& flags,
                                     const compat::CompatSettings& compat, extflat::NameFlags& names,
                                     const NameConverter& convert = {});

/// `name` with an `x` prefix when it does not start with a letter.
[[nodiscard]] std::string subcircuit_name(std::string_view name);

}  // namespace hefesto::netlist::spice
