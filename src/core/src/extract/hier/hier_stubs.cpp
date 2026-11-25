// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract::interactions (extract_cell.hpp) with Magic's default options;
// the former scaffold stubs of extract/hier are all implemented.
//
// Owner: EXTHIER (design D3).

#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/extract/hier/interact.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::extract {

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
