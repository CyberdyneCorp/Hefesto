// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The maze engine's request and result (capability router-maze;
// Q-LEGACY-41, J4: a session-scoped API with a structured result and the
// style's bloomLimit as the deterministic bound; openspec/changes/
// m10-plot-route/design.md D4, D11, D23, "Area contracts" MZ). The engine
// owns its blockage planes, estimate plane, heaps and point hash per call;
// nothing is static. Painting the result, the selection, redisplay and the
// DRC queue belong to the caller.
//
// Owner: MZ.

#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::route::maze {

/// A start point and the route type (layer or contact) it starts on.
struct StartTerm {
  db::Point point;
  tech::TileType type = tech::kSpace;
};

/// A destination area and its route type.
struct DestArea {
  db::Rect area;
  tech::TileType type = tech::kSpace;
};

/// The read view of the layout the search sees (taken under the session's
/// shared lock, design D16).
struct LayoutView {
  const db::CellDef* root = nullptr;           // not owned
  const tech::Technology* technology = nullptr;  // not owned
  db::Rect reference_area;                     // the reference window area (Magic's routeArea hint)
  int expansion_depth = 0;                     // subcell expansion of the reference window
};

/// Run controls that are not part of the style.
struct RunControls {
  const std::atomic<bool>* cancel = nullptr;  // interrupt or `-timeout`; nullptr = never
  std::size_t search_memory_limit = 0;        // bytes; 0 = no limit (router.max-search-memory, Q-LEGACY-48)
  bool magic_compat = true;
};

struct RouteRequest {
  MazeStyle style;  // value copy, never shared; bloom_limit is the bound
  std::vector<StartTerm> starts;
  std::vector<DestArea> dests;
  LayoutView layout;
  RunControls controls;
};

/// How a search ended (Magic's MZ_* results).
enum class ResultCode : std::uint8_t {
  success,
  best_before_interrupt,
  failure,
  unroutable,
  interrupted,
  already_routed,
};

/// Orientation of a path point (Magic's RoutePath rp_orient).
enum class Orientation : std::uint8_t { horizontal, vertical, other, contact_up, contact_down };

struct PathPoint {
  db::Point point;
  tech::TileType type = tech::kSpace;
  Orientation orientation = Orientation::other;
};

/// One painted rectangle of the result, in root coordinates.
struct PaintRect {
  tech::TileType type = tech::kSpace;
  db::Rect rect;
};
using PaintList = std::vector<PaintRect>;

/// Magic's dlong costs.
using Cost = std::int64_t;

/// Why the search loop stopped.
enum class EndReason : std::uint8_t { none, complete, bloom_limit, interrupted, memory_limit, exhausted };

struct SearchStats {
  std::int64_t blooms = 0;
  std::int64_t points_processed = 0;
  std::size_t heap_peak = 0;
  EndReason end_reason = EndReason::none;
};

struct RouteResult {
  ResultCode code = ResultCode::failure;
  std::vector<PathPoint> path;
  PaintList paint;
  Cost cost = 0;
  Cost initial_estimate = 0;
  SearchStats stats;
  std::vector<diag::Diagnostic> messages;  // the engine's own lines (fence choice, zero length, statistics)
};

}  // namespace hefesto::route::maze
