// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of view3d/pick: an instance's geometry in root
// coordinates (pick.cpp, highlight.cpp) and the net search shared by net
// highlighting and the net overlay (net.cpp). Owner: INSPECT; the net
// search: CORE (round 6).

#pragma once

#include "hefesto/connect/search.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/pick/pick_id.hpp"

namespace hefesto::view3d::detail {

/// The layer of the instance `ref` names (which must exist).
[[nodiscard]] LayerId layer_of(const Chunk& chunk, const PickRef& ref) noexcept;

/// The instance's face in root coordinates: the rectangle, the bounding box
/// of a triangle, or the (zero-width) bounding box of a side segment.
[[nodiscard]] db::Rect face_rect(const Chunk& chunk, const PickRef& ref) noexcept;

/// An area of positive extent inside the material the instance shows: the
/// rectangle; a triangle's bounding box; for an axis-aligned side, the
/// strip one unit wide on its inner side (opposite to its outward normal);
/// for a diagonal side, its bounding box.
[[nodiscard]] db::Rect probe_rect(const Chunk& chunk, const PickRef& ref) noexcept;

/// Collects a net into a scratch cell in root coordinates: each tile is
/// painted into the plane it was reported on (a contact on its home plane).
class NetPaint final : public connect::NetVisitor {
 public:
  explicit NetPaint(const tech::Technology& tech);

  bool on_tile(const connect::NetTile& t) override;

  [[nodiscard]] const db::CellDef& cell() const noexcept { return cell_; }
  [[nodiscard]] const tech::Technology& technology() const noexcept { return *tech_; }

 private:
  const tech::Technology* tech_;
  db::CellDef cell_;
};

/// Whether `seed` can start a net search under `options`: it has a tile
/// type, a rendered cell and a source tile, and a flat search needs the
/// tile in the rendered cell itself.
[[nodiscard]] bool usable_seed(const PickTarget& seed, const NetOptions& options) noexcept;

/// The net of the `type` material under `area` of `cell` (root
/// coordinates) through `connect` (options' scope and compat setting),
/// painted into `net`. False when the search failed.
bool search_net(const db::CellDef& cell, const db::Rect& area, tech::TileType type, const NetOptions& options,
                NetPaint& net);

}  // namespace hefesto::view3d::detail
