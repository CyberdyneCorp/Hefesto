// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers that reproduce the C library input functions Magic's
// display-file readers are built on, so that Hefesto reads the same bytes
// the same way: fgets() line chunking and the sscanf() conversions %d, %o,
// %x, %i, %c and %Ns (capability display-style-files, "Dstyle file
// grammar", "Color map file grammar", "Glyph file grammar"; font-labels
// "Outline font file loading").
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::display::cscan {

/// The successive results of fgets(buf, size, f) over `text`: each chunk
/// holds at most size-1 bytes and ends after a '\n' when one fits.
[[nodiscard]] std::vector<std::string_view> fgets_chunks(std::string_view text, std::size_t size);

/// C isspace() in the "C" locale.
[[nodiscard]] constexpr bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/// Magic's StrIsWhite(line, commentok).
[[nodiscard]] bool is_white(std::string_view line, bool comment_ok) noexcept;

/// Magic's StrIsInt: optional sign, then only decimal digits (an empty
/// remainder counts as an integer, as in Magic).
[[nodiscard]] bool is_int(std::string_view s) noexcept;

/// C atoi() (leading blanks, optional sign, decimal digits; 0 if none).
[[nodiscard]] std::int64_t atoi(std::string_view s) noexcept;

/// strtod()-style "is there a number at the start" test (Magic's StrIsNumeric)
/// and the parsed value.
[[nodiscard]] std::optional<double> leading_number(std::string_view s) noexcept;

/// A cursor over one input string with sscanf()'s conversion rules.
class Scanner {
 public:
  explicit Scanner(std::string_view input) noexcept : input_(input) {}

  /// A blank in a format string: skips any white space.
  void skip_space() noexcept;
  /// %d (base 10), %o (base 8), %x (base 16), %i (base 0: C prefixes).
  /// Leading white space is skipped. std::nullopt is a matching failure.
  [[nodiscard]] std::optional<std::int64_t> integer(int base) noexcept;
  /// %Ns: skips white space, then reads at most `width` non-blank bytes.
  [[nodiscard]] std::optional<std::string> word(std::size_t width);
  /// %c: one byte, white space included (call skip_space() first for " %c").
  [[nodiscard]] std::optional<char> character() noexcept;
  /// %N[^\n]: at most `width` bytes up to a newline (no white-space skip).
  [[nodiscard]] std::optional<std::string> until_newline(std::size_t width);
  /// A literal in the format (matched exactly).
  [[nodiscard]] bool literal(std::string_view text) noexcept;

  [[nodiscard]] bool at_end() const noexcept { return pos_ >= input_.size(); }
  [[nodiscard]] std::string_view rest() const noexcept { return input_.substr(pos_); }

 private:
  std::string_view input_;
  std::size_t pos_ = 0;
};

}  // namespace hefesto::display::cscan
