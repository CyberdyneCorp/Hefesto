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
// As landed (M10 round 5, task 11.1; design.md "As-landed: round 5
// SECTION"):
// - The line samples the material on its upper side (a horizontal line at
//   y = c covers a rectangle when y0 <= c < y1) or on its right side (a
//   vertical line at x = c: x0 <= c < x1), Magic's tile convention that a
//   tile owns its bottom and left edges, so a line along a tile edge is not
//   ambiguous. A split-tile triangle covers the part of the line inside it
//   by the same rule.
// - Abutting or overlapping pieces of one layer (rectangles and triangles,
//   from any chunk) are merged into one interval.
// - Heights: the layer's LayerStack::physical_height_um, or, for a layer
//   whose MeshModel::layer_heights entry differs from the stack's
//   draw_height (a contact spanned by the J1 option), that entry in µm. A
//   split-tile triangle is reported with its layer's height in both
//   magic-compat modes (Magic's 3D window draws it with its left type's,
//   Q-MISC-79; a section is µm output and not a Magic feature, Q-MISC-72).
// - Positions and heights are µm in both magic-compat modes (Q-MISC-72).
//
// Owner: SCAFFOLD (this header, M10 task 2.3); INSPECT fills in
// src/core/src/view3d/section/ (design D3, task 11.1).

#pragma once

#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/mode.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

/// An axis-aligned cut line (the board's A-A') in the mesh's coordinates:
/// root coordinates times MeshModel::units_per_root (mask units in
/// mask-layer mode).
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
  std::vector<LayerId> no_height;   // layers the line crosses that have no physical height, in layer order
  /// The line's length in µm (0 when it is refused or µm are unknown).
  double length_um = 0.0;
  /// VIEW3D-SECTION-001 when the line is refused (not axis-aligned, or of
  /// zero length: the section is then empty); VIEW3D-STACK-001 (mask-layer
  /// mode) or VIEW3D-SECTION-002 (layout-layer mode) naming the layers of
  /// `no_height`. The caller reports them.
  std::vector<diag::Diagnostic> diagnostics;
};

/// The cross-section of `mesh` along `line`, every layer of the mesh.
[[nodiscard]] CrossSection section(const MeshModel& mesh, const LayerStack& stack, const Segment& line);

/// The same, leaving out the layers flagged in `hidden` (per stack layer,
/// as render::DrawParams::hidden; layers past its end are visible).
[[nodiscard]] CrossSection section(const MeshModel& mesh, const LayerStack& stack, const Segment& line,
                                   const std::vector<bool>& hidden);

}  // namespace hefesto::view3d
