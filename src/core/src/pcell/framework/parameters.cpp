// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/pcell/framework/parameters.hpp"

#include <algorithm>

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "pcell/framework/codes.hpp"

namespace hefesto::pcell {

namespace {

auto find_key(auto& entries, std::string_view key) {
  return std::find_if(entries.begin(), entries.end(), [key](const auto& entry) { return entry.first == key; });
}

}  // namespace

ParameterDict::ParameterDict(std::vector<Entry> entries) {
  for (auto& [key, value] : entries) {
    set(key, std::move(value));
  }
}

diag::Outcome<ParameterDict> ParameterDict::from_tcl(std::string_view text) {
  auto words = split_tcl_list(text);
  if (!words.ok()) {
    return words.error();
  }
  const auto& w = words.value();
  if (w.size() % 2 != 0) {
    return diag::make(codes::kListSyntax, "missing value to go with key");
  }
  ParameterDict dict;
  for (std::size_t i = 0; i < w.size(); i += 2) {
    dict.set(w[i], w[i + 1]);
  }
  dict.text_ = std::string{text};
  return dict;
}

std::string ParameterDict::to_tcl() const {
  if (text_) {
    return *text_;
  }
  std::vector<std::string> words;
  words.reserve(entries_.size() * 2);
  for (const auto& [key, value] : entries_) {
    words.push_back(key);
    words.push_back(value);
  }
  return merge_tcl_list(words);
}

void ParameterDict::set(std::string_view key, std::string value) {
  text_.reset();
  const auto it = find_key(entries_, key);
  if (it != entries_.end()) {
    it->second = std::move(value);
    return;
  }
  entries_.emplace_back(std::string{key}, std::move(value));
}

std::optional<std::string_view> ParameterDict::get(std::string_view key) const {
  const auto it = find_key(entries_, key);
  if (it == entries_.end()) {
    return std::nullopt;
  }
  return std::string_view{it->second};
}

bool ParameterDict::contains(std::string_view key) const {
  return find_key(entries_, key) != entries_.end();
}

void ParameterDict::erase(std::string_view key) {
  const auto it = find_key(entries_, key);
  if (it != entries_.end()) {
    entries_.erase(it);
    text_.reset();  // Tcl_DictObjRemove invalidates only when the key existed
  }
}

ParameterDict ParameterDict::merged(const ParameterDict& other) const {
  ParameterDict result = *this;
  for (const auto& [key, value] : other.entries_) {
    result.set(key, value);
  }
  return result;
}

}  // namespace hefesto::pcell
