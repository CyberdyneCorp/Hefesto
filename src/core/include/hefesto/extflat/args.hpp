// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFArgs: the run arguments shared by ext2spice, ext2sim, extcheck and
// antennacheck (capability ext-flattening-hierarchy "Common arguments";
// openspec/changes/m7-extraction/design.md D13): `-c -C -r -R -p -s -S -t
// -T -v -o` and the root cell name. Each netlister parses its own flags
// through `extra`, which is called with a flag it does not know and the
// remaining words. `-o` applies to one run (Q-NETLIST-42, fix).
//
// Scaffold state (tasks.md 3.3): parse_common_args() reports
// CORE-NOTIMPL-008 until EXTFLAT lands it.
//
// Owner: INTEGRATION (this header); EXTFLAT (the implementation).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::extflat {

/// The parsed common arguments.
struct CommonArgs {
  std::optional<std::string> cell;      // the root cell (a trailing `.ext` removed)
  std::optional<double> cap_threshold;  // `-c` (fF); infinite = no cap output
  bool cap_infinite = false;            // `-C`
  std::optional<std::int64_t> resist_threshold;  // `-r` (ohms)
  bool resist_infinite = false;                  // `-R`
  std::vector<std::string> search_path;          // `-p`
  std::vector<std::string> symbols;              // `-s sym=value`
  std::optional<std::string> symbol_file;        // `-S`
  std::string trim_chars;                        // `-t`
  std::optional<std::string> tech;               // `-T`
  bool verbose = false;                          // `-v`
  std::optional<std::string> output;             // `-o`, this run only
};

/// Called for a flag EFArgs does not know: the flag, and the index of the
/// next word in `args`; returns the index after the words it consumed, or
/// nullopt when the flag is not recognised either ("Unrecognized flag").
using ExtraFlags = std::function<std::optional<std::size_t>(std::span<const std::string> args, std::size_t flag)>;

[[nodiscard]] diag::Outcome<CommonArgs> parse_common_args(std::span<const std::string> args,
                                                          const ExtraFlags& extra);

}  // namespace hefesto::extflat
