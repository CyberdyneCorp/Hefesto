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
#include <cstdint>
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

/// Most bytes of expanded bus text, commas included, one netlist run or one
/// editing session may produce in total: 64 x kMaxBusBytes (Q-SCHEM-41,
/// user decision M8-D3 of 2026-11-22; both modes of `xschem-compat`).
inline constexpr std::uint64_t kMaxBusTotalBytes = std::uint64_t{64} * kMaxBusBytes;

/// The expanded bus text still allowed in one netlist run or editing session
/// (Q-SCHEM-41). Each expansion that builds names takes its text from it; an
/// expansion that no longer fits is used unchanged with unknown width and
/// SCHEM-NET-006, exactly like a single name over kMaxBusBytes. A plain name
/// (nothing to expand) takes nothing.
class BusBudget {
 public:
  explicit BusBudget(std::uint64_t total = kMaxBusTotalBytes) noexcept : remaining_{total} {}

  [[nodiscard]] std::uint64_t remaining() const noexcept { return remaining_; }

  /// Takes `bytes` when that many remain; otherwise takes nothing and
  /// returns false.
  bool take(std::uint64_t bytes) noexcept {
    if (bytes > remaining_) {
      return false;
    }
    remaining_ -= bytes;
    return true;
  }

 private:
  std::uint64_t remaining_;
};

struct BusExpansion {
  /// The single names in order; the input itself after an error.
  std::vector<std::string> bits;
  /// False after a syntax error and for `name[name]` items.
  bool width_known = true;
  /// The input is not valid bus notation ("syntax error in <string>").
  bool syntax_error = false;
  /// The expansion would exceed kMaxBusBits, kMaxBusBytes or
  /// kMaxBusParenDepth, or the BusBudget it was parsed with.
  bool too_large = false;
  /// `too_large` because the text did not fit the remaining BusBudget
  /// (Q-SCHEM-41).
  bool over_budget = false;

  /// The width, which is 1 when the width is unknown.
  [[nodiscard]] std::size_t width() const noexcept { return width_known ? bits.size() : 1; }
};

/// Expands `name` by the grammar of "Bus and vector notation". With a
/// `budget`, the expanded text must also fit its remaining bytes and is taken
/// from it (Q-SCHEM-41); nullptr: only the per-name limits apply.
[[nodiscard]] BusExpansion parse_bus(std::string_view name, BusBudget* budget = nullptr);

/// `parse_bus(name, budget).width()` without building the names or taking
/// from `budget`: the same limits apply, so a name past them has width 1.
[[nodiscard]] std::size_t bus_width(std::string_view name, const BusBudget* budget = nullptr);

/// The warning `parse_bus(name)` gives: SCHEM-NET-002 `syntax error in
/// <name>`, SCHEM-NET-006 `bus expansion of <name> exceeds a limit (...);
/// used unchanged`, or for an expansion over its budget SCHEM-NET-006 `bus
/// expansion of <name> exceeds the total of 4294967296 bytes of expanded bus
/// text per netlist run or session; used unchanged` (the name cut to 80
/// bytes and `...`); nullopt otherwise.
[[nodiscard]] std::optional<diag::Diagnostic> bus_diagnostic(const BusExpansion& bus, std::string_view name);

/// The bit names of a net or instance name in bus notation, in order; a
/// plain name, or one with a syntax error or over a limit, gives itself.
[[nodiscard]] std::vector<std::string> expand_bus(std::string_view name, BusBudget* budget = nullptr);

/// `name` with every `[` and `]` replaced by the first and second character
/// of `replacement` (option `bus_replacement_char`); unchanged unless
/// `replacement` has exactly two characters.
[[nodiscard]] std::string replace_bus_chars(std::string_view name, std::string_view replacement);

/// The bits joined with `separator`.
[[nodiscard]] std::string join_bits(std::span<const std::string> bits, std::string_view separator);

}  // namespace hefesto::schem
