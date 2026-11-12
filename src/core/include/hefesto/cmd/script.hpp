// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M0 command-script word splitter, a placeholder for the Tcl binding
// (decision X-3). Scripts no longer run through it: every script, `-c`
// text, startup file and console line is Tcl, evaluated in the project's
// interpreter (m4-scripting design D3, tasks.md 13.2). It is kept only for
// the readers that still split words without an interpreter (the key-binding
// file reader ui::read_macro_file and the desktop shell's command sinks) and
// is removed when they move to the interpreter. The rules (M0 design D6):
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

}  // namespace hefesto::cmd
