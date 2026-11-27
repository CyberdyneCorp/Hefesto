// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Style lookup (capability router-maze "Style defaults and lookup"): styles
// are found by exact name. Magic prepends each new style to mzStyles
// (mzTechStyle) and MZFindStyle walks from the head, so of two styles with
// the same name the last one in the file is found; `styles` is in file
// order, so it is searched from the back (Q-LEGACY-107, replicated in both
// modes). The reader is in reader.cpp, the DRC derivation in derive.cpp and
// the derived `irouter` style in derived_style.cpp.
//
// Ported from Magic VLSI 8.3.694 mzrouter/mzMain.c (MZFindStyle) and
// mzrouter/mzTech.c (mzTechStyle), Copyright (C) 1988, 1990 Michael H.
// Arnold and the Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: MZTECH.

#include "hefesto/route/tech/styles.hpp"

#include <algorithm>

namespace hefesto::route {

const MazeStyle* find_style(const MazeStyles& styles, std::string_view name) {
  const auto it = std::find_if(styles.styles.rbegin(), styles.styles.rend(),
                               [name](const MazeStyle& s) { return s.name == name; });
  return it == styles.styles.rend() ? nullptr : &*it;
}

}  // namespace hefesto::route
