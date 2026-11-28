// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Setup-file evaluation (openspec/changes/m9-lvs-simulation design.md D7,
// "Area contracts" LVSSETUP; capability lvs-setup-file). A setup file is
// Tcl evaluated in a fresh sandboxed interpreter (X-3) per run, with the
// `netgen::` command set and the setup commands; the result is the state
// the comparison reads.
//
// Where the setup writes (design "As landed (LVSALG, LVSRED, LVSPROP; M9
// R2)", "Setup inputs"): per class into the two circuits' netlist
// databases (PropertyDef::merge and ::pin, Cell::permutes, CellFlags::
// no_parallel and ::series_combinable, the device class, instances removed
// by `ignore`, nets made global by `global`, ports added by `convert`,
// instances flattened by `flatten`), and the global settings into
// SetupState (parallel_open for ReduceOptions, strict_topology and
// equated_classes for CompareOptions, the report column widths, the
// flattening prohibitions and the ignore list of later reads).
//
// Netlist numbers. Each circuit has its own NetlistDb; a setup file names a
// netlist by number (`{cell 0}`). Circuit 1's files keep their numbers;
// circuit 2's keep theirs unless one is also used by circuit 1, in which
// case all of circuit 2's files are numbered after circuit 1's highest.
//
// Owner: LVSSETUP.

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/cancel.hpp"
#include "hefesto/lvs/net/circuit.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/lvs/read/source.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::lvs {

/// The default step budget of one setup evaluation: Tcl commands, counted
/// by the interpreter (`info cmdcount`). A setting may lower it and never
/// raise it past twice this value (design D18, Q-LVS-36 pattern).
inline constexpr std::uint64_t kSetupStepBudget = 50'000'000;

/// The wall-time backstop of one setup evaluation. Tcl counts only the
/// commands it invokes, so a compiled loop with no command in its body
/// (`while {1} {}`) never reaches the step budget; this bound stops it. A
/// setting may lower it and never raise it past twice this value.
inline constexpr std::chrono::milliseconds kSetupTimeBudget{60'000};

/// The largest setup file read (a crafted-input limit, both modes).
inline constexpr std::uint64_t kMaxSetupFileBytes = std::uint64_t{16} << 20U;  // 16 MiB

/// One line printed while the setup ran: `err` lines are netgen's stderr
/// (the `Error <file>:<line> (ignoring), ...` lines), the rest its stdout.
struct SetupMessage {
  enum class Stream : std::uint8_t { out, err };
  Stream stream = Stream::out;
  std::string text;

  friend bool operator==(const SetupMessage&, const SetupMessage&) = default;
};

/// A cell of one compared circuit (1 or 2), by name.
struct SetupCellRef {
  int circuit = 1;
  std::string cell;

  friend bool operator==(const SetupCellRef&, const SetupCellRef&) = default;
};

/// One `equate classes` declaration. `first` and `second` are the class
/// names as resolved; `identity` is the fresh class identity `-unique` gave
/// the first cell (Q-LVS-29: drawn from the seeded generator), else empty;
/// `wildcard` is set when either side named every netlist, so the pair
/// applies while the compare queue is built (lvs-hierarchy).
struct EquatedClass {
  SetupCellRef first;
  SetupCellRef second;
  std::string identity;
  bool wildcard = false;

  friend bool operator==(const EquatedClass&, const EquatedClass&) = default;
};

/// What the setup commands recorded. The word lists keep each successful
/// command's words in evaluation order (the R0 contract); the typed fields
/// below them are what the comparison reads.
struct SetupState {
  std::vector<std::vector<std::string>> permutes;
  std::vector<std::vector<std::string>> properties;
  std::vector<std::vector<std::string>> equates;
  std::vector<std::vector<std::string>> ignores;
  std::vector<std::vector<std::string>> models;
  std::vector<std::vector<std::string>> globals;
  std::vector<std::vector<std::string>> flatten;

  // --- M9 R3 (LVSSETUP): global settings ---------------------------------
  /// `property parallel open` (true, the default) or `connected`
  /// (ReduceOptions::parallel_open).
  bool parallel_open = true;
  /// `property parallel none` is in force: cells defined later are
  /// excluded from parallel combination too.
  bool parallel_none = false;
  /// `property topology strict` (CompareOptions::strict_topology).
  bool strict_topology = false;
  /// `model blackbox on` (ReadOptions::auto_blackbox of later reads).
  bool auto_blackbox = false;
  /// The `equate classes` pairs as (circuit-1 class, circuit-2 class), for
  /// CompareOptions::equated_classes.
  std::vector<std::pair<std::string, std::string>> equated_classes;
  /// Every `equate classes` declaration, in order.
  std::vector<EquatedClass> equated;
  /// `flatten prohibit`: cells hierarchical comparison never flattens.
  std::vector<SetupCellRef> no_flatten;
  /// `ignore class|shorted`: per circuit, the classes later reads drop.
  std::vector<std::pair<int, IgnoredClass>> ignored;
  /// The report column widths of `format` (lvs-reports "Two-column layout").
  int column_left = 41;
  int column_right = 41;

  // --- M9 R3 (LVSSETUP): the evaluation ----------------------------------
  /// Everything the evaluation printed, in order.
  std::vector<SetupMessage> log;
  /// Setup lines that failed and were ignored.
  std::size_t errors = 0;
  /// Tcl commands the evaluation ran (the step budget's count).
  std::uint64_t steps = 0;
  /// Whether a setup file was evaluated (false: the trivial default setup
  /// or `nosetup`).
  bool evaluated = false;
  /// The LVS-SETUP-* warnings of the evaluation (one per ignored error).
  std::vector<diag::Diagnostic> diagnostics;
};

struct SetupContext {
  Circuit* first = nullptr;   // circuit 1 (required)
  Circuit* second = nullptr;  // circuit 2 (required)
  /// The sandbox of the fresh interpreter: a read-only root over the PDK
  /// store and the document folder, and the step budget (design D7). A
  /// zero `limits.command_limit` takes `step_budget`.
  script::SandboxPolicy sandbox;
  CancelToken cancel;
  // --- M9 R3 (LVSSETUP), additive ----------------------------------------
  /// Reads a setup file named by path; nullptr: only in-memory sources.
  platform::FileSystem* files = nullptr;
  /// The project setting netgen-compat (design D5).
  bool netgen_compat = true;
  /// The seed of the deterministic generator (Q-LVS-4, Q-LVS-29).
  std::uint64_t seed = 0;
  /// Tcl commands one evaluation may run (at most 2 × kSetupStepBudget).
  std::uint64_t step_budget = kSetupStepBudget;
  /// Wall time of the whole evaluation (at most 2 × kSetupTimeBudget).
  std::chrono::milliseconds time_budget = kSetupTimeBudget;
};

/// Evaluates `setup` against the two circuits ("Setup file evaluation").
/// An empty name with no bytes is the trivial default setup; a name that
/// contains `nosetup` and does not exist continues without a setup.
/// Fails with LVS-SETUP-002 (missing file), -003 (unreadable), -004 (step
/// or time budget), -005 (cancelled), -007 (file too large), -008 (bad context).
/// A failing setup line does not fail the evaluation.
[[nodiscard]] diag::Outcome<SetupState> evaluate_setup(const Source& setup, SetupContext& context);

/// Applies `permute default` and `property default` to both circuits, as
/// the trivial default setup does.
void apply_default_setup(SetupContext& context, SetupState& state);

}  // namespace hefesto::lvs
