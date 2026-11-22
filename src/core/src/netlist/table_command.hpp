// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the dispatcher shape shared by ext2spice, ext2sim and
// antennacheck (openspec/changes/m7-extraction/design.md D4): the
// first-word rule (netlist/first_word.hpp), `help` printing each table
// entry as `    <entry>` (Magic's usage loop), the run path, and
// CORE-NOTIMPL-008 for every option the scaffold has not landed.
//
// Owner: INTEGRATION.

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"

namespace hefesto::netlist::detail {

struct TableCommand {
  std::span<const std::string_view> table;  // entry 0 is `[run] [options]`
  std::size_t help = 0;                     // the index of `help`
  /// The run path, with the run arguments.
  std::function<cmd::CommandResult(cmd::CommandContext&, std::vector<std::string>)> run;
  /// The options already implemented: called with the option index; returns
  /// nullopt for an option it does not handle (CORE-NOTIMPL-008). May be
  /// empty.
  std::function<std::optional<cmd::CommandResult>(cmd::CommandContext&, const cmd::Invocation&, std::size_t)> option;
};

/// The option name of a table entry: its text up to the first blank.
[[nodiscard]] std::string option_name(std::string_view entry);

/// Prints every entry of `table` as Magic's usage loop does.
void print_table(cmd::CommandContext& ctx, std::span<const std::string_view> table);

[[nodiscard]] cmd::CommandResult dispatch(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                                          const TableCommand& command);

}  // namespace hefesto::netlist::detail
