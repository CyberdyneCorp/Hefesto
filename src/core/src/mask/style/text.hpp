// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// STYLES-private: the C library and Magic utility semantics the cifoutput
// reader needs (atoi, StrIsInt, FindGCF, strncmp prefix tests, the comma
// list loop), with saturating arithmetic where C overflows.
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::mask::style_text {

/// Saturates a 64-bit value to the 32-bit range of Magic's `int`.
[[nodiscard]] inline std::int32_t clamp_int(std::int64_t v) noexcept {
  constexpr auto lo = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
  constexpr auto hi = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
  return static_cast<std::int32_t>(v < lo ? lo : (v > hi ? hi : v));
}

/// C atoi: leading white space, an optional sign, the leading digits; 0
/// when there are none; saturating.
[[nodiscard]] inline std::int32_t c_atoi(std::string_view s) noexcept {
  std::size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])) != 0) {
    ++i;
  }
  bool negative = false;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
    negative = s[i] == '-';
    ++i;
  }
  std::int64_t value = 0;
  constexpr std::int64_t kCap = std::int64_t{1} << 40;
  for (; i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) != 0; ++i) {
    if (value < kCap) {
      value = value * 10 + (s[i] - '0');
    }
  }
  return clamp_int(negative ? -value : value);
}

/// StrIsInt: an optional sign followed only by digits (the empty string and
/// a lone sign count, as in Magic).
[[nodiscard]] inline bool str_is_int(std::string_view s) noexcept {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  for (const char c : s) {
    if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
      return false;
    }
  }
  return true;
}

/// FindGCF: the positive greatest common factor; gcf(a, 0) = |a|.
[[nodiscard]] inline std::int64_t gcf(std::int64_t a, std::int64_t b) noexcept {
  a = a < 0 ? -a : a;
  b = b < 0 ? -b : b;
  while (b != 0) {
    const std::int64_t r = a % b;
    a = b;
    b = r;
  }
  return a;
}

[[nodiscard]] inline bool starts_with(std::string_view s, std::string_view prefix) noexcept {
  return s.substr(0, prefix.size()) == prefix;
}

/// Magic's comma list loop (`while (*tptr) { cptr = strchr(tptr, ','); ... }`):
/// "a,,b" gives "a", "", "b"; a trailing comma adds no empty piece.
[[nodiscard]] inline std::vector<std::string> split_commas(std::string_view text) {
  std::vector<std::string> pieces;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t comma = text.find(',', start);
    if (comma == std::string_view::npos) {
      pieces.emplace_back(text.substr(start));
      break;
    }
    pieces.emplace_back(text.substr(start, comma - start));
    start = comma + 1;
  }
  return pieces;
}

}  // namespace hefesto::mask::style_text
