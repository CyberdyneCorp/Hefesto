// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared writers of CapTables for cap_rules.cpp and default_caps.cpp
// (openspec/changes/m7-extraction/design.md D7).
//
// Owner: EXTTECH.

#pragma once

#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract::detail {

/// One recorded overlap pair: (s, t), the value, the plane of s and the
/// other plane (none for a default rule without one).
struct OverlapEntry {
  tech::TileType s = 0;
  tech::TileType t = 0;
  double cap = 0.0;
  tech::PlaneId p1 = 0;
  std::optional<tech::PlaneId> p2;
};

/// exts_overlapMult[s][t] and [t][s] = (float) cap * FRINGE_MULT.
void set_fringe(CapTables& tables, tech::TileType s, tech::TileType t, double cap);
/// The table writes of one overlap pair (value, fringe, planes, types).
void set_overlap(CapTables& tables, const OverlapEntry& entry);
/// exts_overlapCap[s][t].
[[nodiscard]] double overlap_value(const CapTables& tables, tech::TileType s, tech::TileType t);
/// Prepends `rule` to the list of edge (s|t).
void prepend_rule(std::map<TypePair, std::vector<EdgeRule>>& lists, tech::TileType s, tech::TileType t,
                  const EdgeRule& rule);
/// exts_sidePlanes, exts_sideTypes[plane] and exts_sideEdges[s] for an
/// inside type s.
void add_side_type(CapTables& tables, tech::PlaneId plane, tech::TileType s, const tech::TypeMask& edges);
/// The plane mask DBTechNameMask returns for `text` (whose types are
/// `types`): the planes of its user types, a `/plane` element replacing
/// what came before it, every plane but the router when the list is empty
/// or space alone.
[[nodiscard]] tech::PlaneMask name_mask_planes(const tech::Technology& tech, std::string_view text,
                                               const tech::TypeMask& types);

}  // namespace hefesto::extract::detail
