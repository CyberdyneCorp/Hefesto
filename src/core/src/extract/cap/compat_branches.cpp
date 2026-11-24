// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The magic-compat splits of extract/cap (compat_branches.hpp; design D9).
// Owner: EXTCAP.

#include "hefesto/extract/cap/compat_branches.hpp"

namespace hefesto::extract::cap {

bool shield_counted_per_rule(const compat::CompatSettings& compat) noexcept {
  return compat.magic_compat;
}

tech::TileType shield_inside_type(const compat::CompatSettings& compat, tech::TileType stored,
                                  tech::TileType residue) noexcept {
  return compat.magic_compat ? stored : residue;
}

bool shield_needs_distinct_nodes(const compat::CompatSettings& compat) noexcept {
  return compat.magic_compat;
}

std::int64_t legacy_cap_length(const compat::CompatSettings& compat, std::int64_t whole_segment,
                               std::int64_t sub_segment) noexcept {
  return compat.magic_compat ? whole_segment : sub_segment;
}

std::int64_t legacy_accounted_area(const compat::CompatSettings& compat,
                                   std::span<const std::int64_t> rule_areas) noexcept {
  if (rule_areas.empty()) {
    return 0;
  }
  if (!compat.magic_compat) {
    return rule_areas.front();
  }
  std::int64_t sum = 0;
  for (const std::int64_t area : rule_areas) {
    sum += area;
  }
  return sum;
}

bool substrate_corner_concave(const compat::CompatSettings& compat, const CornerTiles& tiles) noexcept {
  if (compat.magic_compat) {
    return tiles.types_connect;
  }
  return tiles.same_node && tiles.quadrant_is_space;
}

bool shorted_ports_warning_suppressed(const compat::CompatSettings& compat, bool lumped, bool extresist) noexcept {
  return compat.magic_compat ? lumped : extresist;
}

bool class_values_stored(const compat::CompatSettings& compat, bool lumped) noexcept {
  return !compat.magic_compat || lumped;
}

}  // namespace hefesto::extract::cap
