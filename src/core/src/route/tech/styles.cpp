// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Style lookup (capability router-maze "Style defaults and lookup"): styles
// are found by exact name. The reader is in reader.cpp, the DRC derivation
// in derive.cpp and the derived `irouter` style in derived_style.cpp.
//
// Owner: MZTECH.

#include "hefesto/route/tech/styles.hpp"

#include <algorithm>

namespace hefesto::route {

const MazeStyle* find_style(const MazeStyles& styles, std::string_view name) {
  const auto it =
      std::find_if(styles.styles.begin(), styles.styles.end(), [name](const MazeStyle& s) { return s.name == name; });
  return it == styles.styles.end() ? nullptr : &*it;
}

}  // namespace hefesto::route
