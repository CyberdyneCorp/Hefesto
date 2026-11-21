// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Bus and vector notation (openspec/changes/m8-schematic design.md "Area
// contracts" NETLIST; capability schematic-netlisting "Bus and vector
// notation").
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::schem {

/// Most single names one expansion may produce. Larger expansions are
/// treated like a syntax error (the string is used unchanged with unknown
/// width) and flagged by `too_large`. Proposed limit, pending approval.
inline constexpr std::size_t kMaxBusBits = std::size_t{1} << 20;

struct BusExpansion {
  /// The single names in order; the input itself after an error.
  std::vector<std::string> bits;
  /// False after a syntax error and for `name[name]` items.
  bool width_known = true;
  /// The input is not valid bus notation ("syntax error in <string>").
  bool syntax_error = false;
  /// The expansion would exceed kMaxBusBits.
  bool too_large = false;

  /// The width, which is 1 when the width is unknown.
  [[nodiscard]] std::size_t width() const noexcept { return width_known ? bits.size() : 1; }
};

/// Expands `name` by the grammar of "Bus and vector notation".
[[nodiscard]] BusExpansion parse_bus(std::string_view name);

/// The bit names of a net or instance name in bus notation, in order; a
/// plain name, or one with a syntax error, gives itself.
[[nodiscard]] std::vector<std::string> expand_bus(std::string_view name);

/// `name` with every `[` and `]` replaced by the first and second character
/// of `replacement` (option `bus_replacement_char`); unchanged unless
/// `replacement` has exactly two characters.
[[nodiscard]] std::string replace_bus_chars(std::string_view name, std::string_view replacement);

/// The bits joined with `separator`.
[[nodiscard]] std::string join_bits(std::span<const std::string> bits, std::string_view separator);

}  // namespace hefesto::schem
