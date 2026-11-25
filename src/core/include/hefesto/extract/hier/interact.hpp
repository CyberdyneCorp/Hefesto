// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The interaction areas of hierarchical extraction (capability
// extract-hierarchy "Interaction halo and step squares", "Interaction
// area"): the halo, the step squares of a cell and, per square, the
// interaction area DRCFindInteractions finds, widened by the cell's labels.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtSubtree.c (extSubtree,
// :164-284) and drc/DRCsubcell.c (DRCFindInteractions, drcSubcellFunc,
// drcFindOtherCells, drcSubCheckPaint); Magic is Copyright (C) 1985, 1990
// Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md). DRCFindInteractions runs here on its own (the
// DRC engine's copy needs an editor state); its Q-MASK-400 branch follows
// the same magic-compat setting as the DRC engine's.
//
// Owner: EXTHIER.

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/util/function_ref.hpp"

namespace hefesto::extract {

class ExtStyle;

/// The hierarchical halo: sidehalo + 1 with options `coupling` and
/// `adjust` both on, else 1 (the `extract halo` override replaces the
/// style's side halo).
[[nodiscard]] std::int32_t hier_halo(const ExtStyle& style, const ExtractOptions& options, const Overrides& overrides);

/// The step size of the squares (the `extract step` override or the
/// style's).
[[nodiscard]] std::int32_t hier_step(const ExtStyle& style, const Overrides& overrides);

/// What one step square holds.
enum class SquareKind : std::uint8_t {
  none,         // no subcell near the square: nothing to do
  substrate,    // subcells near, no interaction: only their substrates
  interaction,  // an interaction to process
};

struct InteractionSquare {
  db::Rect square;  // r
  SquareKind kind = SquareKind::none;
  db::Rect area;    // I (interaction only; may extend past r)
  db::Rect clip;    // C = I clipped to r (interaction only)
};

/// DRCFindInteractions(cell, area, radius): nullopt when no subcell use
/// overlaps `area` expanded by `radius` (Magic's -1); otherwise the
/// interaction area clipped to `area`, itself nullopt when there is none
/// (Magic's 0).
[[nodiscard]] std::optional<std::optional<db::Rect>> find_interaction(const db::CellDef& cell, const db::Rect& area,
                                                                      std::int32_t radius, bool magic_compat);

/// extSubtree's step squares of `cell` (rows bottom to top, squares left
/// to right, from the bounding box's lower-left corner), each classified
/// with what DRCFindInteractions and the cell's labels make of it, and
/// handed to `visit` one at a time unless it is SquareKind::none (nothing
/// is kept, so a small step on a large cell costs no memory). `visit`
/// returns false to stop (a cancelled pass). A step below 1 makes one
/// square of the whole bounding box. A square edge past the plane's
/// infinity is clamped to it. Returns false when `visit` stopped the walk.
bool for_each_square(const db::CellDef& cell, std::int32_t step, std::int32_t halo, bool magic_compat,
                     util::FunctionRef<bool(const InteractionSquare&)> visit);

/// The squares of for_each_square that are not SquareKind::none, in its
/// order.
[[nodiscard]] std::vector<InteractionSquare> interaction_squares(const db::CellDef& cell, std::int32_t step,
                                                                 std::int32_t halo, bool magic_compat);

}  // namespace hefesto::extract
