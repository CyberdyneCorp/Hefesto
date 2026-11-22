// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The run flags of `ext2spice` (capability netlist-spice-options "Run
// arguments and output file"): spcParseArgs, called by EFArgs
// (extflat::parse_common_args) for each flag it does not know. Flags that
// are Magic globals change the session settings and persist (`-M`, `-m`,
// `-f`, `-B`, `-d`, `-J`); `-F` and `-o` apply to one run (Q-NETLIST-42,
// fix).
//
// Owner: SPICE.

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "hefesto/netlist/spice/settings.hpp"
#include "hefesto/netlist/spice/writer.hpp"

namespace hefesto::netlist::spice {

/// The state one run's flags change.
struct RunArgs {
  SpiceSettings& settings;          // the session's settings
  RunFlags flags;                   // -F
  std::optional<std::string> output;  // -o, this run only
  std::vector<WriteMessage> messages;  // the text spcParseArgs prints
};

/// spcParseArgs for `args[flag]`: returns the index after the words it
/// consumed, or nullopt for a usage error (the message is in `run`).
[[nodiscard]] std::optional<std::size_t> parse_spice_flag(std::span<const std::string> args, std::size_t flag,
                                                          RunArgs& run);

}  // namespace hefesto::netlist::spice
