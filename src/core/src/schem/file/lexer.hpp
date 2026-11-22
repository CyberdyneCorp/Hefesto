// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The byte-level lexer of `.sch`/`.sym` files (capability
// schematic-file-format "File kinds and character handling", "Record
// grammar", "Brace strings", "Numeric fields", "Malformed input").
//
// Every operation is bounded by the input: no step reads past the end, and
// each call either consumes at least one byte or reports that it could not.
//
// Owner: FORMAT.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hefesto::schem::file_detail {

/// A brace string as read: its decoded value and whether the input ended
/// before the closing brace (the field is then absent, "Brace strings" 6).
struct BraceString {
  std::string value;
  bool unterminated = false;
};

class Lexer {
 public:
  explicit Lexer(std::string_view bytes) noexcept : bytes_(bytes) {}

  [[nodiscard]] bool at_end() const noexcept { return pos_ >= bytes_.size(); }
  [[nodiscard]] std::size_t position() const noexcept { return pos_; }
  /// The 1-based line of the current position.
  [[nodiscard]] std::uint32_t line();

  /// Skips spaces, tabs, CRs and LFs ("Record grammar").
  void skip_whitespace() noexcept;
  /// The next byte, consumed / not consumed; the caller checks at_end() first.
  char take() noexcept { return bytes_[pos_++]; }
  [[nodiscard]] char peek() const noexcept { return bytes_[pos_]; }

  /// Skips white space, then reads a `%lf` / `%d` field. On failure nothing
  /// past the white space is consumed ("Malformed input").
  [[nodiscard]] std::optional<double> read_double();
  [[nodiscard]] std::optional<int> read_int();

  /// Skips every byte up to and including the first `{`, then reads the
  /// string up to the unescaped `}` ("Brace strings").
  [[nodiscard]] BraceString read_brace_string();
  /// Reads the rest of a brace string whose `{` was already consumed.
  [[nodiscard]] BraceString read_brace_body();

  /// Discards the bytes up to, not including, the next LF.
  void skip_to_line_end() noexcept;

  /// Discards the rest of the current line and every later line up to and
  /// including the first one whose first byte is `]` (the skipped embedded
  /// block of "Embedded symbols"; nested `[` lines are not counted). Stops
  /// at the LF that ends that line, or at the end of the input.
  void skip_past_closing_line() noexcept;

 private:
  std::string_view bytes_;
  std::size_t pos_ = 0;
  std::size_t line_pos_ = 0;
  std::uint32_t line_ = 1;
};

}  // namespace hefesto::schem::file_detail
