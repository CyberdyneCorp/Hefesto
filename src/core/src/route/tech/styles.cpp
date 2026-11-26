// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): read_mzrouter_section
// returns no style and derive_after_load changes nothing until MZTECH
// (tasks 9.1-9.3) lands them; find_style is final.
//
// Owner: MZTECH.

#include "hefesto/route/tech/styles.hpp"

#include <algorithm>

namespace hefesto::route {

MazeStyles read_mzrouter_section(const tech::RawSection* /*section*/, const tech::Technology& /*tech*/,
                                 diag::DiagnosticSink& /*sink*/) {
  return MazeStyles{};
}

void derive_after_load(MazeStyles& /*styles*/, const tech::Technology& /*tech*/, const drc::RuleSet* /*rules*/,
                       const lef::LayerTable& /*lef_layers*/, bool /*magic_compat*/, diag::DiagnosticSink& /*sink*/) {}

const MazeStyle* find_style(const MazeStyles& styles, std::string_view name) {
  const auto it =
      std::find_if(styles.styles.begin(), styles.styles.end(), [name](const MazeStyle& s) { return s.name == name; });
  return it == styles.styles.end() ? nullptr : &*it;
}

}  // namespace hefesto::route
