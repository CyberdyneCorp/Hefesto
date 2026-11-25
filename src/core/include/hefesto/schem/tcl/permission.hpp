// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The script permission model of schematics (capability
// schematic-symbols-and-libraries "Script execution permission";
// Q-SCHEM-25, `Resolution: platform`): `tcleval` values always run in the
// sandbox (TclBridge), and a launcher's `tclcommand` runs only after the
// user confirms it once per document, in the same sandbox. xschem's
// `xschem_execute_scripts` has no effect (SCHEM-LIB-007).
//
// Owner: SYMLIB.

#pragma once

#include <functional>
#include <set>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::schem {

/// Asks the user whether a launcher's `tclcommand` may run; true confirms.
using ConfirmLauncher = std::function<bool(std::string_view tclcommand)>;

/// Which documents the user has confirmed launchers for in this session.
class LauncherPermission {
 public:
  /// True when launchers of `document` may run: confirmed earlier in the
  /// session, or confirmed now by `ask` (called only when not yet
  /// confirmed; a refusal is not remembered, so the next Ctrl+click asks
  /// again). An empty `ask` refuses.
  [[nodiscard]] bool allow(DocumentId document, std::string_view tclcommand, const ConfirmLauncher& ask);

  [[nodiscard]] bool confirmed(DocumentId document) const { return confirmed_.count(document) != 0; }

  /// The document was closed: its confirmation ends.
  void forget(DocumentId document) { confirmed_.erase(document); }

 private:
  std::set<DocumentId> confirmed_;
};

/// Runs a launcher's `tclcommand` for `document` in `sandbox` (the `tcleval`
/// interpreter) when `permission` allows it; otherwise fails with
/// SCHEM-TCL-002 without evaluating anything.
///
/// capability schematic-simulation-integration "Simulation launchers"
/// (SIMINT): while the script runs, `xschem <sub>` reaches the interpreter's
/// `xschem` command (schem/cmd: `xschem netlist` is `schematic netlist`) and
/// `simulate` is `schematic simulate`; a command that is not supported (or
/// not available until M9, CORE-NOTIMPL) is skipped with warning
/// SCHEM-SIM-003 and the rest of the script runs; changes to the `sim`
/// array (xschem's simulator commands) are undone afterwards, because the
/// simulator is embedded.
[[nodiscard]] cmd::CommandResult run_launcher(script::Interpreter& sandbox, LauncherPermission& permission,
                                              DocumentId document, std::string_view tclcommand,
                                              const ConfirmLauncher& ask);

}  // namespace hefesto::schem
