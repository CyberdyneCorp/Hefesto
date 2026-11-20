// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shape repetition detection (capability oasis-write "Shape repetition
// detection"; openspec/changes/m6-lef-def-oasis/design.md D13, Q-OASIS-67):
// with `oasis repetitions yes` (the default) the shapes of one mask layer
// with the same key (RECTANGLE size, or POLYGON point list) are grouped
// into repetitions, which reorders the elements of the layer (so OASIS-052
// compares with `repetitions no`). The grouping is deterministic: the same
// origins give the same groups, in the same order, on every platform.
//
// Owner: OASW (design D2; the header was created by the M6 scaffold).

#pragma once

#include <optional>
#include <span>
#include <vector>

#include "hefesto/oasis/codec/elements.hpp"

namespace hefesto::oasis {

/// One output element: the shape at `origin`, repeated by `repetition`
/// (type 1, 2 or 3), or alone when it has none.
struct RepetitionGroup {
  Point64 origin;
  std::optional<Repetition> repetition;

  friend bool operator==(const RepetitionGroup&, const RepetitionGroup&) = default;
};

/// The groups of `origins` (the origins of the shapes of one key, any
/// order, duplicates allowed) by the algorithm of "Shape repetition
/// detection": rows of equal pitch, sorted by (x, pitch, count, y), then
/// rows of equal columns and equal row spacing merged. The groups expanded
/// by their repetitions give back exactly `origins`.
[[nodiscard]] std::vector<RepetitionGroup> detect_repetitions(std::span<const Point64> origins);

}  // namespace hefesto::oasis
