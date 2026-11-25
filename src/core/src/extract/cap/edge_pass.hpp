// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private state of the edge pass (Magic's extSidewallStruct): the boundary
// extAddCouple started from, its plane, the fringe model and the rule lists
// of its edge, shared by the shadowing walk (edges.cpp), the sideoverlap
// handlers (sideoverlap.cpp) and the corner fringe (corners.cpp).
//
// Owner: EXTCAP.

#pragma once

#include <utility>
#include <vector>

#include "cap_context.hpp"

namespace hefesto::extract::cap {

struct EdgeState {
  const Context* ctx = nullptr;
  Kinds kinds;
  Boundary bp;  // esws->bp: the boundary of extAddCouple, clipped to the search area
  tech::PlaneId plane = 0;
  bool fringe_halo = false;
  db::Rect area;  // esws->area: the fringe search area of the current call
  const std::vector<EdgeRule>* couple_list = nullptr;   // exts_sideCoupleCap[tin][tout]
  const std::vector<EdgeRule>* overlap_list = nullptr;  // exts_sideOverlapCap[tin][tout]
  tech::TileType tin = 0;   // residues of the boundary types
  tech::TileType tout = 0;
};

/// extGetBoundaryTypes: the inside and outside types facing the boundary
/// (contacts not reduced).
[[nodiscard]] std::pair<tech::TileType, tech::TileType> boundary_types(const Boundary& b);

/// The node of a boundary's inside tile (ExtGetRegion(b_inside, 0)).
[[nodiscard]] Node inside_node(const EdgeState& es);

/// extFindOverlap: the sideoverlap search over `area` (the walk sub-area).
void find_overlap(EdgeState& es, const db::Rect& area);
/// extRemoveSubcap: the substrate fringe blocked by a same-plane neighbour.
void remove_subcap(const EdgeState& es, const Boundary& bloc, const db::Rect& clip);
/// extAddCornerCouple for a horizontal boundary.
void add_corner_couple(EdgeState& es);

/// 0.6366 * (atan(mult * far) - atan(mult * near)) style products of the
/// corner model (extCornerFrac).
[[nodiscard]] double corner_frac(const db::Rect& r, db::Point corner, int quadrant, double mult);

/// Quadrants of extCornerStruct (GEO_NORTHEAST, ...).
inline constexpr int kNorthEast = 0;
inline constexpr int kNorthWest = 1;
inline constexpr int kSouthEast = 2;
inline constexpr int kSouthWest = 3;

}  // namespace hefesto::extract::cap
