// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DBArrayOverlap (see hefesto/db/array_overlap.hpp). Ported from Magic VLSI
// 8.3.694 (fc5daa9e) database/DBcellbox.c, Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
// Owner: MODEL.

#include "hefesto/db/array_overlap.hpp"

#include <algorithm>

#include "hefesto/db/hierarchy.hpp"

namespace hefesto::db {

namespace {

struct Axis {
  std::int64_t lo;    // the use's smaller index
  std::int64_t hi;    // the larger index
  std::int64_t sep;   // the pitch
  std::int64_t a_lo;  // the area, child coordinates
  std::int64_t a_hi;
  std::int64_t b_lo;  // the child's bounding box
  std::int64_t b_hi;
};

/// One axis on the canonical (ascending) indices: [out_lo, out_hi], empty
/// when out_lo > out_hi.
void axis_overlap(Axis a, std::int64_t& out_lo, std::int64_t& out_hi) {
  if (a.sep < 0) {  // flip everything about the axis
    a = Axis{a.lo, a.hi, -a.sep, -a.a_hi, -a.a_lo, -a.b_hi, -a.b_lo};
  }
  out_lo = a.lo;
  out_hi = a.hi;
  if (a.sep != 0) {
    out_lo = a.lo + (a.a_lo - a.b_hi + a.sep - 1) / a.sep;
    out_hi = a.lo + (a.a_hi - a.b_lo) / a.sep;
  }
  out_lo = std::max(out_lo, a.lo);
  out_hi = std::min(out_hi, a.hi);
}

/// Back from canonical indices to the use's own index direction. The
/// results lie in [min(first, last), max(first, last)] (or form an empty
/// range next to it), so they fit 32 bits.
void to_use_indices(std::int32_t first, std::int32_t last, std::int64_t lo, std::int64_t hi, std::int32_t& out_lo,
                    std::int32_t& out_hi) {
  if (first > last) {
    const std::int64_t sum = std::int64_t{last} + first;
    const std::int64_t mirrored_lo = sum - hi;
    hi = sum - lo;
    lo = mirrored_lo;
  }
  out_lo = static_cast<std::int32_t>(std::clamp<std::int64_t>(lo, INT32_MIN, INT32_MAX));
  out_hi = static_cast<std::int32_t>(std::clamp<std::int64_t>(hi, INT32_MIN, INT32_MAX));
}

}  // namespace

ElementRange array_overlap(const CellUse& use, const Rect& parent_area) {
  const ArrayInfo& ar = use.array;
  if (!ar.is_array()) {
    return ElementRange{ar.xlo, ar.xlo, ar.ylo, ar.ylo};
  }
  const Rect child = apply(invert(use.transform), parent_area);
  const Rect box = use.def->bbox();
  std::int64_t xlo = 0;
  std::int64_t xhi = 0;
  std::int64_t ylo = 0;
  std::int64_t yhi = 0;
  axis_overlap(Axis{std::min(ar.xlo, ar.xhi), std::max(ar.xlo, ar.xhi), ar.xsep, child.x0, child.x1, box.x0, box.x1},
               xlo, xhi);
  axis_overlap(Axis{std::min(ar.ylo, ar.yhi), std::max(ar.ylo, ar.yhi), ar.ysep, child.y0, child.y1, box.y0, box.y1},
               ylo, yhi);
  ElementRange r;
  to_use_indices(ar.xlo, ar.xhi, xlo, xhi, r.xlo, r.xhi);
  to_use_indices(ar.ylo, ar.yhi, ylo, yhi, r.ylo, r.yhi);
  return r;
}

}  // namespace hefesto::db
