// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The magic-compat splits of extract/cap, one named function per decision
// (openspec/changes/m7-extraction/design.md D9). ON copies Magic; OFF does
// what the code intends. Each has a unit test per mode
// (tests/unit/extract/cap/compat_branches_test.cpp).
//
// Owner: EXTCAP.

#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::extract::cap {

/// Q-EXTRACT-17 / Q-EXTRACT-18 / the corner fringe: whether the shielded
/// (or accounted) amount is summed over every matching sideoverlap rule
/// (ON, as Magic: ExtCouple.c:1560-1600, :1780-1782, extCornerOverlap) or
/// counted once per overlapped tile (OFF).
[[nodiscard]] bool shield_counted_per_rule(const compat::CompatSettings& compat) noexcept;

/// Q-EXTRACT-17 (and the corner fringe): the inside type whose fringe
/// multiplier the shield fractions use: the boundary's stored type, a
/// contact unreduced (ON, ExtCouple.c:688), or its residue (OFF).
[[nodiscard]] tech::TileType shield_inside_type(const compat::CompatSettings& compat, tech::TileType stored,
                                                tech::TileType residue) noexcept;

/// Q-EXTRACT-17 (and the corner fringe): whether a rule's shielded
/// substrate fraction counts only when the two nodes differ (ON) or always
/// (OFF).
[[nodiscard]] bool shield_needs_distinct_nodes(const compat::CompatSettings& compat) noexcept;

/// Q-EXTRACT-18: the length that caps the legacy sideoverlap substrate
/// subtraction: the target tile's overlap with the whole boundary segment
/// (ON) or with the clipped sub-segment being processed (OFF).
[[nodiscard]] std::int64_t legacy_cap_length(const compat::CompatSettings& compat, std::int64_t whole_segment,
                                             std::int64_t sub_segment) noexcept;

/// Q-EXTRACT-18: the area the legacy subtraction accounts for, given the
/// unshielded area of each matching rule in rule order: their sum (ON) or
/// the first one (OFF; 0 with no rule).
[[nodiscard]] std::int64_t legacy_accounted_area(const compat::CompatSettings& compat,
                                                 std::span<const std::int64_t> rule_areas) noexcept;

/// What the corner tests know about the two tiles at a corner (the tile
/// continuing the inside and the tile diagonally beyond the endpoint) and
/// the boundary's outside tile (the free quadrant of a concave corner).
struct CornerTiles {
  bool types_connect = false;     // both types are in the node type's connect mask
  bool same_node = false;         // both belong to the node
  bool quadrant_is_space = false; // the outside tile is space
};

/// Q-EXTRACT-24: whether the substrate corner fringe treats a corner as
/// concave: by Magic's type test (ON, ExtBasic.c:5434-5438), or by the
/// coupling test, same node and a free quadrant of space (OFF).
[[nodiscard]] bool substrate_corner_concave(const compat::CompatSettings& compat, const CornerTiles& tiles) noexcept;

/// Q-EXTRACT-22: whether the "Ports ... are electrically shorted" warning
/// is suppressed: whenever lumped resistance is on (ON, ExtBasic.c:826-829)
/// or only when extresist runs in the same pass (OFF).
[[nodiscard]] bool shorted_ports_warning_suppressed(const compat::CompatSettings& compat, bool lumped,
                                                    bool extresist) noexcept;

/// No Q-row yet (reported for a decision): whether the per-class area and
/// perimeter of a node are stored: only when lumped resistance is on (ON,
/// ExtBasic.c:5205/5477, extSetResist is their only writer) or always (OFF,
/// as extract-resistance-lumped "Per-class area and perimeter of a node"
/// states).
[[nodiscard]] bool class_values_stored(const compat::CompatSettings& compat, bool lumped) noexcept;

}  // namespace hefesto::extract::cap
