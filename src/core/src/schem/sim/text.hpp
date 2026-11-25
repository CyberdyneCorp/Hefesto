// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Line and word helpers of schem/sim (private to src/core/src/schem/sim/).
//
// Owner: SIMINT.

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::schem::sim_text {

[[nodiscard]] inline bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r';
}

[[nodiscard]] inline std::string lower(std::string_view text) {
  std::string out{text};
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

[[nodiscard]] inline std::string_view trim_left(std::string_view text) noexcept {
  while (!text.empty() && is_blank(text.front())) text.remove_prefix(1);
  return text;
}

[[nodiscard]] inline std::string_view trim(std::string_view text) noexcept {
  text = trim_left(text);
  while (!text.empty() && is_blank(text.back())) text.remove_suffix(1);
  return text;
}

/// The blank-separated words of `text`.
[[nodiscard]] inline std::vector<std::string_view> words(std::string_view text) {
  std::vector<std::string_view> out;
  for (text = trim_left(text); !text.empty(); text = trim_left(text)) {
    std::size_t end = 0;
    while (end < text.size() && !is_blank(text[end])) ++end;
    out.push_back(text.substr(0, end));
    text.remove_prefix(end);
  }
  return out;
}

[[nodiscard]] inline std::string_view first_word(std::string_view text) {
  const std::vector<std::string_view> all = words(text);
  return all.empty() ? std::string_view{} : all.front();
}

/// The parts of `text` between `separator` characters (at least one part).
[[nodiscard]] inline std::vector<std::string_view> split(std::string_view text, char separator) {
  std::vector<std::string_view> out;
  for (std::size_t at = text.find(separator); at != std::string_view::npos; at = text.find(separator)) {
    out.push_back(text.substr(0, at));
    text.remove_prefix(at + 1);
  }
  out.push_back(text);
  return out;
}

/// The LF-separated lines of `text`; a final LF ends the last line and
/// starts none.
[[nodiscard]] inline std::vector<std::string_view> lines(std::string_view text) {
  if (text.empty()) return {};
  if (text.back() == '\n') text.remove_suffix(1);
  return split(text, '\n');
}

}  // namespace hefesto::schem::sim_text
