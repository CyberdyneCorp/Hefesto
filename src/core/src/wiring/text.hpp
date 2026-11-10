// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the C string tests the wiring area reproduces from Magic
// (utils/utils.c StrIsInt / StrIsNumeric, libc atoi). Owner: WIRE.

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

namespace hefesto::wiring {

/// StrIsInt: an optional sign, then only digits (an empty rest passes, as
/// in Magic).
[[nodiscard]] inline bool str_is_int(std::string_view text) noexcept {
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    text.remove_prefix(1);
  }
  return std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

/// StrIsNumeric: strtod reads a number at the start of the text.
[[nodiscard]] inline bool str_is_numeric(std::string_view text) {
  const std::string copy{text};
  char* end = nullptr;
  (void)std::strtod(copy.c_str(), &end);
  return end != copy.c_str();
}

/// atoi, saturated at the int32 range instead of undefined on overflow.
[[nodiscard]] inline std::int32_t c_atoi(std::string_view text) noexcept {
  std::size_t i = 0;
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0) {
    ++i;
  }
  bool negative = false;
  if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
    negative = text[i] == '-';
    ++i;
  }
  std::int64_t value = 0;
  constexpr std::int64_t kLimit = std::int64_t{std::numeric_limits<std::int32_t>::max()} + 1;
  for (; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0; ++i) {
    value = std::min(value * 10 + (text[i] - '0'), kLimit);
  }
  value = negative ? -value : std::min(value, kLimit - 1);
  return static_cast<std::int32_t>(std::max<std::int64_t>(value, std::numeric_limits<std::int32_t>::min()));
}

}  // namespace hefesto::wiring
