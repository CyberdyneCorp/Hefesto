// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SPICE/CDL lexer of the LVS reader (lvs-netlist-input "SPICE lexical
// structure"): physical lines joined by a trailing backslash and by `+`
// continuation lines (a full comment line between continuation lines is
// skipped), then split into tokens. Private to lvs/read.
//
// Owner: LVSREAD.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::lvs::spice {

struct LogicalLine {
  std::string text;
  std::uint32_t line = 0;  // first physical line, from 1
};

/// Splits a text into logical lines. Never reads past the text; each call
/// is linear in the bytes it consumes.
class Lexer {
 public:
  enum class Next : std::uint8_t { line, end, too_long };

  Lexer(std::string_view text, std::size_t max_line) noexcept : text_(text), max_line_(max_line) {}

  /// The next logical line, or end, or too_long (the line exceeded
  /// `max_line` bytes; `out.line` names where it starts).
  Next next(LogicalLine& out);

 private:
  /// The physical line at pos_ without its line end; advances pos_ and
  /// line_. A trailing backslash joins the following physical line.
  bool append_physical(std::string& out);
  [[nodiscard]] std::string_view peek_line(std::size_t pos) const noexcept;
  /// pos of the next continuation line, skipping full comment lines, or npos.
  [[nodiscard]] std::size_t continuation_after_comments(std::uint32_t& lines) const noexcept;

  std::string_view text_;
  std::size_t max_line_;
  std::size_t pos_ = 0;
  std::uint32_t line_ = 0;
};

/// The tokens of a logical line: separated by space, tab, CR and LF; inside
/// single or double quotes spaces are dropped so a quoted expression is one
/// token (a single quote right after a digit opens no quote).
[[nodiscard]] std::vector<std::string> tokenize(std::string_view line);

}  // namespace hefesto::lvs::spice
