// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Name tables and keyword matching (capability tech-layers-and-planes,
// "Name lookup by unique prefix", "Short and long name reporting"; the
// keyword matcher of utils/lookup.c used by compose rules and `tech`).
//
// Ported from Magic VLSI 8.3.694 database/DBtechtype.c (dbTechNameLookup,
// dbTechNameAdd) and utils/lookup.c (Lookup), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <cctype>
#include <climits>

#include "tech/technology_data.hpp"

namespace hefesto::tech {

namespace {

bool has_prefix(std::string_view name, std::string_view prefix) noexcept {
  return name.substr(0, prefix.size()) == prefix;
}

// Lower/upper-case-insensitive character match as in Lookup().
bool keyword_char_matches(char table_char, char text_char) noexcept {
  if (table_char == text_char) {
    return true;
  }
  const auto t = static_cast<unsigned char>(table_char);
  const auto s = static_cast<unsigned char>(text_char);
  return std::tolower(t) == std::tolower(s) && std::isalpha(t) != 0;
}

std::string_view strip_magic_namespace(std::string_view text) noexcept {
  for (std::string_view prefix : {std::string_view{"::magic::"}, std::string_view{"magic::"}}) {
    if (has_prefix(text, prefix)) {
      return text.substr(prefix.size());
    }
  }
  return text;
}

}  // namespace

int name_lookup(const NameTable& table, std::string_view name, bool same_value_resolves) {
  // Magic returns "not found" when the table has at most one entry (top == bot).
  if (table.size() <= 1) {
    return kNotFound;
  }
  const auto first = std::lower_bound(table.begin(), table.end(), name,
                                      [](const NameEntry& e, std::string_view n) { return e.name < n; });
  auto last = first;
  while (last != table.end() && has_prefix(last->name, name)) {
    ++last;
  }
  if (first == last) {
    return kNotFound;
  }
  if (last - first == 1 || first->name == name) {  // a sorted range starts with the exact match
    return first->value;
  }
  const bool same = std::all_of(first, last, [&](const NameEntry& e) { return e.value == first->value; });
  return same_value_resolves && same ? first->value : kAmbiguous;
}

int name_lookup_exact(const NameTable& table, std::string_view name) {
  const auto it = std::find_if(table.begin(), table.end(), [&](const NameEntry& e) { return e.name == name; });
  return it == table.end() ? kNotFound : it->value;
}

std::vector<std::string> split_name_list(std::string_view names) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start < names.size()) {
    const auto comma = names.find(',', start);
    const auto end = comma == std::string_view::npos ? names.size() : comma;
    if (end > start) {
      out.emplace_back(names.substr(start, end - start));
    }
    start = end + 1;
  }
  return out;
}

std::string first_duplicate(const NameTable& table, const std::vector<std::string>& names) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    const bool repeated = std::find(names.begin(), names.begin() + static_cast<std::ptrdiff_t>(i), names[i]) !=
                          names.begin() + static_cast<std::ptrdiff_t>(i);
    if (repeated || name_lookup_exact(table, names[i]) != kNotFound) {
      return names[i];
    }
  }
  return {};
}

std::string add_names(NameTable& table, const std::vector<std::string>& names, int value, bool alias) {
  std::size_t primary = names.size();
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (primary == names.size() || names[i].size() < names[primary].size()) {
      primary = i;
    }
  }
  for (std::size_t i = 0; i < names.size(); ++i) {
    NameEntry entry{names[i], value, i == primary && !alias, alias};
    const auto pos = std::lower_bound(table.begin(), table.end(), entry.name,
                                      [](const NameEntry& e, const std::string& n) { return e.name < n; });
    table.insert(pos, std::move(entry));
  }
  return names.empty() ? std::string{} : names.front();
}

const std::string* primary_name(const NameTable& table, int value) {
  for (const auto& e : table) {
    if (e.value == value && e.primary) {
      return &e.name;
    }
  }
  return nullptr;
}

bool str_is_int(std::string_view text) noexcept {
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    text.remove_prefix(1);
  }
  return std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

long long str_to_int(std::string_view text) noexcept {
  bool negative = false;
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  long long value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') {
      break;
    }
    if (value > (LLONG_MAX - 9) / 10) {
      return negative ? LLONG_MIN : LLONG_MAX;
    }
    value = value * 10 + (c - '0');
  }
  return negative ? -value : value;
}

int keyword_lookup(std::string_view text, const std::vector<std::string_view>& table, bool strip_namespace) {
  if (strip_namespace) {
    text = strip_magic_namespace(text);
  }
  int match = kNotFound;
  for (std::size_t pos = 0; pos < table.size(); ++pos) {
    const std::string_view entry = table[pos];
    std::size_t i = 0;
    while (i < text.size() && i < entry.size() && entry[i] != ' ' && keyword_char_matches(entry[i], text[i])) {
      ++i;
    }
    if (i < text.size()) {
      continue;
    }
    if (i == entry.size() || entry[i] == ' ') {
      return static_cast<int>(pos);  // exact match
    }
    match = match == kNotFound ? static_cast<int>(pos) : kAmbiguous;
  }
  return match;
}

}  // namespace hefesto::tech
