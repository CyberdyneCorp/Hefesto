// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The setup-file runtime (capability lvs-setup-file; design D7): the state
// one evaluation shares between the setup commands, the netlist numbering
// over the two circuits' databases, the cell reference syntax ("Cell
// reference syntax") and the command handlers. Private to lvs/setup.
//
// Owner: LVSSETUP.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::lvs::setup {

/// The words of a command after its name.
using Words = std::vector<std::string>;

/// One netlist (file) a setup command can name.
struct Netlist {
  Circuit* circuit = nullptr;
  int side = 1;               // 1 or 2
  FileNumber local = 0;       // the number in the circuit's database
  FileNumber number = 0;      // the number setup commands use
  std::string name;           // the file's name (its file cell)
  CellId top = 0;             // LoadedFile::top
};

/// A resolved cell: one cell of one circuit.
struct Target {
  Circuit* circuit = nullptr;
  int side = 1;
  CellId cell = 0;
  FileNumber number = 0;  // the netlist number (Netlist::number)

  [[nodiscard]] Cell& get() const { return circuit->db.cells.at(cell); }
};

/// The result of a cell reference: one target, or one per netlist that has
/// the cell when the file is a wildcard.
struct Resolved {
  std::vector<Target> targets;
  bool wildcard = false;
};

class Runtime {
 public:
  Runtime(SetupContext& context, SetupState& state);

  [[nodiscard]] SetupContext& context() noexcept { return context_; }
  [[nodiscard]] SetupState& state() noexcept { return state_; }
  [[nodiscard]] const std::vector<Netlist>& netlists() const noexcept { return netlists_; }
  /// The distinct circuits (one when both sides are the same object).
  [[nodiscard]] std::vector<Circuit*> circuits() const;
  /// The circuit of side 1 or 2 when it has a top cell, else nullptr.
  [[nodiscard]] Circuit* circuit(int side) const;
  /// The netlist holding `cell` of `circuit`.
  [[nodiscard]] const Netlist* netlist_of(const Circuit& circuit, CellId cell) const;

  /// Resolves a `valid_cellname` argument; the error is the netgen message.
  [[nodiscard]] diag::Outcome<Resolved> resolve(std::string_view reference) const;

  /// The next value of the deterministic generator (SplitMix64, seeded
  /// with SetupContext::seed; Q-LVS-4, Q-LVS-29).
  [[nodiscard]] std::uint64_t next_random();

  void print(std::string text);
  void print_error(std::string text);
  /// Polls cancellation; true when the run should stop (the interpreter is
  /// then asked to interrupt the evaluation).
  bool cancelled();
  void set_interpreter(script::Interpreter* interpreter) noexcept { interpreter_ = interpreter; }
  /// Stops the evaluation because its time budget ran out.
  void time_out();
  [[nodiscard]] bool timed_out() const noexcept { return timed_out_; }

 private:
  [[nodiscard]] diag::Outcome<Resolved> resolve_one(std::string_view word) const;
  [[nodiscard]] diag::Outcome<Resolved> resolve_pair(std::string_view a, std::string_view b) const;
  [[nodiscard]] diag::Outcome<Target> circuit_top(std::string_view keyword) const;
  [[nodiscard]] diag::Outcome<Resolved> in_netlist(const Netlist& netlist, std::string_view cell) const;
  [[nodiscard]] Resolved everywhere(std::string_view cell) const;
  [[nodiscard]] const Netlist* netlist_by_number(FileNumber number) const;
  [[nodiscard]] const Netlist* netlist_by_file_name(std::string_view name) const;
  [[nodiscard]] const Netlist* netlist_by_top_cell(std::string_view name) const;
  void number_netlists();

  SetupContext& context_;
  SetupState& state_;
  std::vector<Netlist> netlists_;
  std::uint64_t rng_ = 0;
  script::Interpreter* interpreter_ = nullptr;
  bool timed_out_ = false;
};

// --- words.cpp: shared helpers ----------------------------------------------

/// Tcl_GetIndexFromObj's rule: the exact keyword, else the only keyword the
/// word is a prefix of; nullopt when none or several.
[[nodiscard]] std::optional<std::size_t> match_keyword(std::string_view word,
                                                       std::span<const std::string_view> keywords);
