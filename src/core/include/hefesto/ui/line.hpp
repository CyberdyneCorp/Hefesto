// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command lines (capability ui-command-dispatch, requirements "Command line
// splitting and quoting", "Script files and source", "Host command
// interface mechanics"). A line typed at a command prompt or stored in a
// macro is split with Magic's ParsSplit rules into commands and words, with
// no length or word-count limit (Q-UI-10). Command files read through the
// dispatcher follow the Tcl line rules of Q-UI-13.
// (Magic ref: utils/parser.c:55-160, textio/txCommands.c:1095-1153)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"

namespace hefesto::ui {

struct SplitLine {
  /// The commands of the line, each its words; empty commands are dropped.
  std::vector<std::vector<std::string>> commands;
  /// The quote character of the first unmatched quoted span, if any (parsing
  /// went on as if it were closed at the end of the line).
  std::optional<char> unmatched_quote;
};

/// Splits a command line: blanks separate words, an unquoted `;` ends a
/// command, `"` and `'` quote (a backslash inside quotes makes the next
/// character literal), quoted spans join adjacent text, `""` is an empty
/// word. Outside quotes a backslash is literal.
[[nodiscard]] SplitLine split_command_line(std::string_view line);

/// The UI-PARSE-001 warning of an unmatched quote: `Unmatched " in string,
/// I'll pretend that there is one at the end`.
[[nodiscard]] diag::Diagnostic unmatched_quote_warning(char quote);

/// Splits `line` and dispatches each command with `origin`; returns the
/// results in order (an unmatched quote is reported through the dispatcher).
std::vector<cmd::CommandResult> run_command_line(cmd::Dispatcher& dispatcher, std::string_view line,
                                                 cmd::Origin origin);

/// The command lines of a command file read through the dispatcher
/// (Q-UI-13): `\`-newline joins with the next line, the newline and the
/// next line's leading blanks becoming one space; empty lines and lines
/// whose first non-blank is `#` are skipped; one leading `:` (not `::`) is
/// removed.
[[nodiscard]] std::vector<std::string> command_file_lines(std::string_view text);

}  // namespace hefesto::ui
