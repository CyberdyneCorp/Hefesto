// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFArgs: the run arguments shared by ext2spice, ext2sim, extcheck and
// antennacheck (capability ext-flattening-hierarchy "Common netlister
// arguments"; openspec/changes/m7-extraction/design.md D13): `-c -C -r -R
// -p -s -S -t -T -v -o` (and the debugging `-n -N -z -h`) and the root cell
// name. Each netlister parses its own flags through `extra`, which is
// called with a flag it does not know and the remaining words, and also
// for `-h` before the usage text (Magic's argsProc). `-o` applies to one
// run (Q-NETLIST-42, fix): it lives in the CommonArgs of that run only,
// and only a tool whose own flags have it (ext2spice, ext2sim) accepts
// it. `-T` is accepted by the standalone programs only (Magic compiles it
// out of the Tcl build). `-s` and `-S` build the symbol table of the run
// in argument order, `-S` reading its file through ArgOptions::read_file.
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

namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::extflat {

/// efSymHash: the `-s`/`-S` symbols and their integer values.
using SymbolTable = std::map<std::string, std::int32_t, std::less<>>;

/// The parsed common arguments.
struct CommonArgs {
  std::optional<std::string> cell;      // the root cell (a trailing `.ext` removed)
  std::optional<double> cap_threshold;  // `-c` (fF); infinite = no cap output
  bool cap_infinite = false;            // `-C`
  std::optional<std::int64_t> resist_threshold;  // `-r` (ohms)
  bool resist_infinite = false;                  // `-R`
  std::vector<std::string> search_path;          // `-p`, split at ':'
  /// The `-s sym=value` and `-S symfile` symbols (efSymHash), in argument
  /// order; a rejected assignment is reported and skipped.
  SymbolTable symbols;
  std::optional<std::string> symbol_file;        // the last `-S`
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

/// What a tool adds to EFArgs.
struct ArgOptions {
  /// `-o file` is a flag of the tool (spcParseArgs, simParseArgs); without
  /// it `-o` goes to `extra` like any other unknown flag.
  bool output = false;
  /// `-T techname`: the standalone build only (EFargs.c `#ifndef
  /// MAGIC_WRAPPER`); without it `-T` goes to `extra`.
  bool tech = false;
  /// Reads a `-S` file whole; nullopt when it cannot be opened. Unset, no
  /// file can be opened.
  std::function<std::optional<std::string>(const std::string& path)> read_file{};
};

/// An ArgOptions::read_file over `fs` (which must outlive the parse).
[[nodiscard]] std::function<std::optional<std::string>(const std::string& path)> file_reader(
    platform::FileSystem& fs);

/// EFArgs over the words after the command name. Fails with the usage text
/// (EXTFLAT-READ-015) for an unknown flag, a flag without its value, or
/// `-h` (after `extra` has seen it).
[[nodiscard]] diag::Outcome<CommonArgs> parse_common_args(std::span<const std::string> args,
                                                          const ExtraFlags& extra, const ArgOptions& options = {});

/// The usage text EFArgs prints (MAGIC_WRAPPER form).
[[nodiscard]] std::string_view common_args_usage() noexcept;

/// efSymAdd: adds `sym=value` (value an integer) to `symbols`; returns the
/// message Magic prints when it is rejected (no '=', a value that is not
/// an integer, or a symbol already defined).
[[nodiscard]] std::optional<std::string> add_symbol(SymbolTable& symbols, std::string_view assignment);

}  // namespace hefesto::extflat
