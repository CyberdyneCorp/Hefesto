// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Numeric fields (schematic-file-format "Numeric fields"); see numbers.hpp.
//
// Owner: FORMAT.

#include "numbers.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace hefesto::schem::file_detail {

namespace {

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

// The bytes a C number can be made of; strtod sees at most this run.
bool is_number_byte(char c) noexcept {
  return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '+' || c == '-' ||
         c == '(' || c == ')' || c == '_';
}

}  // namespace

std::optional<Parsed<double>> parse_c_double(std::string_view text) {
  // std::from_chars for double needs macOS 26; strtod in the "C" locale (the
  // only one Hefesto runs in: it never calls setlocale) is the reference
  // the spec names. It reads a NUL-terminated copy of the candidate run.
  std::size_t run = 0;
  while (run < text.size() && is_number_byte(text[run])) {
    ++run;
  }
  if (run == 0) {
    return std::nullopt;
  }
  const std::string copy{text.substr(0, run)};
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  const auto used = static_cast<std::size_t>(end - copy.c_str());
  if (used == 0) {
    return std::nullopt;
  }
  return Parsed<double>{value, used};
}

std::optional<Parsed<int>> parse_c_int(std::string_view text) {
  std::size_t i = 0;
  bool negative = false;
  if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
    negative = text[i] == '-';
    ++i;
  }
  const std::size_t first_digit = i;
  constexpr std::int64_t kLimit = static_cast<std::int64_t>(std::numeric_limits<int>::max()) + 1;
  std::int64_t value = 0;
  for (; i < text.size() && is_digit(text[i]); ++i) {
    value = value * 10 + (text[i] - '0');
    if (value > kLimit) {
      return std::nullopt;
    }
  }
  if (i == first_digit || (!negative && value == kLimit)) {
    return std::nullopt;
  }
  return Parsed<int>{static_cast<int>(negative ? -value : value), i};
}

std::string format_g16(double value) {
  if (std::isnan(value)) {
    return std::signbit(value) ? "-nan" : "nan";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-inf" : "inf";
  }
  std::array<char, 64> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                    std::chars_format::general, 16);
  return {buffer.data(), result.ptr};
}

std::string format_int(int value) {
  std::array<char, 16> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  return {buffer.data(), result.ptr};
}

}  // namespace hefesto::schem::file_detail
