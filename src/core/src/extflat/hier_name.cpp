// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchical names (see hier_name.hpp).
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#include "hier_name.hpp"

#include <algorithm>

namespace hefesto::extflat::detail {

std::string name_from_string(std::string_view s) {
  std::string key{s};
  std::replace(key.begin(), key.end(), '/', kSep);
  return key;
}

std::string concat_names(std::string_view prefix, std::string_view suffix) {
  if (prefix.empty()) {
    return std::string{suffix};
  }
  std::string key;
  key.reserve(prefix.size() + 1 + suffix.size());
  key.append(prefix);
  key.push_back(kSep);
  key.append(suffix);
  return key;
}

std::string name_to_string(std::string_view key, char sep) {
  std::string out{key};
  std::replace(out.begin(), out.end(), kSep, sep);
  return out;
}

std::vector<std::string> name_components(std::string_view key) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t end = key.find(kSep, start);
    if (end == std::string_view::npos) {
      out.emplace_back(key.substr(start));
      return out;
    }
    out.emplace_back(key.substr(start, end - start));
    start = end + 1;
  }
}

std::string_view name_leaf(std::string_view key) noexcept {
  const std::size_t sep = key.rfind(kSep);
  return sep == std::string_view::npos ? key : key.substr(sep + 1);
}

std::size_t name_component_count(std::string_view key) noexcept {
  return static_cast<std::size_t>(std::count(key.begin(), key.end(), kSep)) + 1;
}

std::uint32_t component_hash(std::string_view component) noexcept {
  std::uint32_t sum = 0;
  for (const char c : component) {
    const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(c)));
    sum = ((sum >> 28U) | (sum << 4U)) + value;
  }
  return sum;
}

std::string use_component(std::string_view use_id, bool has_x, bool has_y, std::int32_t x, std::int32_t y) {
  std::string out{use_id};
  if (!has_x && !has_y) {
    return out;
  }
  out.push_back('[');
  if (has_y) {
    out += std::to_string(y);
  }
  if (has_x) {
    if (has_y) {
      out.push_back(',');
    }
    out += std::to_string(x);
  }
  out.push_back(']');
  return out;
}

namespace {

char last_char(std::string_view key) noexcept {
  const std::string_view leaf = name_leaf(key);
  return leaf.empty() ? '\0' : leaf.back();
}

// Rule 1: unless both leaves end in '!', a label beats a generated name.
int label_rule(char last_a, char last_b) noexcept {
  if (last_a == '!' && last_b == '!') {
    return 0;
  }
  if (last_a != '#' && last_b == '#') {
    return 1;
  }
  if (last_a == '#' && last_b != '#') {
    return -1;
  }
  return 0;
}

}  // namespace

bool prefer_name(std::string_view a, std::string_view b) noexcept {
  if (const int r = label_rule(last_char(a), last_char(b)); r != 0) {
    return r > 0;
  }
  const std::size_t na = name_component_count(a);
  const std::size_t nb = name_component_count(b);
  if (na != nb) {
    return na < nb;
  }
  if (na == 1 && a == "0") {
    return false;
  }
  if (nb == 1 && b == "0") {
    return true;
  }
  // Characters summed over the components: the separators do not count.
  const std::size_t la = a.size() - (na - 1);
  const std::size_t lb = b.size() - (nb - 1);
  if (la != lb) {
    return la < lb;
  }
  return a.compare(b) > 0;
}

}  // namespace hefesto::extflat::detail
