// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF/DEF tokenizer and message reporting (capability lef-read
// "LEF/DEF tokenizer", "LEF/DEF message reporting";
// openspec/changes/m6-lef-def-oasis/design.md D4, D6):
// - Lexer: LefNextToken and LefEstimate (lefRead.c) over a byte source:
//   2048-character lines, `#` comments, quoted strings, `;` glued to a
//   number split off, and the line and byte position of each token for
//   messages and progress. LEF read and DEF read share it, each with its
//   own keyword tables;
// - MessageCounter: LefError's message cap (100 messages of each severity,
//   then one "further messages suppressed" note) and the totals printed at
//   the end of a read.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFR fills
// in the implementation in src/core/src/lef/lex/ (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::lef {

/// One token and where it starts.
struct Token {
  std::string text;
  std::uint32_t line = 0;    // 1-based
  std::uint64_t offset = 0;  // byte offset in the decompressed stream

  friend bool operator==(const Token&, const Token&) = default;
};

class Lexer {
 public:
  /// Tokenizes `source`, which must outlive the lexer.
  explicit Lexer(gds::ByteSource& source) : source_(&source) {}

  /// The next token; nullopt at end of input. SCAFFOLD: fails with
  /// CORE-NOTIMPL-006 until LEFR lands it.
  [[nodiscard]] diag::Outcome<std::optional<Token>> next();

  /// The current line (1-based; 0 before the first token).
  [[nodiscard]] std::uint32_t line() const noexcept { return line_; }

  /// LefEstimate: the fraction of the input consumed, in [0, 1], for
  /// progress reports (Q-LEFDEF-88). SCAFFOLD: returns 0.
  [[nodiscard]] double estimate() const noexcept;

 private:
  [[maybe_unused]] gds::ByteSource* source_;  // read by LEFR's implementation
  std::uint32_t line_ = 0;
};

/// LefError's counters for one read.
class MessageCounter {
 public:
  /// The cap of messages printed per severity (lefRead.c LEF_MAX_ERRORS).
  static constexpr std::uint32_t kLimit = 100;

  /// Counts `severity`; true when the message is to be printed (under the
  /// cap). SCAFFOLD: counts and always answers true until LEFR lands the
  /// cap and its suppression note.
  bool count(diag::Severity severity);

  [[nodiscard]] std::uint32_t errors() const noexcept { return errors_; }
  [[nodiscard]] std::uint32_t warnings() const noexcept { return warnings_; }

 private:
  std::uint32_t errors_ = 0;
  std::uint32_t warnings_ = 0;
};

}  // namespace hefesto::lef
