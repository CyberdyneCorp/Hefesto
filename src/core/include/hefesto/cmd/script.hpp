// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M0 command-script reader and runner. A placeholder for the Tcl binding
// (decision X-3), covering only what M0 needs (design D6):
//   - commands are separated by ';' and newlines;
//   - words are separated by blanks (space, tab, CR);
//   - "..." groups a word; inside it \" \\ \n \t are escapes and any other
//     backslash pair stands for the escaped character;
//   - {...} groups a word verbatim, with nesting; a backslash in braces keeps
//     itself and the next character and stops that character from counting
//     as a brace;
//   - outside groups, a backslash escapes the next character, and
//     backslash-newline joins lines;
//   - '#' at the start of a command comments out the rest of the line.
// No substitution ($, [ ]) is performed.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::cmd {

struct ParsedCommand {
  std::vector<std::string> words;  // never empty
  std::uint32_t line = 1;          // 1-based line of the first word

  friend bool operator==(const ParsedCommand&, const ParsedCommand&) = default;
};

struct ParsedScript {
  std::vector<ParsedCommand> commands;
  /// Set when the text is malformed (CMD-PARSE-00x); `commands` then holds
  /// the commands before the error and nothing is run.
  std::optional<diag::Diagnostic> error;
};

/// Splits `text` into commands. Never throws; malformed input gives `error`.
/// `source_name` labels the location of a parse error.
[[nodiscard]] ParsedScript parse_script(std::string_view text, std::string_view source_name = {});

/// Quotes one word so that parse_script() reads it back unchanged.
[[nodiscard]] std::string quote_word(std::string_view word);

struct ScriptRun {
  std::size_t commands_run = 0;
  std::size_t failures = 0;  // commands whose status was failed
  bool aborted = false;      // stopped by a parse error, dispatch failure, raised error or strict mode
  bool quit = false;         // stopped by `quit`
  /// The error that aborted the script: the parse error, or the first error
  /// diagnostic of the command that aborted it (code and text).
  std::optional<diag::Diagnostic> error;

  [[nodiscard]] bool ok() const noexcept { return failures == 0 && !aborted; }
};

/// Runs a command script through the dispatcher.
///
/// Status convention (capability hefesto-diagnostics, "Command results and
/// errors" and "Script strict mode switch", mirrored on the M0 reader):
///   - by default a failing command does not stop the script (Magic's Tcl
///     convention); its diagnostics are still recorded and it counts in
///     `failures`, so the CLI can still exit non-zero;
///   - in strict mode (session().strict) the first failing command aborts
///     the script with that command's first error diagnostic;
///   - a dispatch failure (unknown command) and a failure marked
///     CommandResult::raises (e.g. `hefesto::strict maybe`) abort in both modes;
///   - with session().catch_lines (each line wrapped in `catch`) no command
///     failure aborts the script;
///   - a parse error aborts before any command runs.
ScriptRun run_script(Dispatcher& dispatcher, std::string_view text, Origin origin, std::string_view source_name = {});

}  // namespace hefesto::cmd
