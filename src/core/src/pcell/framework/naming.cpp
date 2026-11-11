// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Ports of magic::normalize_value, magic::murmur3_32, magic::get_gencell_hash
// and magic::get_gencell_name (Magic ref: tcltk/toolkit.tcl:1133-1198,
// 1778-1869), with the Tcl 8.6 number grammar of generic/tclStrToD.c
// TclParseNumber (flags 0) that `string is double -strict` applies.

#include "hefesto/pcell/framework/naming.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace hefesto::pcell {

namespace {

bool is_space(char c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

char lower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool starts_with_nocase(std::string_view s, std::string_view word) {
  if (s.size() < word.size()) {
    return false;
  }
  for (std::size_t i = 0; i < word.size(); ++i) {
    if (lower(s[i]) != word[i]) {
      return false;
    }
  }
  return true;
}

int digit_value(char c, int radix) {
  int d = -1;
  if (is_digit(c)) {
    d = c - '0';
  } else if (lower(c) >= 'a' && lower(c) <= 'f') {
    d = lower(c) - 'a' + 10;
  }
  return d < radix ? d : -1;
}

// A number body (after white space and sign): its value and length, or
// nullopt when no number starts here.
struct Body {
  double value = 0.0;
  std::size_t length = 0;
};

// Digits of `radix` from s[start]; at least one is required.
std::optional<Body> radix_integer(std::string_view s, std::size_t start, int radix) {
  std::size_t i = start;
  std::uint64_t exact = 0;
  double approx = 0.0;
  bool overflow = false;
  while (i < s.size() && digit_value(s[i], radix) >= 0) {
    const auto d = static_cast<unsigned>(digit_value(s[i], radix));
    const auto r = static_cast<unsigned>(radix);
    overflow = overflow || exact > (std::numeric_limits<std::uint64_t>::max() - d) / r;
    exact = exact * r + d;
    approx = approx * radix + d;
    ++i;
  }
  if (i == start) {
    return std::nullopt;
  }
  return Body{overflow ? approx : static_cast<double>(exact), i};
}

std::size_t skip_digits(std::string_view s, std::size_t i) {
  while (i < s.size() && is_digit(s[i])) {
    ++i;
  }
  return i;
}

// The decimal (or BAD_OCTAL-turned-decimal) form: digits [. digits] [exp]
// or . digits [exp]; the accepted prefix, as TclParseNumber's accept point.
std::optional<std::size_t> decimal_length(std::string_view s) {
  std::size_t i = skip_digits(s, 0);
  const bool int_digits = i > 0;
  std::size_t accepted = i;
  if (i < s.size() && s[i] == '.') {
    const std::size_t frac_end = skip_digits(s, i + 1);
    if (!int_digits && frac_end == i + 1) {
      return std::nullopt;  // a lone "."
    }
    i = frac_end;
    accepted = i;
  }
  if (accepted == 0) {
    return std::nullopt;
  }
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    std::size_t j = i + 1;
    if (j < s.size() && (s[j] == '+' || s[j] == '-')) {
      ++j;
    }
    const std::size_t exp_end = skip_digits(s, j);
    if (exp_end > j) {
      accepted = exp_end;
    }
  }
  return accepted;
}

bool all_octal(std::string_view digits) {
  return std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '7'; });
}

std::optional<Body> decimal_body(std::string_view s) {
  const auto length = decimal_length(s);
  if (!length) {
    return std::nullopt;
  }
  const std::string_view text = s.substr(0, *length);
  // Leading 0 followed only by digits: a legacy octal integer (an 8 or 9
  // makes it BAD_OCTAL, which only a fraction or exponent can rescue).
  if (text.size() > 1 && text.front() == '0' && text.find_first_not_of("0123456789") == std::string_view::npos) {
    if (!all_octal(text)) {
      return Body{0.0, 1};  // accept point stays after the "0"
    }
    return radix_integer(text, 1, 8);
  }
  const std::string copy{text};
  return Body{std::strtod(copy.c_str(), nullptr), *length};
}

