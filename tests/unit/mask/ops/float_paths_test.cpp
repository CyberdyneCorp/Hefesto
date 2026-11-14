// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pinned float paths of design D22: Euclidean grow (cifGrowEuclideanFunc
// offsets), the grow-min sqrt (Q-MASK-15) and the bridge distance
// (GetExpandedAreaGrid with the Q-MASK-227 clamp). The values were computed
// with IEEE double (and single, for the bridge) arithmetic in Magic's
// operation order; they must be identical on every CI leg. Several inputs
// are chosen where the product lands next to an integer (8.0 against
// 11.999999999999998), where a fused multiply-add or extended precision
// would change the ceiling.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/mask/ops/compat.hpp"

using hefesto::db::Rect;
using namespace hefesto::mask;

namespace {

void check_offsets(std::int32_t d, std::int32_t w, std::int32_t h, std::int32_t dx, std::int32_t dy) {
  const EuclideanOffsets o = euclidean_offsets(d, w, h);
  CHECK(o.dx == dx);
  CHECK(o.dy == dy);
}

}  // namespace

TEST_CASE("pinned Euclidean grow offsets", "[cap:mask-output-operators][req:Euclidean grow and shrink]") {
  check_offsets(10, 10, 10, 5, 5);  // ceil(4.142135623730951)
  check_offsets(1000, 10, 10, 415, 415);
  check_offsets(7, 3, 11, 6, 1);     // 5.346570886994514
  check_offsets(25, 40, 30, 9, 13);  // 8.333..., 12.5
  check_offsets(24, 40, 30, 8, 12);  // 8.0 and 11.999999999999998
  check_offsets(18, 40, 30, 6, 9);   // 6.0 and 8.999999999999998
  check_offsets(6, 4, 3, 2, 3);      // 2.0 and 2.9999999999999996
}

TEST_CASE("pinned grow-min corner heights (Q-MASK-15)", "[cap:mask-output-operators][req:grow-min]") {
  CHECK(grow_min_height(10, 4) == 7);
  CHECK(grow_min_height(100, 0) == 86);
  CHECK(grow_min_height(13, 6) == 8);
  // (g + w)^2 overflows a 32-bit int and wraps, as Magic's int product does.
  CHECK(grow_min_height(30000, 20000) == 36725);
}

TEST_CASE("pinned bridge expansions (Q-MASK-227)", "[cap:mask-output-operators][req:bridge]") {
  // Catecorner squares: W = H = 2, vertical: dy = 1, dx = ceil(sqrtf(15)) = 4.
  CHECK(bridge_expand(Rect{10, 10, 12, 12}, 4, false, 0) == Rect{6, 9, 16, 13});
  CHECK(bridge_expand(Rect{10, 10, 12, 12}, 4, false, 5) == Rect{5, 5, 20, 15});
  // Pinch (space): W = H = 1: dy = 2, dx = ceil(sqrtf(7) - 1) = 2.
  CHECK(bridge_expand(Rect{9, 9, 10, 10}, 4, true, 0) == Rect{7, 7, 12, 12});
  // Overlap in x (inverted) with space: vertical, b = 16 - (3 + 1)^2 = 0,
  // dx = max(0, ceil((sqrtf(7) - 1) / 2)) = 1.
  CHECK(bridge_expand(Rect{10, 0, 9, 3}, 4, true, 0) == Rect{8, -1, 11, 4});
  // Negative sqrtf arguments are clamped to 0 (the fix of Q-MASK-227).
  CHECK(bridge_expand(Rect{10, 10, 18, 11}, 4, false, 0) == Rect{12, 6, 16, 15});
  CHECK(bridge_expand(Rect{0, 0, 30, 30}, 4, false, 0) == Rect{0, 13, 30, 17});
}
