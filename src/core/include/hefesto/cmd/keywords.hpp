// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Keyword matching and direction/position names shared by every editing
// command (capability cmd-coordinates-and-units, requirements "Keyword
// matching" and "Direction and position names"). This is the one copy of
// Magic's unique-prefix lookup (utils/lookup.c); the duplicates in
// db/command_util and view/command_util are removed onto it (EDIT task 3.1),
// and cmd::Registry uses the same rule for command names (DISPATCH task 7.1).
// (Magic ref: utils/lookup.c:40-126, utils/geometry.c:353-450)
//
// Owner: EDIT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::cmd {

/// How a word matched a keyword table.
enum class Match : std::uint8_t { found, ambiguous, unrecognized };

struct LookupResult {
  Match match = Match::unrecognized;
  std::size_t index = 0;  // the selected entry when `match == found`
};

/// Magic's Lookup(): case-insensitive; an exact match wins, then a unique
/// prefix; only the leading word of each entry (up to the first space)
/// takes part; a leading `::magic::` or `magic::` qualifier is ignored.
[[nodiscard]] LookupResult lookup(std::string_view word, std::span<const std::string_view> table);

/// The directions and positions of "Direction and position names".
enum class Direction : std::uint8_t { north, south, east, west, northeast, northwest, southeast, southwest, center };

/// True for north, south, east and west.
[[nodiscard]] constexpr bool is_manhattan(Direction d) noexcept {
  return d == Direction::north || d == Direction::south || d == Direction::east || d == Direction::west;
}

/// Parses a direction name with the keyword rule. Fails with the "ambiguous"
/// or "not a valid direction or position" error (followed by the legal
/// names); with `manhattan_only`, every non-Manhattan name fails with "not a
/// Manhattan direction or position".
[[nodiscard]] diag::Outcome<Direction> parse_direction(std::string_view word, bool manhattan_only = false);

/// Parses a position name (the same table; `center` included).
[[nodiscard]] diag::Outcome<Direction> parse_position(std::string_view word);

}  // namespace hefesto::cmd