/// `bad option "<w>": must be a, b, or c` (Tcl's text for a failed match).
[[nodiscard]] std::string bad_option(std::string_view kind, std::string_view word,
                                     std::span<const std::string_view> keywords);
/// A failed command whose Tcl error is `text` (LVS-SETUP-006).
[[nodiscard]] cmd::CommandResult command_error(std::string text);
/// `wrong # args: should be "<usage>"`.
[[nodiscard]] cmd::CommandResult wrong_args(std::string_view usage);
/// A decimal integer (Tcl_GetInt's accepted forms: optional sign, digits).
[[nodiscard]] std::optional<std::int64_t> parse_integer(std::string_view text) noexcept;
/// A real number (Tcl_GetDouble: the whole text is a number).
[[nodiscard]] std::optional<double> parse_real(std::string_view text) noexcept;
/// The elements of a Tcl list.
[[nodiscard]] diag::Outcome<std::vector<std::string>> split_list(std::string_view text);
/// A Tcl list of `elements`.
[[nodiscard]] std::string make_list(std::span<const std::string> elements);
/// Tcl `string match` of `pattern` against `text` (`*`, `?`, `[...]`, `\`).
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view text) noexcept;

// --- class groups (permute.cpp) ---------------------------------------------

[[nodiscard]] bool is_transistor(DeviceClass device_class) noexcept;
[[nodiscard]] bool is_resistor(DeviceClass device_class) noexcept;
[[nodiscard]] bool is_capacitor(DeviceClass device_class) noexcept;

/// Which pairs `permute <keyword>` declares.
struct PermuteGroups {
  bool transistors = false;
  bool resistors = false;
  bool inductors = false;
  bool capacitors = false;
};
void apply_permute_groups(Runtime& runtime, PermuteGroups groups);
void apply_property_default(Runtime& runtime);

// --- the commands (one file each) -------------------------------------------

[[nodiscard]] cmd::CommandResult permute_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult property_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult equate_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult ignore_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult model_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult global_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult convert_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult flatten_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult format_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult cells_command(Runtime& runtime, const Words& words);
[[nodiscard]] cmd::CommandResult ports_command(Runtime& runtime, const Words& words);
/// nodes, elements, instances, contents, describe, leaves, log, readnet,
/// readlib, writenet, canonical, help.
[[nodiscard]] cmd::CommandResult diagnostic_command(Runtime& runtime, std::string_view name, const Words& words);
/// The names diagnostic_command answers.
[[nodiscard]] std::span<const std::string_view> diagnostic_command_names() noexcept;

// --- property helpers shared by property.cpp and property_merge.cpp ---------

/// The property of interest `key` of `cell`, or nullptr.
[[nodiscard]] PropertyDef* find_definition(Cell& cell, std::string_view key) noexcept;
/// `No property <k> found for device <c>`.
void report_missing_property(Runtime& runtime, const Cell& cell, std::string_view key);
/// `property <c> series|serial|parallel ...`.
[[nodiscard]] cmd::CommandResult combine_option(Runtime& runtime, const Resolved& cells, bool series,
                                                std::span<const std::string> args);
/// `property <c> merge {key type} ...` (deprecated).
[[nodiscard]] cmd::CommandResult merge_option(Runtime& runtime, const Resolved& cells,
                                              std::span<const std::string> args);
/// `property parallel|series none|all|connected|open` (global forms).
[[nodiscard]] cmd::CommandResult global_combine(Runtime& runtime, bool series, std::string_view word);

// --- flattening (flatten.cpp), shared with `flatten class` ------------------

/// Flattens instance `index` of `parent` (lvs-hierarchy "Flattening
/// semantics"); the instance's class must be a subcircuit.
void flatten_instance(NetlistDb& db, CellId parent, std::size_t index);
/// Whether instances of `klass` may be flattened (subcircuit, not a
/// placeholder, not prohibited).
[[nodiscard]] bool flattenable(Runtime& runtime, const Target& klass);

}  // namespace hefesto::lvs::setup
