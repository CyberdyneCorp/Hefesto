// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A technology section body split into raw lines, as the technology reader
// keeps raw sections (tech::RawSection), for the M10 plot/route section
// fuzz targets (fuzz_plot_section, fuzz_mzrouter_section).
//
// Owner: SCAFFOLD (m10-plot-route tasks.md 3.1).

#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/tech/technology.hpp"

namespace hefesto::fuzz {

inline tech::RawSection raw_section(std::string name, std::string_view text, const std::string& file) {
  tech::RawSection section{std::move(name), {}};
  std::istringstream in{std::string{text}};
  std::string line;
  std::uint32_t number = 0;
  while (std::getline(in, line)) {
    tech::RawLine raw{{}, file, ++number};
    std::istringstream words{line};
    std::string word;
    while (words >> word) {
      raw.tokens.push_back(word);
    }
    if (!raw.tokens.empty()) {
      section.lines.push_back(std::move(raw));
    }
  }
  return section;
}

}  // namespace hefesto::fuzz
