// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF 5.8 via cut patterns (`+ PATTERN`), applied with magic-compat OFF only
// (capability def-read "VIAS section grammar", Q-LEFDEF-62). Magic does not
// read them.
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
#include <utility>
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

/// One row definition: hex digits, `R<k><digit>` repeating, as runs of
/// digits (no expansion, so a long repeat costs nothing); nullopt past
/// `max_bits` (a row is at most the columns rounded up to 4).
std::optional<std::vector<CutPattern::Run>> row_runs(std::string_view row, std::uint64_t max_bits) {
  std::vector<CutPattern::Run> runs;
  std::uint64_t bits = 0;
  std::size_t i = 0;
  while (i < row.size()) {
    const auto repeat = repeat_at(row, i);
    if (!repeat) {
      return std::nullopt;
    }
    const auto digit = hex_digit(row[i]);
    if (!digit || bits + 4 * static_cast<std::uint64_t>(*repeat) > max_bits) {
      return std::nullopt;
    }
    bits += 4 * static_cast<std::uint64_t>(*repeat);
    runs.push_back(CutPattern::Run{bits, static_cast<std::uint8_t>(*digit)});
    ++i;
  }
  return runs;
}

}  // namespace

bool CutPattern::at(std::int32_t row, std::int32_t column) const {
  const auto r = static_cast<std::uint64_t>(row);
  const auto c = static_cast<std::uint64_t>(column);
  const auto group = std::upper_bound(groups.begin(), groups.end(), r,
                                      [](std::uint64_t v, const Group& g) { return v < g.end_row; });
  if (group == groups.end()) {
    return false;
  }
  const auto run = std::upper_bound(group->runs.begin(), group->runs.end(), c,
                                    [](std::uint64_t v, const Run& one) { return v < one.end_bit; });
  if (run == group->runs.end()) {
    return false;
  }
  const std::uint64_t bit = c % 4;  // every run starts on a digit boundary
  return ((run->digit >> (3 - bit)) & 1U) != 0;
}

std::optional<CutPattern> cut_pattern(std::string_view pattern, std::int32_t rows, std::int32_t columns) {
  const std::vector<std::string_view> parts = split(pattern, '_');
  if (parts.size() % 2 != 0 || rows < 0 || columns < 0) {
    return std::nullopt;
  }
  CutPattern present;
  std::uint64_t covered = 0;
  for (std::size_t g = 0; g < parts.size(); g += 2) {
    const auto n = count_of(parts[g]);
    auto runs = row_runs(parts[g + 1], static_cast<std::uint64_t>(columns) + 3);
    if (!n || !runs || (runs->empty() ? 0 : runs->back().end_bit) < static_cast<std::uint64_t>(columns) ||
        covered + static_cast<std::uint64_t>(*n) > static_cast<std::uint64_t>(rows)) {
      return std::nullopt;
    }
    covered += static_cast<std::uint64_t>(*n);
    present.groups.push_back(CutPattern::Group{covered, *std::move(runs)});
  }
  if (covered != static_cast<std::uint64_t>(rows)) {
    return std::nullopt;
  }
  return present;
}

}  // namespace hefesto::lef
