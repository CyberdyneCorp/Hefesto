// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF unit conversion (lefRead.c:3015, 2797, 868-936; cif/CIFmain.c:139-146).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/units.hpp"

#include <cmath>

#include "hefesto/mask/style/style.hpp"

namespace hefesto::lef {

namespace {

// The int range Magic's cast keeps; outside it the cast is undefined.
constexpr float kIntLimit = 2147483648.0F;  // 2^31, exact in binary32

std::optional<db::Coord> rounded(float quotient) noexcept {
  const float r = std::roundf(quotient);
  if (!(r >= -kIntLimit && r < kIntLimit)) {
    return std::nullopt;  // NaN or out of range
  }
  return static_cast<db::Coord>(r);
}

}  // namespace

float lef_scale(const mask::OutputStyle* style) noexcept {
  if (style == nullptr) {
    return 1.0F;
  }
  // 64-bit products: Magic's int products overflow only for scale factors
  // no style has.
  const std::int64_t numerator = std::int64_t{10} * style->scale.scale_factor;
  const std::int64_t denominator = std::int64_t{style->scale.expander} * 1000;
  return static_cast<float>(numerator) / static_cast<float>(denominator);
}

std::optional<db::Coord> to_internal(float value, float scale) noexcept {
  return rounded(value / scale);
}

std::optional<db::Coord> to_half_internal(float value, float scale) noexcept {
  return rounded(value / (scale / 2.0F));
}

}  // namespace hefesto::lef
