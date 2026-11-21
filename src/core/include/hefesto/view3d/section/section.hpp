// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cross-sections (P2; hefesto-3d-visualization "Interactive 3D inspection"
// "Cross-section"; openspec/changes/m10-3d-view/design.md D10): an
// axis-aligned cut line gives, for each layer, the intervals of the line it
// covers, drawn from h to h + t in µm (LayerStack::physical_height_um).
// Built from the mesh model, so it costs no re-flattening. Layers with no
// physical height (mask layers without `render`) are left out and named
// (VIEW3D-STACK-001).
//
// Owner: SCAFFOLD (this header, M10 task 2.3); INSPECT fills in
// src/core/src/view3d/section/ (design D3, task 11.1).

#pragma once

#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/mode.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

/// An axis-aligned cut line in root coordinates (the board's A-A').
struct Segment {
  db::Point a;
  db::Point b;
};

struct CrossSection {
  /// One covered interval of one layer.
  struct Interval {
    LayerId layer = 0;
    double from_um = 0.0;    // along the line, from `a`
    double to_um = 0.0;
    double bottom_um = 0.0;  // h
    double top_um = 0.0;     // h + t
  };
  std::vector<Interval> intervals;  // layer order, then position
  std::vector<LayerId> no_height;   // layers left out (no physical height)
};

/// The cross-section of `mesh` along `line`.
[[nodiscard]] CrossSection section(const MeshModel& mesh, const LayerStack& stack, const Segment& line);

}  // namespace hefesto::view3d
