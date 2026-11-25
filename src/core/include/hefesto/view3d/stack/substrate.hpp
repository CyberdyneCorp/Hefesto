// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The board's substrate slab (hefesto-3d-visualization "Interactive 3D
// inspection"; Q-MISC-84/85, the approved board look; the item Q-MISC-85
// deferred in round 4; openspec/changes/m10-3d-view tasks.md 16.11): a
// synthetic, neutral slab under the stack that the desktop 3D tab draws in
// both magic-compat modes. It is not layout: it is not in the mesh model,
// the face log, the visibility sets (`see`), picking or export; the
// renderer draws it only when render::DrawParams::substrate is set, which
// only the desktop tab does.
//
// Owner: SECTION (M10 round 5).

#pragma once

#include <array>
#include <optional>

#include "hefesto/db/geometry.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

/// The slab's thickness as a fraction of the drawn stack's height (the
/// lowest bottom to the highest top of the layers the mesh draws), about
/// the board's proportion.
inline constexpr float kSubstrateFraction = 0.2F;

/// The slab's colour, display (sRGB) values: the board's substrate top
/// (#2A2F37), a neutral dark grey; the renderer shades its faces as any
/// other face (board lighting).
inline constexpr std::array<float, 3> kSubstrateColor{42.0F / 255.0F, 47.0F / 255.0F, 55.0F / 255.0F};

/// A box under the stack: x and y over `area` (root coordinates, as the
/// mesh; the renderer applies DrawParams::xy_scale), z from `height` to
/// `height + thickness` in draw units (as DrawHeight: the renderer scales
/// them by s_z), so its top is the lowest drawn layer's bottom.
struct SubstrateSlab {
  db::Rect area;
  float height = 0.0F;
  float thickness = 0.0F;
  std::array<float, 3> color = kSubstrateColor;

  friend bool operator==(const SubstrateSlab&, const SubstrateSlab&) = default;
};

/// The slab of `mesh`: its region's x/y extent; its top at the lowest
/// bottom (h) of every instance the mesh draws (a layer's
/// MeshModel::layer_heights entry, or the stack's draw_height when the
/// model lists none; a split-tile triangle's or own_height side face's own
/// h), whatever the visibility; thickness kSubstrateFraction of the drawn
/// stack's height. Nullopt for an empty mesh or region, and for a flat
/// stack (zero or non-finite height), which has no thickness to scale by.
[[nodiscard]] std::optional<SubstrateSlab> substrate_slab(const MeshModel& mesh, const LayerStack& stack);

}  // namespace hefesto::view3d
