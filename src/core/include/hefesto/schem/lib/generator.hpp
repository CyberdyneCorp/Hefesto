// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol generators (capability schematic-symbols-and-libraries "Symbol
// generators"; Q-SCHEM-11, `Resolution: platform`): a reference `name(args)`
// runs `name`, found on the search path, and reads what it prints as a
// `.sym` file. Only Tcl generators run, in a fresh sandboxed interpreter;
// a generator that would run an external program is a missing symbol.
//
// Owner: SYMLIB.

#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/schematic_symbol.hpp"
#include "hefesto/schem/lib/search_path.hpp"

namespace hefesto::schem {

/// The message of a generator that is not a Tcl script ("Symbol generators").
inline constexpr std::string_view kExternalGeneratorMessage =
    "symbol generators that run external programs are not supported";

/// True when `reference` is a generator reference: it matches
/// `^[^ \t()]+\([^()]*\)[ \t]*$`.
[[nodiscard]] bool is_generator_reference(std::string_view reference) noexcept;

/// A generator reference split at `(`, `)`, `,` and spaces (empty pieces
/// dropped): the first piece is the program, the others its arguments.
struct GeneratorCall {
  std::string program;
  std::vector<std::string> args;

  friend bool operator==(const GeneratorCall&, const GeneratorCall&) = default;
};

/// The call a generator reference makes; nullopt when `reference` is not one.
[[nodiscard]] std::optional<GeneratorCall> parse_generator(std::string_view reference);

/// The longest run of one Tcl generator (the time limit of the `xschemrc`
/// files, Q-SCHEM-32, applied to generators too).
inline constexpr std::chrono::milliseconds kGeneratorTimeLimit = kXschemrcTimeLimit;

/// The longest the Tcl generators of one symbol cache may run in total
/// between two SymbolCache::clear() calls (a schematic with thousands of
/// distinct generator references would otherwise block for hours); past it
/// no generator runs and the reference is a missing symbol (SCHEM-LIB-012).
/// Both modes of `xschem-compat`; proposed in M8 round 3, awaiting the user.
inline constexpr std::chrono::milliseconds kGeneratorTotalTimeLimit{60'000};

/// The largest generator file read, to check its first line or to run it
/// (the bound of a flattened symbol, Q-SCHEM-40); a larger one is not a Tcl
/// generator, or fails with SCHEM-LIB-012.
inline constexpr std::size_t kMaxGeneratorScriptBytes = kMaxFlattenedBytes;

/// True when the generator `file` is a Tcl script: its name ends in `.tcl`
/// (as the generators of the pinned sky130 tree, `res.tcl(...)`), or its
/// first line is a `#!` line that names `tclsh` or `wish` (a file over
/// kMaxGeneratorScriptBytes is not read and is not one).
[[nodiscard]] bool is_tcl_generator(const platform::Path& file, platform::FileSystem& fs,
                                    std::size_t max_script = kMaxGeneratorScriptBytes);

struct GeneratorInput {
  platform::Path script;           // the Tcl generator, absolute
  std::vector<std::string> args;   // `argv`
  platform::Path document_folder;  // readable, and env(HOME) and env(PWD)
  /// The run stops after this long, in both modes of `xschem-compat`.
  std::chrono::milliseconds time_limit = kGeneratorTimeLimit;
  /// The most bytes the generator may print; more is a failure (the bound
  /// of a flattened symbol, Q-SCHEM-40).
  std::size_t max_output = kMaxFlattenedBytes;
  /// The largest script read; a larger one fails (tests lower it).
  std::size_t max_script = kMaxGeneratorScriptBytes;
};

struct GeneratorOutput {
  /// What the generator printed to standard output; nullopt when it failed
  /// (Tcl error, time limit, too much output, unreadable script).
  std::optional<std::string> text;
  /// SCHEM-LIB-012 when it failed.
  std::vector<diag::Diagnostic> diagnostics;
};

/// Runs the Tcl generator `input.script`, read through `fs`, in a fresh
/// sandboxed project interpreter that may read only the script's folder and
/// the document folder and write nothing, with `argv` (a list of
/// `input.args`), `argc` and `argv0` set as `tclsh` sets them. `puts` to
/// `stdout` (or with no channel) is the symbol text; `puts` to another
/// channel is dropped.
[[nodiscard]] GeneratorOutput run_tcl_generator(platform::FileSystem& fs, const GeneratorInput& input);

}  // namespace hefesto::schem
