// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The board's substrate slab (substrate.hpp; Q-MISC-84/85, tasks.md 16.11).
//
// Owner: SECTION (M10 round 5).

#include "hefesto/view3d/stack/substrate.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hefesto::view3d {

namespace {

/// The z range [bottom, top] of the instances the mesh draws, in draw units.
struct ZRange {
  float bottom = std::numeric_limits<float>::infinity();
  float top = -std::numeric_limits<float>::infinity();

  void add(DrawHeight h) {
    bottom = std::min(bottom, h.height);
    top = std::max(top, h.height + h.thickness);
  }
  [[nodiscard]] bool empty() const { return !(bottom <= top); }
};

DrawHeight layer_height(const MeshModel& mesh, const LayerStack& stack, LayerId layer) {
  return layer < mesh.layer_heights.size() ? mesh.layer_heights[layer] : stack.draw_height(layer);
}

void add_chunk(ZRange& z, const Chunk& c, const MeshModel& mesh, const LayerStack& stack) {
  for (const RectInstance& r : c.rects) {
    z.add(layer_height(mesh, stack, r.layer));
  }
  for (const SideInstance& s : c.sides) {
    const bool own = (s.flags & side_flag::own_height) != 0;
    z.add(own ? DrawHeight{s.height, s.thickness} : layer_height(mesh, stack, s.layer));
  }
  for (const Triangle& t : c.triangles) {
    z.add(DrawHeight{t.height, t.thickness});
  }
}

}  // namespace

std::optional<SubstrateSlab> substrate_slab(const MeshModel& mesh, const LayerStack& stack) {
  if (mesh.empty() || mesh.region.width() <= 0 || mesh.region.height() <= 0) {
    return std::nullopt;
  }
  ZRange z;
  for (const Chunk& c : mesh.chunks) {
    add_chunk(z, c, mesh, stack);
  }
  const float stack_height = z.top - z.bottom;
  if (z.empty() || !std::isfinite(stack_height) || !(stack_height > 0.0F)) {
    return std::nullopt;
  }
  SubstrateSlab slab;
  slab.area = mesh.region;
  slab.thickness = kSubstrateFraction * stack_height;
  slab.height = z.bottom - slab.thickness;
  return slab;
}

}  // namespace hefesto::view3d
