// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF unit conversion (capability lef-read "LEF unit conversion"; design
// D3, D6): LEF distances are microns whatever the UNITS section says. The
// scale s (microns per internal unit) is CIFGetOutputScale(1000) of the
// current output style, and a value v becomes round(v / s) in binary32,
// rounded half away from zero (roundf). Via geometry uses half units,
// round(v / (s / 2)).
//
// Magic casts the rounded float to int, which is undefined behaviour when
// the value does not fit; Hefesto answers nullopt there and for NaN, and
// the callers ignore the value (X-2 tier 1).
//
// Owner: LEFR (design D2).

#pragma once

#include <optional>

#include "hefesto/db/geometry.hpp"

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::lef {

/// CIFGetOutputScale(1000): (float)(10 * scale factor) / (float)(expander *
/// 1000) of `style`; 1.0 without an output style.
[[nodiscard]] float lef_scale(const mask::OutputStyle* style) noexcept;

/// (int)roundf(value / scale); nullopt when the result is NaN or does not
/// fit an internal coordinate.
[[nodiscard]] std::optional<db::Coord> to_internal(float value, float scale) noexcept;

/// Via geometry in half internal units: (int)roundf(value / (scale / 2)).
[[nodiscard]] std::optional<db::Coord> to_half_internal(float value, float scale) noexcept;

}  // namespace hefesto::lef
