// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A correctly rounded binary64 arctangent, written in-house from IEEE
// basic operations and fused multiply-add (openspec/changes/m7-extraction
// design.md Open Question 3, decided (a); D4, D22). The fringe-halo
// capacitance model evaluates 0.6366 * atan(mult * d) (Magic
// extract/ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875); the platform
// atan functions are not guaranteed to agree to the last bit, so the
// extractor uses this one on every platform.
//
// Method: |x| > 1 is reduced with atan(x) = pi/2 - atan(1/x); the argument
// t in [0, 1] is reduced around c = k/64 with atan(t) = atan(c) +
// atan((t - c) / (1 + t*c)), |u| <= 1/128; atan(u) is an odd Taylor series
// of 10 terms. Everything runs in double-double arithmetic (about 100
// correct bits), so the final rounding to nearest is correct except for an
// argument whose atan lies within about 2^-100 relative of a rounding
// midpoint; the reference test (tests/unit/extract/cap/cr_atan_test.cpp)
// checks the result bit for bit against a 300-bit evaluation.
//
// Owner: EXTCAP (openspec/changes/m7-extraction/design.md D3, Open Question 3).

#pragma once

namespace hefesto::util {

/// atan(x) rounded to nearest binary64; atan(+-0) = +-0, atan(NaN) is NaN,
/// atan(+-inf) = +-pi/2 rounded.
[[nodiscard]] double cr_atan(double x) noexcept;

}  // namespace hefesto::util
