// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extract/hier (extract_cell.hpp): CORE-NOTIMPL-008 until
// EXTHIER lands them (openspec/changes/m7-extraction tasks.md, EXTHIER).
// EXTHIER replaces this file with its own sources.
//
// Owner: EXTHIER (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/extract/hier/interact.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::extract {

diag::Outcome<CellResult> extract_cell(ExtractContext& /*ctx*/, db::CellDef& /*cell*/, ExtWriter& /*writer*/,
                                       bool /*is_top*/) {
  return cmd::m7_not_implemented_diagnostic("extract cell pipeline");
}

// The interaction areas of `cell` with Magic's default options (coupling and
// adjust on, so the halo is sidehalo + 1) and the style's magic-compat
// setting; interact.hpp has the general form (EXTHIER, tasks.md 8.2).
diag::Outcome<InteractionAreas> interactions(const db::CellDef& cell, const ExtStyle& style,
                                             const Overrides& overrides) {
  InteractionAreas out;
  const std::int32_t halo = hier_halo(style, ExtractOptions{}, overrides);
  for (const InteractionSquare& s : interaction_squares(cell, hier_step(style, overrides), halo, style.magic_compat)) {
    if (s.kind == SquareKind::interaction) {
      out.areas.push_back(s.area);
    }
  }
  return out;
}

}  // namespace hefesto::extract
