// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The ghost classes of the layer stack (hefesto-3d-visualization
// "Interactive 3D inspection"; Q-MISC-85, approved 2026-11-24; openspec/
// changes/m10-3d-view/design.md "As-landed: round 4 SHELL"): which stack
// layers the desktop 3D tab draws ghosted (translucent, no depth write) by
// default, so wells, implants, substrate and other non-conducting area
// layers do not hide the devices, as the board
// docs/design/mockups/View3D.dc.html ghosts the substrate.
//
// The rule reads the technology's types and planes only:
// - the planes that carry conduction are the home planes of the residues of
//   every contact type (a stacked contact with its full residue set,
//   Technology::full_residues);
// - a layer is ghosted when its tile type is not a contact type and its home
//   plane is not one of those planes.
// sky130A: the dwell, well and comment planes are ghosted; active, locali,
// the metals, cap1/cap2, metali, block (it holds `ubm`, a residue of `pi2`)
// and every contact and via stay opaque. gf180mcuD: dwell, well and comment.
// Mask-layer entries (no tile type) and types without a home plane are
// never ghosted, and a technology without any contact type ghosts nothing
// (no plane can be told apart; SHELL's reading, round 4). Both magic-compat
// modes: the default is Hefesto view state, outside Magic's.
//
// Owner: SHELL (round 4; the classifier only, beside TECH's layer_stack).

#pragma once

#include <vector>

#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

/// The planes holding a residue of some contact type (the home planes of
/// every contact's full residue set).
[[nodiscard]] tech::PlaneMask conducting_planes(const tech::Technology& tech);

/// Whether `type` is ghosted by default: not a contact type, and its home
/// plane is not in `conducting` (conducting_planes). False for space, a
/// type without a home plane, and when `conducting` is empty (a technology
/// without contacts).
[[nodiscard]] bool ghosted_type(const tech::Technology& tech, tech::TileType type, tech::PlaneMask conducting);

/// Per stack entry, whether it is ghosted by default (ghosted_type of its
/// tile type; false for a mask-layer entry).
[[nodiscard]] std::vector<bool> ghosted_by_default(const LayerStack& stack, const tech::Technology& tech);

}  // namespace hefesto::view3d
