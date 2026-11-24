// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pick resolution (pick.hpp; openspec/changes/m10-3d-view/design.md D10,
// tasks.md 9.3): an id to its instance (pick_id.hpp), the instance to its
// layer and root rectangle, the layer to its tile type through the stack
// (a (type, style) pair maps back to its type, Q-MISC-77), and the source
// tile found by one hierarchical tile search of the rendered cell under
// the face, so the mesh keeps no side table.
//
// Owner: INSPECT (design D3).

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "hefesto/connect/tree.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "view3d/pick/internal.hpp"

namespace hefesto::view3d {

namespace detail {

namespace {

db::Coord to_coord(double origin, float offset) noexcept {
  const double v = std::round(origin + static_cast<double>(offset));
  const double lo = static_cast<double>(std::numeric_limits<db::Coord>::min());
  const double hi = static_cast<double>(std::numeric_limits<db::Coord>::max());
  return static_cast<db::Coord>(std::clamp(v, lo, hi));
}

db::Rect rect_of(const Chunk& c, float x0, float y0, float x1, float y1) noexcept {
  const db::Coord ax = to_coord(c.origin_x, x0);
  const db::Coord bx = to_coord(c.origin_x, x1);
  const db::Coord ay = to_coord(c.origin_y, y0);
  const db::Coord by = to_coord(c.origin_y, y1);
  return db::Rect{std::min(ax, bx), std::min(ay, by), std::max(ax, bx), std::max(ay, by)};
}

db::Rect triangle_bbox(const Chunk& c, const Triangle& t) noexcept {
  const auto [xmin, xmax] = std::minmax({t.x[0], t.x[1], t.x[2]});
  const auto [ymin, ymax] = std::minmax({t.y[0], t.y[1], t.y[2]});
  return rect_of(c, xmin, ymin, xmax, ymax);
}

/// The strip one unit wide on the inner side of an axis-aligned side.
db::Rect inner_strip(const SideInstance& s, db::Rect seg) noexcept {
  const float dx = s.x1 - s.x0;
  const float dy = s.y1 - s.y0;
  // Outward to the left of the direction: the inside is on its right.
  const bool out_left = (s.flags & side_flag::outward_left) != 0;
  const float inward_x = out_left ? dy : -dy;
  const float inward_y = out_left ? -dx : dx;
  if (seg.y0 == seg.y1) {
    return inward_y < 0.0F ? db::Rect{seg.x0, seg.y0 - 1, seg.x1, seg.y0} : db::Rect{seg.x0, seg.y0, seg.x1, seg.y0 + 1};
  }
  return inward_x < 0.0F ? db::Rect{seg.x0 - 1, seg.y0, seg.x0, seg.y1} : db::Rect{seg.x0, seg.y0, seg.x0 + 1, seg.y1};
}

}  // namespace

LayerId layer_of(const Chunk& chunk, const PickRef& ref) noexcept {
  switch (ref.kind) {
    case PickKind::rect:
      return chunk.rects[ref.index].layer;
    case PickKind::side:
      return chunk.sides[ref.index].layer;
    case PickKind::triangle:
      return chunk.triangles[ref.index].layer;
  }
  return 0;
}

db::Rect face_rect(const Chunk& chunk, const PickRef& ref) noexcept {
  switch (ref.kind) {
    case PickKind::rect: {
      const RectInstance& r = chunk.rects[ref.index];
      return rect_of(chunk, r.x0, r.y0, r.x1, r.y1);
    }
    case PickKind::side: {
      const SideInstance& s = chunk.sides[ref.index];
      return rect_of(chunk, s.x0, s.y0, s.x1, s.y1);
    }
    case PickKind::triangle:
      return triangle_bbox(chunk, chunk.triangles[ref.index]);
  }
  return db::Rect{};
}

db::Rect probe_rect(const Chunk& chunk, const PickRef& ref) noexcept {
  const db::Rect face = face_rect(chunk, ref);
  if (ref.kind != PickKind::side || (face.x0 != face.x1 && face.y0 != face.y1)) {
    return face;  // a rectangle, a triangle's box or a diagonal side's box
  }
  return inner_strip(chunk.sides[ref.index], face);
}

}  // namespace detail

namespace {

std::vector<std::string> split_path(const std::string& path) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start < path.size()) {
    const std::size_t slash = path.find('/', start);
    const std::size_t end = slash == std::string::npos ? path.size() : slash;
    out.push_back(path.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

/// Fills in the source tile: the first tile of the target's type under
/// `probe` (root coordinates) in the rendered cell or a cell below it.
void find_source_tile(const PickContext& context, const db::Rect& probe, PickTarget& target) {
  (void)connect::search_tree_tiles(*context.cell, probe, tech::TypeMask::of(*target.type), {}, context.magic_compat,
                                   [&](const connect::TreeTile& t) {
                                     const db::Rect in_root = db::apply(t.to_root, t.tile.area);
                                     if (!db::overlap(in_root, probe)) {
                                       return true;
                                     }
                                     target.cell_rect = t.tile.area;
                                     target.material = in_root;
                                     target.use_path = split_path(t.path != nullptr ? *t.path : std::string{});
                                     return false;
                                   });
}

}  // namespace

std::optional<PickTarget> resolve(const MeshModel& mesh, PickId id) {
  const auto ref = decode_pick(mesh, id);
  if (!ref.has_value()) {
    return std::nullopt;
  }
  const Chunk& chunk = mesh.chunks[ref->chunk];
  PickTarget target;
  target.layer = detail::layer_of(chunk, *ref);
  target.root_rect = detail::face_rect(chunk, *ref);
  target.triangle = ref->kind == PickKind::triangle;
  return target;
}

std::optional<PickTarget> resolve(const MeshModel& mesh, const PickContext& context, PickId id) {
  auto target = resolve(mesh, id);
  if (!target.has_value() || context.stack == nullptr || target->layer >= context.stack->size()) {
    return target;
  }
  target->type = context.stack->layers()[target->layer].type;
  target->cell = context.cell;
  if (target->type.has_value() && context.cell != nullptr) {
    const auto ref = decode_pick(mesh, id);
    find_source_tile(context, detail::probe_rect(mesh.chunks[ref->chunk], *ref), *target);
  }
  return target;
}

}  // namespace hefesto::view3d
