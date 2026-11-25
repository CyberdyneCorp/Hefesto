// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Name folding and equality (lvs-netlist-input "Name matching and case
// sensitivity"; Q-LVS-3: the table has 256 entries and is indexed by
// unsigned byte in both netgen-compat modes).
//
// Owner: LVSREAD.

#include "hefesto/lvs/net/names.hpp"

#include <array>
#include <cstddef>

namespace hefesto::lvs {

namespace {

constexpr std::array<unsigned char, 256> make_fold_table() noexcept {
  std::array<unsigned char, 256> table{};
  for (std::size_t i = 0; i < table.size(); ++i) {
    table.at(i) = static_cast<unsigned char>(i);
  }
  for (std::size_t c = 'a'; c <= 'z'; ++c) {
    table.at(c) = static_cast<unsigned char>(c - 'a' + 'A');
  }
  for (const char open : {'[', '{', '<'}) {
    table.at(static_cast<unsigned char>(open)) = '<';
  }
  for (const char close : {']', '}', '>'}) {
    table.at(static_cast<unsigned char>(close)) = '>';
  }
  return table;
}

constexpr auto kFold = make_fold_table();

/// A Verilog escaped name without its backslash and one trailing space.
std::string_view unescaped(std::string_view name) noexcept {
  if (!name.starts_with('\\')) {
    return name;
  }
  name.remove_prefix(1);
  if (name.ends_with(' ')) {
    name.remove_suffix(1);
  }
  return name;
}

bool folded_equal(std::string_view first, std::string_view second) noexcept {
  if (first.size() != second.size()) {
    return false;
  }
  for (std::size_t i = 0; i < first.size(); ++i) {
    if (fold_name_byte(static_cast<unsigned char>(first[i])) != fold_name_byte(static_cast<unsigned char>(second[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

unsigned char fold_name_byte(unsigned char byte) noexcept {
  return kFold.at(byte);
}

bool names_equal(std::string_view first, std::string_view second, bool case_insensitive) noexcept {
  if (first.empty() || second.empty()) {
    return false;
  }
  if (!case_insensitive) {
    return first == second;
  }
  if (first.starts_with('\\') != second.starts_with('\\')) {
    return folded_equal(unescaped(first), unescaped(second));
  }
  return folded_equal(first, second);
}

std::string name_key(std::string_view name) {
  const auto plain = unescaped(name);
  std::string key;
  key.reserve(plain.size());
  for (const char c : plain) {
    key.push_back(static_cast<char>(fold_name_byte(static_cast<unsigned char>(c))));
  }
  return key;
}

bool name_starts_with(std::string_view name, std::string_view prefix, bool case_insensitive) noexcept {
  if (prefix.size() > name.size()) {
    return false;
  }
  const auto head = name.substr(0, prefix.size());
  return case_insensitive ? folded_equal(head, prefix) : head == prefix;
}

}  // namespace hefesto::lvs
