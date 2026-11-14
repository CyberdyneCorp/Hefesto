// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The bloat-all flood engine shared by bloat-all and tagged (capability
// mask-output-operators "bloat-all", "tagged"; design D6): Magic's
// cifBloatAllFunc with its seed searches, the clipped flood and the mark
// resets (Q-MASK-230, 231, 232). Private to src/core/src/mask/ops/.
//
// Owner: OPS.

#pragma once

#include <vector>

#include "mask/ops/kernel.hpp"

namespace hefesto::mask::detail {

/// Magic's BloatData of one bloat-all (or tagged) evaluation.
struct BloatAllConfig {
  /// bl_plane: the layout plane of the bloated types, or -1 when the
  /// bloated layers are mask layers.
  int bloat_plane = -1;
  /// The connecting types (bl_distance != 0), or {solid} for mask layers.
  tech::TypeMask connect;
  /// The bloated mask layers (bl_plane = -1), in layer order.
  std::vector<const MaskPlane*> layers;
  /// The trigger types (co_paintMask), reset with `connect` on the bloat
  /// plane.
  tech::TypeMask trigger_types;
  /// The clip distance (co_distance); 0 = unclipped.
  db::Coord distance = 0;
};

/// One bloat-all evaluation: triggers are fed in Magic's order, then
/// finish() resets the marks (and keeps the stale ones under
/// magic-compat ON).
class BloatAll {
 public:
  BloatAll(const OpArgs& args, OperatorContext& ctx, const BloatAllConfig& cfg, MaskPlane& out);
  BloatAll(const BloatAll&) = delete;
  BloatAll& operator=(const BloatAll&) = delete;

  /// cifBloatAllFunc for trigger `t` found on `source`; `scale` is the
  /// cifScale in effect (the style's for a layout tile, 1 for a mask tile).
  void trigger(const db::TileView& t, const Source& source, std::int32_t scale);
  /// The mark resets after every trigger.
  void finish();

 private:
  struct Pending {
    db::TileView tile;
    bool side = false;  // TT_SIDE of the pushed dinfo
    const db::Plane* plane = nullptr;
  };

  [[nodiscard]] Mark mark(const db::TileView& t) const;
  void set_mark(const db::TileView& t, const db::Plane* plane, Mark m);
  void push(const db::TileView& t, bool side, const db::Plane* plane);
  void seed(const db::Plane& plane, const db::Rect& area, const tech::TypeMask& mask);
  void flood(std::int32_t loc_scale, const db::Rect& clip);
  void paint_tile(const Pending& p, const db::Rect& a, const db::Rect& clipped);
  void push_neighbors(const Pending& p, const db::Rect& a, const db::Rect& clip);
  [[nodiscard]] const db::Plane* bloat_plane() const;
  [[nodiscard]] tech::PlaneMask coincident_planes(tech::TileType type) const;

  const OpArgs& args_;
  OperatorContext& ctx_;
  const BloatAllConfig& cfg_;
  MaskPlane& out_;
  std::unordered_map<const void*, StaleMarks::Entry> where_;  // plane and types of each marked tile
  Marks marks_;
  std::vector<Pending> stack_;
  std::vector<db::TileView> reset_;
};

}  // namespace hefesto::mask::detail
