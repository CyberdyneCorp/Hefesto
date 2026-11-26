// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The steps of deck preparation (task 13.1), private to sim/deck: logical
// lines, include inlining with the deck limits, sanitisation of `.control`
// sections (Q-SIM-9), structural validation, and delivery.
//
// Owner: SIMDECK.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/deck/deck.hpp"

namespace hefesto::sim::deck {

/// The longest line passed to the engine ("Deck delivery").
inline constexpr std::size_t kMaxDeliveredLine = 65535;

/// One logical line: a card and its `+` continuation lines joined with a
/// space; `first` is the index of its first physical line.
struct LogicalLine {
  std::size_t first = 0;
  std::size_t last = 0;
  std::string text;
};

/// The logical lines of `lines`, from index `from` (1 skips a deck's
/// title line). Comment lines (`*`) and empty lines are skipped.
[[nodiscard]] std::vector<LogicalLine> logical_lines(const std::vector<std::string>& lines, std::size_t from);

/// The first token of `text` in lower case ("" when none).
[[nodiscard]] std::string card_of(std::string_view text);

/// Where included files are read and what they may cost.
struct IncludeContext {
  platform::FileSystem* files = nullptr;
  std::string base_dir;   // relative paths of the top-level lines
  DeckLimits limits;
  std::string source;     // the name of the top-level text in diagnostics
};

/// Replaces every `.include` / `.inc` line by the file's lines (resolved
/// against the including file's directory; the top level against
/// `base_dir`), recursively. SIM-DECK-001 for a file that cannot be read,
/// SIM-DECK-002 past the include depth (an include loop ends there),
/// SIM-DECK-003 when the result passes the deck size limit.
[[nodiscard]] diag::Outcome<std::vector<std::string>> inline_includes(const std::vector<std::string>& lines,
                                                                      const IncludeContext& context);

/// Q-SIM-9: rejects a `.control` section that holds a forbidden command
/// (SIM-DECK-004) and, unless `allow_control`, removes the others with the
/// warning SIM-DECK-005 `Removed .control section (lines <a>-<b>)` (1-based
/// line numbers of `lines`).
[[nodiscard]] diag::Status sanitise(std::vector<std::string>& lines, bool allow_control,
                                    std::vector<diag::Diagnostic>& warnings);

/// "Deck validation and sanitisation": `.subckt`/`.ends` balance
/// (SIM-DECK-006) and the subcircuits that instances call (SIM-DECK-007;
/// not checked when the deck has `.lib` lines, whose PDK subcircuits are
/// exempt). `raw`: also "Raw decks" (SIM-DECK-008 no analysis, SIM-DECK-009
/// no final `.end`).
[[nodiscard]] diag::Status validate(const std::vector<std::string>& lines, bool raw);

/// "Deck delivery": lines longer than kMaxDeliveredLine split with `+`
/// continuation lines at a space boundary (a line without one is split
/// at the limit).
[[nodiscard]] std::vector<std::string> split_long_lines(const std::vector<std::string>& lines);

/// The deck's size as text: every line and its newline.
[[nodiscard]] std::size_t deck_bytes(const std::vector<std::string>& lines);

}  // namespace hefesto::sim::deck
