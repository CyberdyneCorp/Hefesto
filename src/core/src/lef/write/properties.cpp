// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell properties the LEF writer reads (capability lef-write; see
// properties.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c lefGetProperties and
// the PROPERTY loop of lefWriteMacro, and database/DBprop.c
// DBPropGetString, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include "lef/write/properties.hpp"

#include <variant>

#include "hefesto/db/property.hpp"

namespace hefesto::lef::write_detail {

namespace {

std::size_t skip_to_space(std::string_view s, std::size_t i) {
  while (i < s.size() && s[i] != ' ') {
    ++i;
  }
  return i;
}

std::size_t skip_spaces(std::string_view s, std::size_t i) {
  while (i < s.size() && s[i] == ' ') {
    ++i;
  }
  return i;
}

// The end of the value starting at `i` (a quoted string, closing quote
// included, or a word); npos for an unterminated quote.
std::size_t value_end(std::string_view s, std::size_t i) {
  if (s[i] != '"') {
    return skip_to_space(s, i);
  }
  const std::size_t close = s.find('"', i + 1);
  return close == std::string_view::npos ? std::string_view::npos : close + 1;
}

// lefGetProperties' value scan: an unquoted value starts one character in.
std::size_t magic_value_end(std::string_view s, std::size_t i) {
  return s[i] == '"' ? value_end(s, i) : skip_to_space(s, i + 1);
}

}  // namespace

std::optional<std::string> string_property(const db::CellDef& cell, std::string_view name) {
  const db::PropertyValue* value = cell.properties().find(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  const auto* text = std::get_if<std::string>(value);
  return text != nullptr ? std::optional<std::string>{*text} : std::nullopt;
}

std::optional<db::Rect> fixed_bbox(const db::CellDef& cell) {
  const db::PropertyValue* value = cell.properties().find("FIXED_BBOX");
  if (value == nullptr) {
    return std::nullopt;
  }
  const auto* dims = std::get_if<db::DimensionList>(value);
  if (dims == nullptr || dims->values.size() != 4) {
    return std::nullopt;
  }
  return db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
}

// lefWriteMacro, lefWrite.c:2040-2084.
std::vector<std::string> property_pairs(std::string_view value) {
  std::vector<std::string> pairs;
  std::size_t start = 0;
  while (start < value.size()) {
    std::size_t d = skip_spaces(value, skip_to_space(value, start));
    if (d >= value.size()) {
      break;
    }
    d = value_end(value, d);
    if (d == std::string_view::npos) {
      break;
    }
    pairs.emplace_back(value.substr(start, d - start));
    start = skip_spaces(value, d);
  }
  return pairs;
}

// lefGetProperties, lefWrite.c:2139-2200.
std::vector<std::string> property_keys(std::string_view value, bool magic_compat) {
  std::vector<std::string> keys;
  if (!magic_compat) {
    for (const std::string& pair : property_pairs(value)) {
      keys.push_back(pair.substr(0, pair.find(' ')));
    }
    return keys;
  }
  std::size_t i = 0;
  while (i < value.size()) {
    const std::size_t key_end = skip_to_space(value, i);
    if (key_end >= value.size()) {
      break;
    }
    keys.emplace_back(value.substr(i, key_end - i));
    const std::size_t v = skip_spaces(value, key_end + 1);
    if (v >= value.size()) {
      break;
    }
    const std::size_t end = magic_value_end(value, v);
    if (end == std::string_view::npos || end >= value.size()) {
      break;
    }
    i = end + 1;
  }
  return keys;
}

}  // namespace hefesto::lef::write_detail
