// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIRCLE approximation of the OASIS reader (capability oasis-read
// "CIRCLE elements"; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D3, D12,
// Q-OASIS-31): a regular N-gon whose vertex k is
// (round(r cos(2 pi k / N)), round(r sin(2 pi k / N))), computed from IEEE
// basic operations only (no libm), so every platform gets the same
// vertices.
//
// Owner: OASR (design D2).

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/oasis/codec/elements.hpp"

namespace hefesto::oasis {

/// The `circlepoints` bounds (oasis-command).
inline constexpr std::int32_t kMinCirclePoints = 8;
inline constexpr std::int32_t kMaxCirclePoints = 4096;

/// cos(2 pi k / n) and sin(2 pi k / n), the same bits on every platform:
/// the angle is reduced to the first octant exactly (in integers), and
/// sine and cosine there are Taylor polynomials in binary64.
struct UnitVector {
  double cos = 1.0;
  double sin = 0.0;
};
[[nodiscard]] UnitVector unit_vector(std::uint32_t k, std::uint32_t n) noexcept;

/// The vertices of a circle of radius `radius` centred on the origin with
/// `n` vertices (clamped to [kMinCirclePoints, kMaxCirclePoints]), in order
/// from (radius, 0) counterclockwise, coordinates rounded half away from
/// zero, consecutive coincident vertices merged. The closing vertex is not
/// repeated. Empty for a radius of 0 or one above 2^52.
[[nodiscard]] std::vector<Point64> circle_vertices(std::int64_t radius, std::int32_t n);

}  // namespace hefesto::oasis
