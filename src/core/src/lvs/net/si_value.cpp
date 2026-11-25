// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SI value parsing (lvs-netlist-input "SI value parsing"). A decimal
// mantissa is scanned by hand (C strtod syntax); its exponent and the scale
// suffix are added and the decimal text is converted once by strtod, which
// rounds correctly (the core never changes the C locale, so the decimal
// point is '.'; std::from_chars for double is unavailable before macOS 26).
// Hexadecimal, infinite and NaN mantissas are converted by strtod and then
// scaled.
//
// Owner: LVSREAD.

#include "hefesto/lvs/net/si_value.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>

namespace hefesto::lvs {

namespace {

constexpr long kExponentClamp = 100000;

char lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

bool equal_ignoring_case(std::string_view text, std::string_view word) noexcept {
  if (text.size() != word.size()) {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (lower(text[i]) != word[i]) {
      return false;
    }
  }
  return true;
}

/// A scanned mantissa: `length` bytes of the text. A decimal mantissa keeps
/// its digits (sign, integer and fraction) and its exponent apart; any
/// other kind is already converted into `value`.
struct Mantissa {
  std::size_t length = 0;
  bool decimal = true;
  std::string digits;
  long exponent = 0;
  double value = 0.0;
};

std::size_t skip_digits(std::string_view text, std::size_t pos) noexcept {
  while (pos < text.size() && is_digit(text[pos])) {
    ++pos;
  }
  return pos;
}

/// The exponent part at `pos` ("e", optional sign, digits); returns its end
/// or `pos` when there is none.
std::size_t scan_exponent(std::string_view text, std::size_t pos, long& exponent) noexcept {
  if (pos >= text.size() || lower(text[pos]) != 'e') {
    return pos;
  }
  std::size_t digits = pos + 1;
  if (digits < text.size() && (text[digits] == '+' || text[digits] == '-')) {
    ++digits;
  }
  const std::size_t end = skip_digits(text, digits);
  if (end == digits) {
    return pos;
  }
  long value = 0;
  for (std::size_t i = digits; i < end && value < kExponentClamp; ++i) {
    value = value * 10 + (text[i] - '0');
  }
  exponent = text[pos + 1] == '-' ? -value : value;
  return end;
}

/// Hexadecimal ("0x..."), infinity or NaN after the sign at `start`,
/// converted by strtod from the sign on.
std::optional<Mantissa> scan_special(std::string_view text, std::size_t sign, std::size_t start) {
  const auto rest = text.substr(start);
  const bool hex = rest.size() > 2 && rest[0] == '0' && lower(rest[1]) == 'x';
  if (rest.empty() || (!hex && !(lower(rest[0]) == 'i' || lower(rest[0]) == 'n'))) {
    return std::nullopt;
  }
  const std::string copy{text.substr(sign)};
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  const auto used = static_cast<std::size_t>(end - copy.c_str());
  if (used <= start - sign) {
    return std::nullopt;
  }
  Mantissa mantissa;
  mantissa.decimal = false;
  mantissa.value = value;
  mantissa.length = sign + used;
  return mantissa;
}

std::optional<Mantissa> scan_mantissa(std::string_view text) {
  std::size_t pos = 0;
  while (pos < text.size() && is_space(text[pos])) {
    ++pos;
  }
  const std::size_t sign = pos;
  const bool negative = pos < text.size() && text[pos] == '-';
  if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
    ++pos;
  }
  if (auto special = scan_special(text, sign, pos)) {
    return special;
  }
  Mantissa mantissa;
  mantissa.digits = negative ? "-" : "";
  const std::size_t int_end = skip_digits(text, pos);
  std::size_t end = int_end;
  if (end < text.size() && text[end] == '.') {
    end = skip_digits(text, end + 1);
  }
  const std::size_t digit_count = end - pos - ((end > int_end) ? 1 : 0);
  if (digit_count == 0) {
    return std::nullopt;
  }
  mantissa.digits.append(text.substr(pos, end - pos));
  mantissa.length = scan_exponent(text, end, mantissa.exponent);
  return mantissa;
}

struct Scale {
  std::size_t length = 0;
  int exponent = 0;
};

Scale scan_scale(std::string_view rest) noexcept {
  if (rest.empty()) {
    return {};
  }
  if (rest.size() >= 3 && equal_ignoring_case(rest.substr(0, 3), "meg")) {
    return {3, 6};
  }
  switch (lower(rest[0])) {
    case 'g': return {1, 9};
    case 'k': return {1, 3};
    case 'm': return {1, -3};
    case 'c': return {1, -2};
    case 'u': return {1, -6};
    case 'n': return {1, -9};
    case 'p': return {1, -12};
    case 'f': return {1, -15};
    case 'a': return {1, -18};
    default: return {};
  }
}

bool is_unit_word(std::string_view rest) noexcept {
  if (rest.empty()) {
    return true;
  }
  static constexpr std::array<std::string_view, 7> kUnits{"farad", "meter", "henry", "second", "ohm", "volt", "amp"};
  for (const auto unit : kUnits) {
    const std::string_view letter = unit.substr(0, 1);
    const std::string plural = std::string{unit} + "s";
    if (equal_ignoring_case(rest, letter) || equal_ignoring_case(rest, unit) || equal_ignoring_case(rest, plural)) {
      return true;
    }
  }
  return equal_ignoring_case(rest, "henries");
}

double decimal_value(const Mantissa& mantissa, int scale) {
  // One correctly rounded conversion of the exact decimal product.
  // (std::from_chars for double needs macOS 26; the core targets macOS 14.)
  const long exponent = mantissa.exponent + scale;
  const std::string text = mantissa.digits + "e" + std::to_string(exponent);
  return std::strtod(text.c_str(), nullptr);
}

std::size_t skip_spaces(std::string_view text, std::size_t pos) noexcept {
  while (pos < text.size() && is_space(text[pos])) {
    ++pos;
  }
  return pos;
}

}  // namespace

std::optional<double> parse_si_value(std::string_view text) noexcept {
  try {
    const auto mantissa = scan_mantissa(text);
    if (!mantissa) {
      return std::nullopt;
    }
    auto rest = text.substr(skip_spaces(text, mantissa->length));
    const Scale scale = scan_scale(rest);
    rest.remove_prefix(scale.length);
    if (!is_unit_word(rest)) {
      return std::nullopt;
    }
    if (!mantissa->decimal) {
      return mantissa->value * std::pow(10.0, scale.exponent);
    }
    return decimal_value(*mantissa, scale.exponent);
  } catch (...) {  // std::bad_alloc from the decimal text
    return std::nullopt;
  }
}

bool is_si_number(std::string_view text) noexcept {
  try {
    const auto mantissa = scan_mantissa(text);
    if (!mantissa) {
      return false;
    }
    const std::size_t next = skip_spaces(text, mantissa->length);
    if (next == text.size()) {
      return true;
    }
    switch (lower(text[next])) {
      case 'g':
      case 'k':
      case 'c':
      case 'm':
      case 'u':
      case 'n':
      case 'p':
      case 'f':
      case 'a': return true;
      default: return false;
    }
  } catch (...) {
    return false;
  }
}

}  // namespace hefesto::lvs
