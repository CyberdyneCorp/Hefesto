// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The first-word rule of `ext2spice`, `ext2sim` and `antennacheck`
// (capability netlist-spice-options "Command options", Q-NETLIST-13,
// resolution: compat; openspec/changes/m7-extraction/design.md D4). Their
// option tables start with the help entry `[run] [options]`, which Magic's
// Lookup never matches with `run`, and Magic treats any first word that is
// not an option (unknown, ambiguous or `run`) as a run argument without
// consuming it, so it becomes the cell name.
//
// With magic-compat ON Hefesto copies that. With it OFF the first entry
// matches `run` like any other keyword (so `run` and its unique prefixes
// select the run and are consumed), an ambiguous word prints `Ambiguous
// option: <w>` and does nothing, and an unknown word is still a run
// argument (the `<command> <cell>` form). ext2sim and antennacheck have the
// same table shape and follow the same rule (the standing compat rule; no
// Q-row names them yet, see the M7 SCAFFOLD report).
//
// Owner: INTEGRATION (shared by SPICE, SIM and ANTENNA).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::netlist {

/// What the first word of a call selects.
enum class FirstWord : std::uint8_t {
  run,        // run the netlister with the remaining arguments
  option,     // the option at `index` (never the run entry)
  ambiguous,  // magic-compat OFF only: print "Ambiguous option: <w>" and stop
};

struct FirstWordResult {
  FirstWord kind = FirstWord::run;
  std::size_t index = 0;               // the option, when kind == option
  std::vector<std::string> run_args;   // the run arguments, when kind == run
};

/// Classifies `args` against `table`, whose entry 0 is the `[run]` help
/// entry.
[[nodiscard]] FirstWordResult classify_first_word(std::span<const std::string> args,
                                                  std::span<const std::string_view> table, bool magic_compat);

}  // namespace hefesto::netlist
