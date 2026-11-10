// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The floating-point and wide-integer formulas of the basic check
// (drc-rule-semantics "Euclidean corner filter", "Diagonal edge checks";
// openspec/changes/m3-drc design.md D3/D6), exposed so a determinism test
// can pin their results on edge values.
//
// They use the exact types and operation order of Magic's x86_64 build:
// `long` (64-bit) sums, a single-precision projection term and a
// double-precision square root. The core is compiled with
// -ffp-contract=off (design D3), so no multiply-add is fused and the
// results are identical on x86_64 and aarch64.
//
// (Magic ref: drc/DRCbasic.c drcCifPointToSegment, drcRadialClear,
// drcTile (diagonal offset))
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>

namespace hefesto::drc {

/// drcCifPointToSegment: the squared distance from (px, py) to the segment
/// (s1x, s1y)-(s2x, s2y); the endpoint distances when the projection falls
/// outside the segment, else `a - (c + a - b)^2 / (4c)` with the quotient
/// computed in single precision and truncated toward zero.
[[nodiscard]] std::int64_t point_to_segment_squared(std::int32_t px, std::int32_t py, std::int32_t s1x,
                                                    std::int32_t s1y, std::int32_t s2x, std::int32_t s2y) noexcept;

/// drcRadialClear: whether (sqx*k + b)^2 + (sqy*k + b)^2 >= (s*k + b)^2 in
/// 64-bit integers, with border b = `border_num` and k = `border_den`.
[[nodiscard]] bool radial_clear(std::int32_t sqx, std::int32_t sqy, std::int32_t s, std::int32_t border_num,
                                std::int32_t border_den) noexcept;

/// The offset of a diagonal-edge search (drcTile): for a split tile w wide
/// and h high and rule distance `dist`, dx = trunc(dist * sqrt(1 / (1 +
/// w^2/h^2))) in double precision and dy = trunc(dx * w / h) in 32-bit
/// integers (w*w and dx*w wrap like Magic's int arithmetic).
struct DiagonalOffset {
  std::int32_t dx = 0;
  std::int32_t dy = 0;
  friend bool operator==(const DiagonalOffset&, const DiagonalOffset&) = default;
};
[[nodiscard]] DiagonalOffset diagonal_offset(std::int32_t dist, std::int32_t width, std::int32_t height) noexcept;

}  // namespace hefesto::drc
