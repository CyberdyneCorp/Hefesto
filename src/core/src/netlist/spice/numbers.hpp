// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: Magic's number formats in the SPICE writer, independent of the
// locale: `%g`, `%f` and esSIvalue (format_si, settings.hpp).
//
// Owner: SPICE.

#pragma once

#include <string>

namespace hefesto::netlist::spice::detail {

/// A float promoted to double, as C's usual arithmetic conversions do in
/// Magic's mixed float/double expressions (kept explicit for
/// -Wdouble-promotion).
[[nodiscard]] constexpr double dbl(float value) noexcept {
  return static_cast<double>(value);
}

/// printf("%g", value) in the C locale.
[[nodiscard]] std::string format_g(double value);
/// printf("%f", value) in the C locale.
[[nodiscard]] std::string format_f(double value);
/// esSIvalue of a single-precision value (the conversion to float that
/// Magic's parameter makes is the caller's).
[[nodiscard]] std::string si(float value);

}  // namespace hefesto::netlist::spice::detail
