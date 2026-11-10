// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `drc` command (drc-commands; openspec/changes/m3-drc/design.md D3/D9),
// registered by cmd::register_all_commands().
//
// The option table is Magic's cmdDrcOption (commands/CmdCD.c CmdDrc), in
// Magic's order, wizard options with their leading `*`; each entry has the
// exact help line `drc help` prints. `drc [list|listall] <option> [args]`:
// a first word starting with `list` selects list mode (`listall` also
// list-all mode); the option is found by Magic's Lookup() over the table
// lines (lookup_option); a missing option (also after `list`, Q-DRC-22),
// an unknown or an ambiguous one prints its message and the help; extra
// arguments to an option that takes none print the wrong-arguments text.
// List-mode results are cmd::Value lists with Magic's Tcl structure (the
// CLI prints them in Tcl list syntax).
//
// (Magic ref: commands/CmdCD.c:4015-4610 CmdDrc; utils/lookup.c Lookup)
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <span>
#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::drc {

/// One row of cmdDrcOption.
struct DrcOption {
  std::string_view name;  // "*flatcheck", "catchup", ...
  std::string_view help;  // the full table line, e.g. "catchup                run checker and wait for it to complete"
  bool wizard = false;    // a `*` option (listed only by `drc help wizard`)
  /// Accepts arguments after the option word (printrules, find, *showint,
  /// help, euclidean, *stepsize, *halo, count, style, ignore).
  bool takes_arguments = false;
};

/// Magic's option table in order: *flatcheck, *halo, *showint, *stepsize,
/// catchup, check, count, euclidean, find, help, ignore, off, on, status,
/// style, printrules, rulestats, statistics, why.
[[nodiscard]] std::span<const DrcOption> drc_options() noexcept;

/// Magic's Lookup(word, cmdDrcOption): the index of the option `word`
/// names (letters compared case-insensitively, each table line up to its
/// first space; an exact name wins, otherwise a unique prefix; a leading
/// `::magic::` or `magic::` is skipped), -1 when ambiguous, -2 when none.
[[nodiscard]] int lookup_option(std::string_view word) noexcept;

/// Adds `drc` to `registry`. Returns false if the name was taken.
bool register_drc_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::drc
