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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::schem {

/// Input limits of "Bus and vector notation" (Q-SCHEM-32, approved
/// 2026-11-21; both modes of `xschem-compat`). An expansion past one of them
/// is used unchanged with unknown width, like a syntax error, flagged by
/// `too_large` and reported as SCHEM-NET-006 (bus_diagnostic()).
/// Most single names one expansion may produce.
inline constexpr std::size_t kMaxBusBits = std::size_t{1} << 20;
/// Most bytes of expanded text, commas included, one expansion may produce.
inline constexpr std::size_t kMaxBusBytes = std::size_t{64} << 20;
/// Deepest parenthesis nesting of one name.
inline constexpr int kMaxBusParenDepth = 256;

struct BusExpansion {
  /// The single names in order; the input itself after an error.
  std::vector<std::string> bits;
  /// False after a syntax error and for `name[name]` items.
  bool width_known = true;
  /// The input is not valid bus notation ("syntax error in <string>").
  bool syntax_error = false;
  /// The expansion would exceed kMaxBusBits, kMaxBusBytes or
  /// kMaxBusParenDepth.
  bool too_large = false;

  /// The width, which is 1 when the width is unknown.
  [[nodiscard]] std::size_t width() const noexcept { return width_known ? bits.size() : 1; }
};

/// Expands `name` by the grammar of "Bus and vector notation".
[[nodiscard]] BusExpansion parse_bus(std::string_view name);

/// The warning `parse_bus(name)` gives: SCHEM-NET-002 `syntax error in
/// <name>` or SCHEM-NET-006 `bus expansion of <name> exceeds a limit (...);
/// used unchanged` (the name cut to 80 bytes and `...`); nullopt otherwise.
[[nodiscard]] std::optional<diag::Diagnostic> bus_diagnostic(const BusExpansion& bus, std::string_view name);

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
