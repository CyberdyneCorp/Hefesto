// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tab completion in the Tcl console (board docs/design/mockups/
// M4-Console.dc.html note 5: "Tab completes commands, procedures and
// variables"; openspec/changes/m4-scripting tasks 10.5).
//
//   - tcl_completion_word: the word before the cursor and what it names: a
//     command (procedures included) in command position, a variable after
//     `$`, nothing elsewhere;
//   - complete_tcl_word: the input with that word completed from the names
//     the interpreter knows (`info commands` / `info vars`);
//   - text_offset / text_position: the editor's line and byte column against
//     a byte offset in the input.
//
// Toolkit-free and unit-tested; TclConsole::complete_input asks the
// interpreter through ConsoleEvaluator::names.
//
// Owner: SHELL (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::shell {

/// What the word under completion names.
enum class TclWordKind : std::uint8_t {
  none,      // an argument: nothing to complete
  command,   // the first word of a command (after start, newline, `;`, `[` or `{`)
  variable,  // after `$` (or `${`)
};

struct TclCompletionWord {
  TclWordKind kind = TclWordKind::none;
  std::size_t start = 0;  // byte offset of the word (after `$` / `${`)
  std::string prefix;     // the word's text up to the cursor
};

/// The word that ends at byte `cursor` of `text` (clamped to its size).
[[nodiscard]] TclCompletionWord tcl_completion_word(std::string_view text, std::size_t cursor);

/// A glob pattern matching the names that start with `prefix` (`*`, `?`,
/// `[`, `]` and `\` escaped), for `info commands` / `info vars`.
[[nodiscard]] std::string tcl_prefix_glob(std::string_view prefix);

struct ConsoleCompletion {
  std::string text;     // the input after completion
  std::size_t cursor = 0;  // byte offset of the cursor after it
  std::vector<std::string> candidates;  // sorted, unique: every name that matched
};

/// Completes `word` in `text` (the cursor at `cursor`) from `names`: the
/// names starting with the prefix, sorted. One name replaces the word (a
/// command gets a space after it); several extend it to their longest
/// common prefix. Empty candidates when nothing matched.
[[nodiscard]] ConsoleCompletion complete_tcl_word(std::string_view text, std::size_t cursor,
                                                  const TclCompletionWord& word, std::vector<std::string> names);

/// The transcript line listing several candidates ("3 matches: box · boxes …"),
/// at most `max` names.
[[nodiscard]] std::string completion_listing(const std::vector<std::string>& candidates, std::size_t max = 12);

/// The byte offset of `line` / byte `column` in `text` (clamped).
[[nodiscard]] std::size_t text_offset(std::string_view text, int line, int column);
/// The line and byte column of byte `offset` of `text` (clamped).
[[nodiscard]] std::pair<int, int> text_position(std::string_view text, std::size_t offset);

}  // namespace hefesto::shell
