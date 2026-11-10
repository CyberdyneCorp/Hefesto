// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RULES-private: the C library parsing helpers Magic's drc and cifoutput
// readers use (atoi, atof, strtol, sscanf "%d", utils/lookup.c Lookup),
// with the undefined behavior of the originals on overflow replaced by
// saturation (docs/decisions/cross-cutting.md X-2 tier 1), and the small
// integer helpers of utils/fraction.c.
//
// (Magic ref: utils/lookup.c Lookup, LookupStruct; utils/fraction.c FindGCF)
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace hefesto::drc::text {

/// The leading integer of `s` after optional white space and sign, with the
/// position after its last digit; std::nullopt when there is no digit
/// (strtol with endptr == s).
struct ParsedInt {
  std::int64_t value = 0;
  std::size_t end = 0;  // index after the number (0 when none)
  bool digits = false;
};

[[nodiscard]] inline ParsedInt parse_leading_int(std::string_view s) noexcept {
  std::size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])) != 0) {
    ++i;
  }
  bool negative = false;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
    negative = s[i] == '-';
    ++i;
  }
  ParsedInt out;
  constexpr std::int64_t kCap = std::int64_t{1} << 40;  // saturates long before int64 overflow
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) != 0) {
    out.digits = true;
    if (out.value < kCap) {
      out.value = out.value * 10 + (s[i] - '0');
    }
    ++i;
  }
  if (out.digits) {
    out.end = i;
  }
  out.value = negative ? -out.value : out.value;
  return out;
}

[[nodiscard]] inline std::int32_t clamp_int(std::int64_t v) noexcept {
  constexpr auto lo = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
  constexpr auto hi = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
  return static_cast<std::int32_t>(v < lo ? lo : (v > hi ? hi : v));
}

/// C atoi: the leading integer, 0 when there is none; saturating.
[[nodiscard]] inline std::int32_t c_atoi(std::string_view s) noexcept {
  return clamp_int(parse_leading_int(s).value);
}

/// `int x = atof(s)`: the leading floating-point value truncated toward
/// zero (saturating instead of the undefined out-of-range conversion).
[[nodiscard]] inline std::int32_t c_atof_int(const std::string& s) noexcept {
  const double v = std::strtod(s.c_str(), nullptr);
  if (!(v > -2147483648.0)) {  // also NaN
    return v != v ? 0 : std::numeric_limits<std::int32_t>::min();
  }
  if (v >= 2147483647.0) {
    return std::numeric_limits<std::int32_t>::max();
  }
  return static_cast<std::int32_t>(v);
}

/// sscanf(s, "%d", &v) == 1.
[[nodiscard]] inline std::optional<std::int32_t> scan_int(std::string_view s) noexcept {
  const ParsedInt p = parse_leading_int(s);
  if (!p.digits) {
    return std::nullopt;
  }
  return clamp_int(p.value);
}

[[nodiscard]] inline bool starts_with(std::string_view s, std::string_view prefix) noexcept {
  return s.substr(0, prefix.size()) == prefix;
}

/// strncmp(a, b, n) == 0 for NUL-terminated strings of the given views.
[[nodiscard]] inline bool strncmp_equal(std::string_view a, std::string_view b, std::size_t n) noexcept {
  for (std::size_t i = 0; i < n; ++i) {
    const char ca = i < a.size() ? a[i] : '\0';
    const char cb = i < b.size() ? b[i] : '\0';
    if (ca != cb) {
      return false;
    }
    if (ca == '\0') {
      return true;
    }
  }
  return true;
}

/// Magic's Lookup(): case-insensitive unique-prefix match over `table`
/// (a table entry ends at its first space), the Tcl build's `::magic::` /
/// `magic::` namespace prefix skipped. Returns the index, -1 when
/// ambiguous, -2 when nothing matches.
[[nodiscard]] inline int lookup(std::string_view str, std::span<const std::string_view> table) noexcept {
  if (starts_with(str, "::magic::")) {
    str.remove_prefix(9);
  } else if (starts_with(str, "magic::")) {
    str.remove_prefix(7);
  }
  int match = -2;
  for (std::size_t pos = 0; pos < table.size(); ++pos) {
    const std::string_view entry = table[pos];
    std::size_t k = 0;
    while (k < str.size() && k < entry.size() && entry[k] != ' ' &&
           std::tolower(static_cast<unsigned char>(entry[k])) == std::tolower(static_cast<unsigned char>(str[k]))) {
      ++k;
    }
    if (k != str.size()) {
      continue;
    }
    if (k == entry.size() || entry[k] == ' ') {
      return static_cast<int>(pos);  // exact match
    }
    match = match == -2 ? static_cast<int>(pos) : -1;
  }
  return match;
}

/// FindGCF (utils/fraction.c): the greatest common factor of |a| and |b|;
/// |b| when a is a multiple of b (so FindGCF(0, b) = |b|).
[[nodiscard]] inline std::int64_t find_gcf(std::int64_t a, std::int64_t b) noexcept {
  a = a < 0 ? -a : a;
  b = b < 0 ? -b : b;
  while (b != 0) {
    const std::int64_t r = a % b;
    if (r == 0) {
      return b;
    }
    a = b;
    b = r;
  }
  return a;
}

}  // namespace hefesto::drc::text
