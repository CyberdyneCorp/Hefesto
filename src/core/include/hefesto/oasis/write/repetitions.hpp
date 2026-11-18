// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Repetition detection (capability oasis-write "Repetition detection";
// openspec/changes/m6-lef-def-oasis/design.md D13, Q-OASIS-65..67): with
// `oasis repetitions yes` (the default) the placements of one identical
// shape or placement are grouped into repetitions, which reorders the
// elements of a cell (so OASIS-052 compares with `repetitions no`). The
// grouping is deterministic: the same placements give the same groups, in
// the same order, on every platform.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASW fills
// in the implementation in src/core/src/oasis/write/ and maps each group to
// an oasis::Repetition (oasis/codec/elements.hpp) (design D2).

#pragma once

#include <span>
#include <vector>

#include "hefesto/db/geometry.hpp"

namespace hefesto::oasis {

/// One group: the element at `origin` repeated at `origin + d` for each
/// displacement `d`.
struct RepetitionGroup {
  db::Point origin;
  std::vector<db::Point> displacements;

  friend bool operator==(const RepetitionGroup&, const RepetitionGroup&) = default;
};

/// The groups of `positions` (the positions of one identical element, in
/// output order). Every position is in exactly one group; a lone position
/// is a group without displacements. SCAFFOLD: returns one group per
/// position, in input order (no repetition), until OASW lands it.
[[nodiscard]] std::vector<RepetitionGroup> detect_repetitions(std::span<const db::Point> positions);

}  // namespace hefesto::oasis
