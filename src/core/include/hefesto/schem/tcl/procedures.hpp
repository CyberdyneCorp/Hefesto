// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The procedures xschem provides to `tcleval` values (capability
// schematic-symbols-and-libraries "tcleval values", table "Procedures that
// xschem provides"; Q-SCHEM-8), written from that table, never from
// xschem's Tcl. The `xschem` command itself is capability schematic-command's
// (CMD).
//
// Owner: SYMLIB.

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::schem {

/// The significant digits `ev7` prints (captured, SCHEM-NL-07).
inline constexpr int kEv7Digits = 7;

/// The significant digits `ev` and `ev0` print. Not documented publicly:
/// pinned by capture later (task 4.3); every pinned PDK use gives the same
/// text for any value from 4 to 7.
inline constexpr int kEvDigits = 7;

/// What the procedures need from the host.
struct ProcedureHooks {
  /// `abs_sym_path <ref>`: the absolute path a symbol reference resolves to
  /// ("Symbol references"); empty function: the reference unchanged.
  std::function<std::string(std::string_view reference)> abs_sym_path;
  /// `textwindow <text>`: shows `text` read-only; empty function: nothing.
  std::function<void(std::string_view text)> textwindow;
  /// `write_data <text> <file>` writes through this file system, which the
  /// caller restricts to the sandbox's granted folders (nullptr: refused).
  platform::FileSystem* files = nullptr;
  /// Receives the sandbox's refusal of a `write_data` (SCRIPT-SANDBOX-004),
  /// which a native command's error would otherwise lose (security review,
  /// task 12.1); empty function: dropped.
  std::function<void(const diag::Diagnostic&)> denied;
};

/// The names of the table "Procedures that xschem provides", in its order.
[[nodiscard]] const std::vector<std::string>& xschem_procedure_names();

/// Defines every procedure of xschem_procedure_names() in `interp`: `ev`,
/// `ev7`, `ev0`, `to_eng`, `from_eng`, `sim_is_ngspice` (1), `sim_is_xyce`
/// (0), `tabulate`, the three `ngspice::` procedures (empty until M9 loads
/// simulation results), `abs_sym_path`, `textwindow` and `write_data`.
void install_xschem_procedures(script::Interpreter& interp, ProcedureHooks hooks);

}  // namespace hefesto::schem
