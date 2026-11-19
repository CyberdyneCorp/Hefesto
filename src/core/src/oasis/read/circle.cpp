// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIRCLE vertices (see hefesto/oasis/read/circle.hpp; capability oasis-read
// "CIRCLE elements", Q-OASIS-31 fix). The angle 2 pi k / n is reduced to an
// octant in integer arithmetic, so the reduction is exact; sine and cosine
// of the reduced angle (at most pi / 4) are Taylor polynomials evaluated
// with IEEE additions and multiplications only, which -ffp-contract=off
// keeps unfused (design D3). Every platform therefore computes the same
// bits, and the octants are exact mirror images of each other.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/oasis/read/circle.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace hefesto::oasis {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::int64_t kMaxRadius = std::int64_t{1} << 52;

/// 1/n! for the odd (sine) and even (cosine) Taylor terms; |t| <= pi/4
/// leaves the first omitted term below 2^-60.
constexpr std::array<double, 10> kSine = {
    1.0,           -1.0 / 6.0,          1.0 / 120.0,           -1.0 / 5040.0,          1.0 / 362880.0,
    -1.0 / 39916800.0, 1.0 / 6227020800.0, -1.0 / 1307674368000.0, 1.0 / 355687428096000.0,
    -1.0 / 121645100408832000.0};
constexpr std::array<double, 10> kCosine = {
    1.0,          -1.0 / 2.0,          1.0 / 24.0,           -1.0 / 720.0,           1.0 / 40320.0,
    -1.0 / 3628800.0, 1.0 / 479001600.0, -1.0 / 87178291200.0, 1.0 / 20922789888000.0,
    -1.0 / 6402373705728000.0};

/// sum c_i t2^i by Horner's rule.
double horner(const std::array<double, 10>& c, double t2) {
  double sum = c.back();
  for (std::size_t i = c.size() - 1; i-- > 0;) {
    sum = sum * t2 + c[i];
  }
  return sum;
}

std::int64_t round_away(double v) {
  return static_cast<std::int64_t>(std::round(v));  // round half away from zero, exact in IEEE
}

}  // namespace

UnitVector unit_vector(std::uint32_t k, std::uint32_t n) noexcept {
  if (n == 0) {
    return {};
  }
  const std::uint64_t eighths = std::uint64_t{8} * (k % n);
  const std::uint64_t octant = eighths / n;
  const std::uint64_t rest = eighths - octant * n;
  // The reduced angle (pi / 4) * b / n, measured from the octant's start
  // (even octants) or from its end (odd octants).
  const std::uint64_t b = octant % 2 == 0 ? rest : n - rest;
  const double t = static_cast<double>(b) * kPi / (4.0 * static_cast<double>(n));
  const double t2 = t * t;
  const double s = t * horner(kSine, t2);
  const double c = horner(kCosine, t2);
  switch (octant) {
    case 0:
      return {c, s};
    case 1:
      return {s, c};
    case 2:
      return {-s, c};
    case 3:
      return {-c, s};
    case 4:
      return {-c, -s};
    case 5:
      return {-s, -c};
    case 6:
      return {s, -c};
    default:
      return {c, -s};
  }
}

std::vector<Point64> circle_vertices(std::int64_t radius, std::int32_t n) {
  std::vector<Point64> out;
  if (radius <= 0 || radius > kMaxRadius) {
    return out;
  }
  const auto count = static_cast<std::uint32_t>(std::clamp(n, kMinCirclePoints, kMaxCirclePoints));
  const auto r = static_cast<double>(radius);
  out.reserve(count);
  for (std::uint32_t k = 0; k < count; ++k) {
    const UnitVector u = unit_vector(k, count);
    const Point64 p{round_away(r * u.cos), round_away(r * u.sin)};
    if (out.empty() || !(out.back() == p)) {
      out.push_back(p);
    }
  }
  while (out.size() > 1 && out.back() == out.front()) {
    out.pop_back();
  }
  return out;
}

}  // namespace hefesto::oasis
