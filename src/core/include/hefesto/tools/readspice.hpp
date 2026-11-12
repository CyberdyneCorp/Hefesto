// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// readspice (openspec/changes/m4-scripting/design.md D8/D11; capability
// readspice "Netlist file resolution", "Line preprocessing", "Subcircuit
// recognition", "Port renumbering algorithm", "Port direction from
// PININFO"; Q-SCRIPT-19, Q-SCRIPT-32): annotates loaded cells with the port
// order and direction of a SPICE, CDL or Spectre netlist.
//
// A native port of the Tcl procedure: it issues the same engine commands
// (`cellname list exists`, `load`, `box values`, `port ...`, `select`,
// `what -list`, `goto`, `suspendall`/`resumeall`) as nested commands of the
// running `readspice`, so it is one logged command and one undo step.
// Its `port last` follows Q-SCRIPT-6 through the `port` command.
//
// (Magic ref: tcltk/readspice.tcl:16-254)
//
// Owner: TOOLS (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tools/spice_lines.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::tools {

struct ReadSpiceOptions {
  /// Q-SCRIPT-32: Magic's continuation spacing (magic-compat ON) or the
  /// fixed one (OFF). The command sets it from the project's magic-compat.
  ContinuationMode continuation = ContinuationMode::magic;
  /// Q-SCRIPT-47: Magic's `string map {[ < ] >]}` (magic-compat ON) turns
  /// `D[0]` into `D<0>]`, so it never finds port `D<0>`; OFF maps `]` to `>`.
  bool magic_bus_brackets = true;
};

/// A netlist found by "Netlist file resolution".
struct NetlistFile {
  std::string name;  // as readspice prints it: the argument or the candidate tried
  std::string text;
  bool cdl = false;
};

/// `file rootname`: `name` without the extension of its last element.
[[nodiscard]] std::string tcl_file_rootname(std::string_view name);

/// "Netlist file resolution": `name`, then `<root>.spi`, `.spc`, `.spice`,
/// `.sp`, `.ckt`, `.cdl`. A relative name is read under `cwd` (the session's
/// working directory) when it is not empty. nullopt when none can be read.
[[nodiscard]] std::optional<NetlistFile> resolve_netlist(platform::FileSystem& files, const platform::Path& cwd,
                                                         std::string_view name);

/// Runs readspice on `netlist` in the editor session of `ctx`. The value is
/// `1` when no candidate file can be read, otherwise empty (the procedure's
/// result); the messages are TOOLS-RSP-* diagnostics emitted through `ctx`.
[[nodiscard]] cmd::CommandResult readspice(cmd::CommandContext& ctx, std::string_view netlist,
                                           const ReadSpiceOptions& options);

}  // namespace hefesto::tools
