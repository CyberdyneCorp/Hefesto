// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DBArrayOverlap: the elements of an array use whose images may overlap an
// area of the parent. One implementation for the hierarchy searches that
// visit array elements one by one (the use index's DBCellReadArea, the DRC
// engine, mask generation's flattening and the writers' subtree loading).
// Ported from Magic VLSI 8.3.694 (fc5daa9e) database/DBcellbox.c
// (DBArrayOverlap), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: MODEL (added by the m5-mask-gds-cif round 5 dedupe).

#pragma once

#include <cstdint>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"

namespace hefesto::db {

/// An inclusive element index range in each direction, xlo <= xhi and
/// ylo <= yhi unless the range is empty (lo > hi then).
struct ElementRange {
  std::int32_t xlo = 0;
  std::int32_t xhi = 0;
  std::int32_t ylo = 0;
  std::int32_t yhi = 0;

  friend bool operator==(const ElementRange&, const ElementRange&) = default;
};

/// DBArrayOverlap(use, parent_area): the element indices of `use` whose
/// images may overlap `parent_area` (parent coordinates), clamped to the
/// use's index ranges and given in the use's own index direction. A use
/// that is not an array gives its single element. A zero pitch along an
/// axis keeps that axis's whole range. The arithmetic is 64-bit, so no
/// coordinate or pitch overflows.
[[nodiscard]] ElementRange array_overlap(const CellUse& use, const Rect& parent_area);

}  // namespace hefesto::db
