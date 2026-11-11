// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK command and variable inventory and the command-trace hook
// (openspec/changes/m4-scripting/design.md D7, tasks.md 6.8; capability
// scripting-pdk-integration "PDK command and variable inventory", golden
// SCRIPT-PDK-09). The inventory is the acceptance list for running the
// verified PDKs unchanged; the trace records which engine commands and
// framework procedures a script really invokes, so a run of each PDK's rc
// file and generators can be compared with the table.
//
// Owner: PDKINT (design D2).

#pragma once

#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::script {

/// One column of the inventory table.
struct PdkInventory {
  std::string_view family;  // "sky130", "gf180mcu", "sg13g2"
  std::vector<std::string_view> engine_commands;
  std::vector<std::string_view> framework_procedures;  // under `magic::`

  /// True when `name` (without `magic::`) is an engine command or a
  /// framework procedure of this column.
  [[nodiscard]] bool covers(std::string_view name) const;
};

/// The three verified columns, in the table's order.
[[nodiscard]] std::span<const PdkInventory> pdk_inventories();
/// The column of `family` ("sky130", "gf180mcu", "sg13g2"; also the ciel
/// family "ihp-sg13"), or nullptr.
[[nodiscard]] const PdkInventory* pdk_inventory(std::string_view family);

/// The names of the inventory that `interp` does not define, neither as
/// `::magic::<name>` nor globally.
[[nodiscard]] std::vector<std::string> missing_commands(const Interpreter& interp, const PdkInventory& inventory);

/// The traced names the inventory does not cover (Inventory coverage check).
[[nodiscard]] std::vector<std::string> uncovered_commands(const std::set<std::string>& traced,
                                                          const PdkInventory& inventory);

/// Records every invocation of the traced commands (Tcl `trace add
/// execution ... enter`) by name, without a `::` or `magic::` prefix.
class CommandTrace {
 public:
  /// Traces each of `names` that exists in `interp` (each name is tried as
  /// given and as `::magic::<name>`). The trace must outlive `interp`.
  [[nodiscard]] cmd::CommandResult install(Interpreter& interp, std::span<const std::string> names);

  [[nodiscard]] const std::set<std::string>& names() const noexcept { return names_; }
  void clear() noexcept { names_.clear(); }

 private:
  std::set<std::string> names_;
};

}  // namespace hefesto::script
