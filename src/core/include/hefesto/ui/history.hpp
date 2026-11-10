// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command history of the command prompt (capability ui-command-dispatch,
// requirement "Command history"; hefesto-input-and-touch "Command entry on
// every platform"). Every command-entry front end adds the lines the user
// enters and expands them with GNU Readline history rules before running
// them; the `history` command lists the entries.
//
// Supported expansions (GNU Readline "History Expansion"): event
// designators `!!`, `!n`, `!-n`, `!string`, `!?string?`, `!#`, the quick
// substitution `^old^new^`; word designators after `:` (`0`, `n`, `^`, `$`,
// `x-y`, `x-`, `-y`, `*`, `x*`), also `!^`, `!$`, `!*` without `:`; the
// modifiers `:s/old/new/`, `:gs/old/new/`, `:p`, `:h`, `:t`, `:r`, `:e`.
// A `!` followed by a blank, `=`, `(` or the end of the line stays literal,
// and so does any `!` inside single quotes or after a backslash.
// (Magic ref: commands/CmdFI.c:1472-1552, textio/txInput.c:997-1110)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::ui {

struct HistoryExpansion {
  std::string line;
  /// The `:p` modifier was given: print the line, do not run it.
  bool print_only = false;
};

class CommandHistory {
 public:
  /// Appends an entered line (empty lines are not kept). Indices start at 1.
  void add(std::string line);
  [[nodiscard]] const std::vector<std::string>& entries() const noexcept { return lines_; }
  [[nodiscard]] std::size_t size() const noexcept { return lines_.size(); }

  /// Expands `line` against the history. Fails with UI-HISTORY-001
  /// (`!x: event not found`, `bad word specifier`, `substitution failed`).
  [[nodiscard]] diag::Outcome<HistoryExpansion> expand(std::string_view line) const;

  /// The `history` listing: the last `count` entries (all when 0), each
  /// `\t%4d line` (or `line` without numbers), newest last unless
  /// `reverse`.
  [[nodiscard]] std::string listing(std::size_t count, bool numbers, bool reverse) const;

 private:
  std::vector<std::string> lines_;
};

}  // namespace hefesto::ui