std::optional<Body> nan_body(std::string_view s) {
  std::size_t i = 3;
  if (i < s.size() && s[i] == '(') {
    std::size_t j = i + 1;
    while (j < s.size() && (is_space(s[j]) || digit_value(s[j], 16) >= 0)) {
      ++j;
    }
    if (j < s.size() && s[j] == ')') {
      i = j + 1;
    }
  }
  return Body{std::numeric_limits<double>::quiet_NaN(), i};
}

std::optional<Body> prefixed_body(std::string_view s) {
  const char p = lower(s[1]);
  const int radix = p == 'x' ? 16 : (p == 'b' ? 2 : (p == 'o' ? 8 : 0));
  if (radix == 0) {
    return std::nullopt;
  }
  if (auto body = radix_integer(s, 2, radix)) {
    return body;
  }
  return Body{0.0, 1};  // "0x" alone: only the "0" is a number
}

std::optional<Body> number_body(std::string_view s) {
  if (starts_with_nocase(s, "infinity")) {
    return Body{std::numeric_limits<double>::infinity(), 8};
  }
  if (starts_with_nocase(s, "inf")) {
    return Body{std::numeric_limits<double>::infinity(), 3};
  }
  if (starts_with_nocase(s, "nan")) {
    return nan_body(s);
  }
  if (s.size() > 1 && s[0] == '0') {
    if (auto prefixed = prefixed_body(s)) {
      return prefixed;
    }
  }
  return decimal_body(s);
}

// Tcl_UtfToUniChar over `text` (TCL_UTF_MAX 3): UTF-16 units.
std::vector<std::uint32_t> tcl_chars(std::string_view text) {
  std::vector<std::uint32_t> units;
  std::size_t i = 0;
  const auto byte = [&](std::size_t k) { return static_cast<std::uint32_t>(static_cast<unsigned char>(text[k])); };
  const auto cont = [&](std::size_t k) { return k < text.size() && (byte(k) & 0xC0) == 0x80; };
  while (i < text.size()) {
    const std::uint32_t b = byte(i);
    if (b >= 0xC0 && b < 0xE0 && cont(i + 1)) {
      units.push_back(((b & 0x1F) << 6) | (byte(i + 1) & 0x3F));
      i += 2;
    } else if (b >= 0xE0 && b < 0xF0 && cont(i + 1) && cont(i + 2)) {
      units.push_back(((b & 0x0F) << 12) | ((byte(i + 1) & 0x3F) << 6) | (byte(i + 2) & 0x3F));
      i += 3;
    } else if (b >= 0xF0 && b < 0xF5 && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
      const std::uint32_t cp = ((b & 0x07) << 18) | ((byte(i + 1) & 0x3F) << 12) | ((byte(i + 2) & 0x3F) << 6) |
                               (byte(i + 3) & 0x3F);
      units.push_back(0xD800 + ((cp - 0x10000) >> 10));
      units.push_back(0xDC00 + ((cp - 0x10000) & 0x3FF));
      i += 4;
    } else {
      units.push_back(b);  // ASCII or a stray byte (Latin-1)
      ++i;
    }
  }
  return units;
}

std::uint32_t rotl(std::uint32_t x, int r) {
  return (x << r) | (x >> (32 - r));
}

std::uint32_t mix_block(std::uint32_t k) {
  k *= 0xcc9e2d51U;
  k = rotl(k, 15);
  return k * 0x1b873593U;
}

// format %d of int(round(x)) for an integral double: Tcl 8.6 keeps the low
// 64 bits of a value beyond the long range.
std::string integer_text(double r) {
  std::int64_t v = 0;
  if (std::fabs(r) < 9.2233720368547758e18) {
    v = static_cast<std::int64_t>(r);
  } else {
    double m = std::fmod(r, 18446744073709551616.0);
    if (m < 0) {
      m += 18446744073709551616.0;
    }
    v = static_cast<std::int64_t>(static_cast<std::uint64_t>(m));
  }
  return std::to_string(v);
}

// Tcl's round(): truncate, then step away from zero when |fraction| >= 0.5.
double tcl_round(double x) {
  double int_part = 0.0;
  const double fract = std::modf(x, &int_part);
  if (fract <= -0.5) {
    return int_part - 1.0;
  }
  return fract >= 0.5 ? int_part + 1.0 : int_part;
}

