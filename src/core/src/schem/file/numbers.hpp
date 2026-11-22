// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Numeric fields of `.sch`/`.sym` files (capability schematic-file-format
// "Numeric fields"): reading by C `strtod`/`%d` rules in the "C" locale and
// writing by `%.16g`/`%d` (`std::to_chars`, locale-free; design D20). Reading
// uses `std::strtod`, since `std::from_chars` for doubles needs macOS 26;
// Hefesto never changes the "C" numeric locale.
//
// Owner: FORMAT.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace hefesto::schem::file_detail {

/// A number parsed from the front of a text, and how many bytes it took.
template <typename T>
struct Parsed {
  T value{};
  std::size_t length = 0;
};

/// The longest prefix of `text` that C `strtod` accepts in the "C" locale
/// (sign, decimal or hexadecimal digits, fraction, exponent, `inf`,
/// `infinity`, `nan`, `nan(...)`; no leading white space). A value out of
/// range gives +-infinity or +-0 as `strtod` does. nullopt when no prefix is
/// a number.
[[nodiscard]] std::optional<Parsed<double>> parse_c_double(std::string_view text);

/// The longest prefix of `text` that is a decimal integer (optional sign,
/// digits). nullopt when there is none or it does not fit an `int` (C's
/// `%d` has undefined behaviour there; Hefesto treats it as a missing field).
[[nodiscard]] std::optional<Parsed<int>> parse_c_int(std::string_view text);

/// `text` read as C `atoi` reads it (schematic-symbols-and-libraries "Pin
/// order"): leading blanks skipped, an optional sign, then the longest run
/// of decimal digits; no digits gives 0. A value that does not fit an `int`
/// is clamped to INT_MIN / INT_MAX (C leaves it undefined).
[[nodiscard]] int atoi_clamped(std::string_view text) noexcept;

/// `value` as C `printf("%.16g")` prints it in the "C" locale, with glibc's
/// spelling of the non-finite values (`inf`, `-inf`, `nan`, `-nan`).
[[nodiscard]] std::string format_g16(double value);

/// `value` as C `printf("%d")`.
[[nodiscard]] std::string format_int(int value);

}  // namespace hefesto::schem::file_detail
