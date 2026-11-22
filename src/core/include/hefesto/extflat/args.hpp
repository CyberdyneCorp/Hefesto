// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFArgs: the run arguments shared by ext2spice, ext2sim, extcheck and
// antennacheck (capability ext-flattening-hierarchy "Common netlister
// arguments"; openspec/changes/m7-extraction/design.md D13): `-c -C -r -R
// -p -s -S -t -T -v -o` (and the debugging `-n -N -z -h`) and the root cell
// name. Each netlister parses its own flags through `extra`, which is
// called with a flag it does not know and the remaining words. `-o`
// applies to one run (Q-NETLIST-42, fix): it lives in the CommonArgs of
// that run only.
//
// Owner: INTEGRATION (this header); EXTFLAT (the implementation).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::extflat {

/// The parsed common arguments.
struct CommonArgs {
  std::optional<std::string> cell;      // the root cell (a trailing `.ext` removed)
  std::optional<double> cap_threshold;  // `-c` (fF); infinite = no cap output
  bool cap_infinite = false;            // `-C`
  std::optional<std::int64_t> resist_threshold;  // `-r` (ohms)
  bool resist_infinite = false;                  // `-R`
  std::vector<std::string> search_path;          // `-p`, split at ':'
  std::vector<std::string> symbols;              // `-s sym=value`, the accepted ones
  std::optional<std::string> symbol_file;        // `-S`
  std::string trim_chars;                        // `-t`, every occurrence concatenated
  std::optional<std::string> tech;               // `-T`
  bool verbose = false;                          // `-v`
  std::optional<std::string> output;             // `-o`, this run only

  /// The messages EFArgs prints and goes on after (a second root cell, a
  /// rejected `-s` symbol), in order.
  std::vector<diag::Diagnostic> messages{};
};

/// Called for a flag EFArgs does not know: the words, and the index of the
/// flag in them; returns the index after the words it consumed (at least
/// `flag + 1`), or nullopt when the flag is not recognised either.
using ExtraFlags = std::function<std::optional<std::size_t>(std::span<const std::string> args, std::size_t flag)>;

/// EFArgs over the words after the command name. Fails with the usage text
/// (EXTFLAT-READ-015) for an unknown flag, a flag without its value, or
/// `-h`.
[[nodiscard]] diag::Outcome<CommonArgs> parse_common_args(std::span<const std::string> args,
                                                          const ExtraFlags& extra);

/// The usage text EFArgs prints (MAGIC_WRAPPER form).
[[nodiscard]] std::string_view common_args_usage() noexcept;

/// efSymAdd: adds `sym=value` (value an integer) to `symbols`; returns the
/// message Magic prints when it is rejected (no '=', a value that is not
/// an integer, or a symbol already defined).
[[nodiscard]] std::optional<std::string> add_symbol(std::map<std::string, std::int32_t, std::less<>>& symbols,
                                                    std::string_view assignment);

}  // namespace hefesto::extflat
