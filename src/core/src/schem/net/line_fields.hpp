// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Field splitting and ASCII case helpers of the netlist line rules
// ("Element line post-processing", "Line breaking", "Flat netlist").
// Case-insensitive matching is ASCII, as gawk does in the C locale
// (Q-SCHEM-18). Internal to schem/net.
//
// Owner: NETLIST.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hefesto::schem::detail {

inline bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t';
}

/// The fields of a line: runs of characters other than space and tab.
inline std::vector<std::string_view> split_fields(std::string_view line) {
  std::vector<std::string_view> fields;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && is_blank(line[i])) {
      ++i;
    }
    const std::size_t start = i;
    while (i < line.size() && !is_blank(line[i])) {
      ++i;
    }
    if (i > start) {
      fields.push_back(line.substr(start, i - start));
    }
  }
  return fields;
}

inline char ascii_lower(char c) noexcept {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

inline std::string ascii_lower(std::string_view text) {
  std::string out{text};
  for (char& c : out) {
    c = ascii_lower(c);
  }
  return out;
}

inline bool iequals(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (ascii_lower(a[i]) != ascii_lower(b[i])) {
      return false;
    }
  }
  return true;
}

inline bool starts_with_ci(std::string_view text, std::string_view prefix) noexcept {
  return text.size() >= prefix.size() && iequals(text.substr(0, prefix.size()), prefix);
}

}  // namespace hefesto::schem::detail
