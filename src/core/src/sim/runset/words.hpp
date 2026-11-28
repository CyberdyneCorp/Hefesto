// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Whitespace-separated words of a deck line (sim/deck's spice_tokens also
// splits at `=`, `(`, `)` and `,`, which a line rewritten from its words
// must keep).
//
// Owner: SIMADV.

#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::sim::runset_text {

inline std::vector<std::string> words(std::string_view line) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
      ++i;
    }
    const std::size_t begin = i;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) == 0) {
      ++i;
    }
    if (i > begin) {
      out.emplace_back(line.substr(begin, i - begin));
    }
  }
  return out;
}

inline std::string join(const std::vector<std::string>& words) {
  std::string out;
  for (const std::string& w : words) {
    out += (out.empty() ? "" : " ") + w;
  }
  return out;
}

}  // namespace hefesto::sim::runset_text
