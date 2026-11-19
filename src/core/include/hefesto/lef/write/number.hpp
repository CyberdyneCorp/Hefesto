// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF number output (capability lef-write "Output scale and LEF database
// units", "Distance number formatting"; openspec/changes/m6-lef-def-oasis/
// design.md D7, Q-LEFDEF-34, Q-LEFDEF-45): Magic's lefPrint
// (lefWrite.c:89-127), CIFGetOutputScale(1000) (CIFmain.c:139-146) and the
// LEFdbUnits computation of lefWriteHeader (lefWrite.c:274-292).
//
// A distance is rounded half away from zero to the 1/U grid of the LEF
// database units U in float32 arithmetic (truncf(v * U + r) / U,
// r = +-0.5) and printed with the number of decimals U implies (%.2f for
// 100, %.3f for 200 and 1000, %.4f for 2000 and 10000, %.5f for 20000,
// else the 100000 path, whose product is computed in double). A value in
// (-1/(2U), 0) prints as `-0.000` in both modes (replicated).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFW fills
// in the implementation in src/core/src/lef/write/ (design D2).

#pragma once

#include <cstdint>
#include <string>

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::lef {

/// The LEF database units of a write (LEFdbUnits): the `UNITS DATABASE
/// MICRONS` value and the rounding grid of every distance. Session-sticky
/// with magic-compat ON (Q-LEFDEF-34), so LefSession keeps one (design D4
/// `lefdef.units`); Magic starts the session at 1000.
struct LefUnits {
  std::int32_t database = 1000;

  friend bool operator==(const LefUnits&, const LefUnits&) = default;
};

/// lefPrint(value): `value` in microns, formatted for `units`. `value` is
/// float32, as Magic's argument is.
[[nodiscard]] std::string format_distance(float value, LefUnits units);

/// CIFGetOutputScale(1000): microns per internal unit of `style`, in
/// float32 (`float(10 * scaleFactor) / float(expander * 1000)`); 1.0 with
/// no output style.
[[nodiscard]] float output_scale(const mask::OutputStyle* style) noexcept;

/// The LEF database units lefWriteHeader computes from `style`: 1000, times
/// 10 for a style in angstroms, then divided by a grid limit of 1, 5 or 10.
/// 1000 with no output style.
[[nodiscard]] LefUnits database_units(const mask::OutputStyle* style) noexcept;

/// C's printf("%f", value): the antenna areas, the technology LAYER values
/// and CUT AREA, which are not rounded.
[[nodiscard]] std::string format_real(double value);

}  // namespace hefesto::lef
