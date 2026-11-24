// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Lumped resistance (ExtBasic.c extSetResist :585-670, the record fields
// :689 and :735-759; capability extract-resistance-lumped "Lumped
// resistance formula", "Resistance output rounding", "Resistance class
// header").
//
// Owner: EXTCAP.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "hefesto/extract/cap/caps.hpp"

namespace hefesto::extract {

namespace {

/// A float converted to a C int: truncated toward zero, saturated to the
/// int range, NaN as 0. Magic's `resnew = <float>` is undefined behaviour
/// past the int range (fp == s, a division by zero, gives +inf); fixed in
/// both modes, as Q-EXTRACT-30 decides for the extract section.
std::int32_t to_int32(float v) noexcept {
  if (std::isnan(v)) {
    return 0;
  }
  if (v >= 2147483648.0F) {
    return std::numeric_limits<std::int32_t>::max();
  }
  if (v <= -2147483648.0F) {
    return std::numeric_limits<std::int32_t>::min();
  }
  return static_cast<std::int32_t>(v);
}

std::int32_t to_int32(double v) noexcept {
  if (std::isnan(v)) {
    return 0;
  }
  if (v >= 2147483647.0) {
    return std::numeric_limits<std::int32_t>::max();
  }
  if (v <= -2147483648.0) {
    return std::numeric_limits<std::int32_t>::min();
  }
  return static_cast<std::int32_t>(v);
}

/// One class's resnew in Magic's single precision.
std::int32_t class_resistance(std::int64_t area, std::int64_t perim, std::int32_t sheet) {
  // v = (double)((dlong)perim * perim - 16 * area) assigned to a float:
  // the 64-bit integer is rounded to double, then to float (the two
  // roundings differ from one past 2^53). perim is an int in Magic.
  const auto p = static_cast<std::int64_t>(static_cast<std::int32_t>(perim));
  const auto v = static_cast<float>(static_cast<double>(p * p - 16 * area));
  const float s = v < 0 ? 0.0F : static_cast<float>(std::sqrt(static_cast<double>(v)));
  const auto fperim = static_cast<float>(p);
  const float ratio = (fperim + s) / (fperim - s);
  return to_int32(ratio * static_cast<float>(sheet));
}

}  // namespace

std::int64_t lumped_resistance(const NodeRegion& node, const ExtStyle& style) {
  std::int32_t total = 0;
  const std::size_t n = std::min(node.classes.size(), style.resist_classes.size());
  for (std::size_t i = 0; i < n; ++i) {
    const ClassArea& ca = node.classes[i];
    if (ca.area <= 0 || ca.perimeter <= 0) {
      continue;
    }
    const std::int32_t sheet = to_int32(style.resist_classes[i].sheet_resistance);
    const std::int64_t sum = std::int64_t{total} + class_resistance(ca.area, ca.perimeter, sheet);
    // restot > 0 with Magic's int: a sum past the int range wraps negative
    // and keeps the previous total.
    if (sum > 0 && sum <= std::numeric_limits<std::int32_t>::max()) {
      total = static_cast<std::int32_t>(sum);
    }
  }
  return total;
}

std::int64_t resistance_output(std::int64_t milliohms, double rscale) noexcept {
  const std::int32_t scale = to_int32(rscale);
  if (scale == 0) {
    return milliohms;  // Magic divides by zero; a style never has rscale 0
  }
  const std::int64_t rround = scale / 2;
  // (nreg_resist + rround) / exts_resistScale in int.
  return static_cast<std::int32_t>(milliohms + rround) / scale;
}

std::vector<std::int64_t> resistance_class_header(const ExtStyle& style) {
  std::vector<std::int64_t> out;
  out.reserve(style.resist_classes.size());
  for (const ResistClass& rc : style.resist_classes) {
    out.push_back(to_int32(rc.sheet_resistance));
  }
  return out;
}

}  // namespace hefesto::extract