// regsub {\.?0+$} $str "": delete the leftmost match.
std::string strip_trailing_zeros(std::string s) {
  for (std::size_t i = 0; i < s.size(); ++i) {
    const std::size_t zeros = s[i] == '.' ? i + 1 : i;
    if (zeros < s.size() && s.find_first_not_of('0', zeros) == std::string::npos) {
      s.erase(i);
      break;
    }
  }
  return s;
}

}  // namespace

std::optional<double> tcl_strict_double(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size() && is_space(text[i])) {
    ++i;
  }
  bool negative = false;
  if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
    negative = text[i] == '-';
    ++i;
  }
  const auto body = i < text.size() ? number_body(text.substr(i)) : std::nullopt;
  if (!body) {
    return std::nullopt;
  }
  std::size_t end = i + body->length;
  while (end < text.size() && is_space(text[end])) {
    ++end;
  }
  if (end != text.size()) {
    return std::nullopt;
  }
  return negative ? -body->value : body->value;
}

std::string normalize_value(std::string_view value) {
  const auto number = tcl_strict_double(value);
  if (!number || !std::isfinite(*number)) {
    return std::string{value};  // non-numeric, or Inf/NaN verbatim (Q-SCRIPT-21)
  }
  const double rounded = tcl_round(*number);
  if (std::fabs(*number - rounded) < 1e-9) {
    return integer_text(rounded);
  }
  std::array<char, 400> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.3f", *number);
  return strip_trailing_zeros(buffer.data());
}

std::uint32_t gencell_hash(std::string_view text, std::uint32_t seed) {
  const std::vector<std::uint32_t> chars = tcl_chars(text);
  const std::size_t length = chars.size();
  const auto low = [&](std::size_t k) { return chars[k] & 0xFFU; };
  std::uint32_t h = seed;
  const std::size_t blocks = length / 4;
  for (std::size_t b = 0; b < blocks; ++b) {
    const std::size_t k = b * 4;
    h ^= mix_block(low(k) | (low(k + 1) << 8) | (low(k + 2) << 16) | (low(k + 3) << 24));
    h = rotl(h, 13);
    h = h * 5 + 0xe6546b64U;
  }
  const std::size_t tail = blocks * 4;
  std::uint32_t k1 = 0;
  switch (length - tail) {
    case 3: k1 ^= low(tail + 2) << 16; [[fallthrough]];
    case 2: k1 ^= low(tail + 1) << 8; [[fallthrough]];
    case 1: k1 ^= low(tail); h ^= mix_block(k1); break;
    default: break;
  }
  h ^= static_cast<std::uint32_t>(length);
  h ^= h >> 16;
  h *= 0x85ebca6bU;
  h ^= h >> 13;
  h *= 0xc2b2ae35U;
  h ^= h >> 16;
  return h;
}

std::string gencell_suffix(const ParameterDict& parameters) {
  std::vector<const ParameterDict::Entry*> sorted;
  for (const auto& entry : parameters.entries()) {
    sorted.push_back(&entry);
  }
  std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) { return a->first < b->first; });
  std::string input;
  for (const auto* entry : sorted) {
    input += normalize_value(entry->second);
    input += ';';
  }
  static constexpr std::string_view kAlphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
  const std::uint32_t hash = gencell_hash(input);
  std::string suffix;
  for (int i = 0; i < 6; ++i) {
    suffix += kAlphabet[(hash >> (5 * i)) & 31U];
  }
  return suffix;
}

std::string generated_cell_name(std::string_view device, const ParameterDict& parameters) {
  return std::string{device} + "_" + gencell_suffix(parameters);
}

std::string random_gencell_name(std::string_view device, const std::function<int(int)>& random_index,
                                const std::function<bool(const std::string&)>& exists) {
  while (true) {
    std::string name = std::string{device} + "_";
    for (int i = 0; i < 6; ++i) {
      int c = 48 + random_index(36);
      if (c > 57) {
        c += 39;
      }
      name += static_cast<char>(c);
    }
    if (!exists(name)) {
      return name;
    }
  }
}

}  // namespace hefesto::pcell
