// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF number output (capability lef-write "Output scale and LEF database
// units", "Distance number formatting"; Q-LEFDEF-34, Q-LEFDEF-45). The
// area builds with -ffp-contract=off, so the float32 products and sums
// below round exactly as Magic's separate operations do.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c lefPrint and
// lefWriteHeader (the LEFdbUnits computation) and cif/CIFmain.c
// CIFGetOutputScale, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include "hefesto/lef/write/number.hpp"

#include <array>
#include <cmath>
#include <cstdio>

#include "hefesto/mask/style/style.hpp"

namespace hefesto::lef {

namespace {

// printf("%.<decimals>f", value).
std::string print(int decimals, double value) {
  std::array<char, 64> buffer{};
  const int n = std::snprintf(buffer.data(), buffer.size(), "%.*f", decimals, value);
  if (n < 0) {
    return {};
  }
  if (static_cast<std::size_t>(n) < buffer.size()) {
    return std::string{buffer.data(), static_cast<std::size_t>(n)};
  }
  std::string big(static_cast<std::size_t>(n) + 1, '\0');
  (void)std::snprintf(big.data(), big.size(), "%.*f", decimals, value);
  big.resize(static_cast<std::size_t>(n));
  return big;
}

// The decimals lefPrint prints for each LEFdbUnits value.
int decimals_of(std::int32_t units) noexcept {
  switch (units) {
    case 100:
      return 2;
    case 200:
    case 1000:
      return 3;
    case 2000:
    case 10000:
      return 4;
    default:
      return 5;
  }
}

// printf's "%f" precision.
constexpr int kRealDecimals = 6;

bool rounds_in_float(std::int32_t units) noexcept {
  return units == 100 || units == 200 || units == 1000 || units == 2000 || units == 10000 || units == 20000;
}

}  // namespace

// lefPrint (lefWrite.c:89-127).
std::string format_distance(float value, LefUnits units) {
  const float r = value < 0.0F ? -0.5F : 0.5F;
  float rounded = 0.0F;
  if (rounds_in_float(units.database)) {
    const auto l = static_cast<float>(units.database);
    const float scaled = value * l;
    const float shifted = scaled + r;
    rounded = std::trunc(shifted) / l;
  } else {
    // (float)(truncf((invalue * 100000.) + r) / 100000.)
    const double scaled = static_cast<double>(value) * 100000.0;
    const double shifted = scaled + static_cast<double>(r);
    const float truncated = std::trunc(static_cast<float>(shifted));
    rounded = static_cast<float>(static_cast<double>(truncated) / 100000.0);
  }
  return print(decimals_of(units.database), static_cast<double>(rounded));
}

// CIFGetOutputScale(1000).
float output_scale(const mask::OutputStyle* style) noexcept {
  if (style == nullptr) {
    return 1.0F;
  }
  const auto numerator = static_cast<float>(10 * style->scale.scale_factor);
  const auto denominator = static_cast<float>(style->scale.expander * 1000);
  return numerator / denominator;
}

// lefWriteHeader's LEFdbUnits (lefWrite.c:274-292).
LefUnits database_units(const mask::OutputStyle* style) noexcept {
  LefUnits units;
  if (style == nullptr) {
    return units;
  }
  if (style->has_option(mask::style_option::angstroms)) {
    units.database *= 10;
  }
  const std::int32_t limit = style->grid_limit;
  if (limit == 1 || limit == 5 || limit == 10) {
    units.database /= limit;
  }
  return units;
}

std::string format_real(double value) {
  return print(kRealDecimals, value);
}

}  // namespace hefesto::lef
