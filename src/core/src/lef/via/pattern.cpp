// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF 5.8 via cut patterns (`+ PATTERN`), applied with magic-compat OFF only
// (capability def-read "VIAS section grammar", Q-LEFDEF-62). Magic does not
// read them.
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "hefesto/lef/via/generate.hpp"

namespace hefesto::lef {

namespace {

std::vector<std::string_view> split(std::string_view text, char sep) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == sep) {
      parts.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  return parts;
}

std::optional<std::int32_t> count_of(std::string_view text) {
  std::int32_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value <= 0) {
    return std::nullopt;
  }
  return value;
}

std::optional<int> hex_digit(char c) {
  if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
    return std::nullopt;
  }
  return std::isdigit(static_cast<unsigned char>(c)) != 0 ? c - '0' : std::toupper(static_cast<unsigned char>(c)) - 'A' + 10;
}

void push_bits(int digit, std::vector<bool>& bits) {
  for (int bit = 3; bit >= 0; --bit) {
    bits.push_back(((digit >> bit) & 1) != 0);
  }
}

/// The repeat count at `i`: 1, or k for an `R<k>` prefix, after which `i`
/// is at the repeated digit; nullopt when the prefix is malformed or ends
/// the row.
std::optional<std::int32_t> repeat_at(std::string_view row, std::size_t& i) {
  if (row[i] != 'R' && row[i] != 'r') {
    return 1;
  }
  std::size_t end = i + 1;
  while (end < row.size() && std::isdigit(static_cast<unsigned char>(row[end])) != 0) {
    ++end;
  }
  const auto k = count_of(row.substr(i + 1, end - i - 1));
  if (!k || end >= row.size()) {
    return std::nullopt;
  }
  i = end;
  return k;
}

/// The bits of one row definition: hex digits, `R<k><digit>` repeating;
/// nullopt past `max_bits` (a row is at most the columns rounded up to 4).
std::optional<std::vector<bool>> row_bits(std::string_view row, std::size_t max_bits) {
  std::vector<bool> bits;
  std::size_t i = 0;
  while (i < row.size()) {
    const auto repeat = repeat_at(row, i);
    if (!repeat) {
      return std::nullopt;
    }
    const auto digit = hex_digit(row[i]);
    if (!digit || bits.size() + 4 * static_cast<std::size_t>(*repeat) > max_bits) {
      return std::nullopt;
    }
    for (std::int32_t r = 0; r < *repeat; ++r) {
      push_bits(*digit, bits);
    }
    ++i;
  }
  return bits;
}

}  // namespace

std::optional<std::vector<std::vector<bool>>> cut_pattern(std::string_view pattern, std::int32_t rows,
                                                          std::int32_t columns) {
  const std::vector<std::string_view> parts = split(pattern, '_');
  if (parts.size() % 2 != 0 || rows < 0 || columns < 0) {
    return std::nullopt;
  }
  std::vector<std::vector<bool>> present;
  for (std::size_t g = 0; g < parts.size(); g += 2) {
    const auto n = count_of(parts[g]);
    auto bits = row_bits(parts[g + 1], static_cast<std::size_t>(columns) + 3);
    if (!n || !bits || bits->size() < static_cast<std::size_t>(columns) ||
        present.size() + static_cast<std::size_t>(*n) > static_cast<std::size_t>(rows)) {
      return std::nullopt;
    }
    bits->resize(static_cast<std::size_t>(columns));
    present.insert(present.end(), static_cast<std::size_t>(*n), *bits);
  }
  if (present.size() != static_cast<std::size_t>(rows)) {
    return std::nullopt;
  }
  return present;
}

}  // namespace hefesto::lef
