// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The maximal-rectangle search used by the maxwidth rule and the
// wide-spacing trigger (drc-rule-semantics "Maxwidth rules", "Wide-spacing
// trigger (maximal rectangle search)"; openspec/changes/m3-drc design.md D6).
//
// maximal_rectangles(plane, bounds, wrong_types, max_distance) starts from
// the candidate list {bounds} and lets every tile of `wrong_types` on
// `plane` that overlaps `bounds` (in Magic's area-enumeration order) clip or
// subdivide the candidates, as FindMaxRects does; it returns the remaining
// rectangles in Magic's list order (empty when none is left).
//
// A candidate is cut only by tile sides that do not reach the plane
// boundary; pieces thinner than `max_distance` are dropped; the search
// stops when no candidate is left.
//
// (Magic ref: utils/maxrect.c FindMaxRects, genCanonicalMaxwidth;
// drc/DRCextend.c drcCanonicalMaxwidth)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::drc {

/// See the file comment. `max_distance` is MaxRectsData::maxdist.
[[nodiscard]] std::vector<db::Rect> maximal_rectangles(const db::Plane& plane, const db::Rect& bounds,
                                                       const tech::TypeMask& wrong_types, std::int32_t max_distance);

}  // namespace hefesto::drc
