// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Small SPICE text helpers shared by deck preparation and the result code
// (openspec/changes/m9-lvs-simulation design.md D11, D12). Added in M9 R2
// (task 13.1).
//
// Owner: SIMDECK.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::sim {

/// A SPICE number: a decimal (C locale) with an optional scale suffix
/// (T, G, Meg, K, mil, M = milli, U, N, P, F; case-insensitive) and any
/// trailing unit letters, e.g. `10n`, `1.8`, `2.5Meg`, `1e-9s`. nullopt when
/// the text does not start with a number.
[[nodiscard]] std::optional<double> parse_spice_number(std::string_view text);

/// The whitespace-separated tokens of `line` (`(`, `)`, `,` and `=` are
/// also separators).
[[nodiscard]] std::vector<std::string> spice_tokens(std::string_view line);

/// `text` in ASCII lower case.
[[nodiscard]] std::string ascii_lower(std::string_view text);

/// C `%.12g` (the number format of "Stimulus definitions" and "Supported
/// analyses").
[[nodiscard]] std::string format_g12(double value);

}  // namespace hefesto::sim
