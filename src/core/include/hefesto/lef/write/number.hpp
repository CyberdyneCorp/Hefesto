// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF number output (capability lef-write "Number formatting";
// openspec/changes/m6-lef-def-oasis/design.md D3, D7, Q-LEFDEF-45): Magic's
// lefPrint (lefWrite.c:89-140). A distance is rounded half away from zero
// to the 1/U grid of the LEF database units U in float32 arithmetic
// (truncf(v * U + r) / U, r = +-0.5) and printed with the number of
// decimals U implies (%.2f for 100, %.3f for 200 and 1000, %.4f for 2000
// and 10000, %.5f for 20000, else the 100000 path). A value in
// (-1/(2U), 0) prints as `-0.000` in both modes (replicated). The pinned
// values of design D21 run on every CI leg.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFW fills
// in the implementation in src/core/src/lef/write/ (design D2).

#pragma once

#include <cstdint>
#include <string>

namespace hefesto::lef {

/// The LEF database units of a write (LEFdbUnits): the `UNITS DATABASE
/// MICRONS` value. Session-sticky with magic-compat ON (Q-LEFDEF-34), so
/// LefSession keeps one (design D4 `lefdef.units`).
struct LefUnits {
  std::int32_t database = 100;

  friend bool operator==(const LefUnits&, const LefUnits&) = default;
};

/// lefPrint(value): `value` in microns, formatted for `units`. `value` is
/// float32, as Magic's argument is. SCAFFOLD: returns "" until LEFW lands
/// it.
[[nodiscard]] std::string format_distance(float value, LefUnits units);

}  // namespace hefesto::lef
