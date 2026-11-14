// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF lexer (capability cif-format-io "CIF lexical rules"; Magic's
// cif/CIFrdutils.c:348-560 and 1394-1460): a one-character look-ahead over
// the file text with Magic's skip rules. A blank is any character other
// than a decimal digit, an upper-case ASCII letter, `-`, `;`, `(`, `)` and
// end of file; lower-case letters are blanks. Only the skip functions and
// comments count newlines, as in Magic, so the line numbers of messages
// match Magic's.
//
// Integers are parsed into 64 bits and saturate at kIntegerLimit, so a
// malformed file cannot overflow them; the parser range-checks every value
// it scales (design D21 resource caps).
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D2, D13).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hefesto::cif {

class Lexer {
 public:
  /// The value of peek() and take() at end of file (C's EOF).
  static constexpr int kEof = -1;
  /// Integers saturate at +-kIntegerLimit (Magic's atoi has no limit).
  static constexpr std::int64_t kIntegerLimit = std::int64_t{1} << 62;

  explicit Lexer(std::string_view text) noexcept : text_(text) {}

  /// The next character (0..255) or kEof (PEEK).
  [[nodiscard]] int peek() const noexcept {
    return pos_ < text_.size() ? static_cast<unsigned char>(text_[pos_]) : kEof;
  }
  /// Consumes and returns the next character, or kEof (TAKE). Does not
  /// count newlines.
  int take() noexcept;

  /// The line number for messages (cifLineNumber), starting at 1.
  [[nodiscard]] std::int64_t line() const noexcept { return line_; }
  /// True when every character was consumed.
  [[nodiscard]] bool at_end() const noexcept { return pos_ >= text_.size(); }
  /// The number of characters consumed (for progress reports).
  [[nodiscard]] std::size_t position() const noexcept { return pos_; }

  /// True when `ch` is a CIF blank (cifIsBlank).
  [[nodiscard]] static bool is_blank(int ch) noexcept;

  /// Skips blanks (CIFSkipBlanks).
  void skip_blanks() noexcept;
  /// Skips blanks and upper-case letters (CIFSkipSep).
  void skip_separators() noexcept;
  /// Skips to the next `;` or end of file, leaving it unread (CIFSkipToSemi).
  void skip_to_semicolon() noexcept;
  /// Skips blanks, a `;` and the blanks after it (CIFSkipSemi). Returns
  /// false, consuming nothing more, when the next character is not `;`.
  [[nodiscard]] bool skip_semicolon() noexcept;
  /// Skips a comment starting at `(`, nesting allowed (cifParseComment).
  /// Returns false when it reaches end of file.
  [[nodiscard]] bool skip_comment() noexcept;

  /// A signed integer after separators (CIFParseSInteger); nullopt when no
  /// digit follows (a `-` is consumed even then).
  [[nodiscard]] std::optional<std::int64_t> signed_integer() noexcept;
  /// A name: leading spaces and tabs skipped, then everything up to
  /// whitespace, `;` or end of file (cifParseName).
  [[nodiscard]] std::string name();
  /// Up to five characters of [0-9A-Z] after blanks (the CIFParseLayer name).
  [[nodiscard]] std::string layer_name();

 private:
  std::string_view text_;
  std::size_t pos_ = 0;
  std::int64_t line_ = 1;
};

}  // namespace hefesto::cif
