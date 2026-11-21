// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mesh builders (openspec/changes/m10-3d-view/design.md D6).
// build_from_layout: flatten and clip (flatten.cpp), plan the chunks
// (chunks.cpp), then emit each chunk, and in each chunk the layers in
// stack order, each layer's tiles in its plane's enumeration order
// (faces.cpp). Chunks are emitted nearest the view centre first, each
// handed to `on_chunk` as it finishes (progressive display), and the model
// keeps them in Morton order, so two builds of the same cell are equal.
// Every chunk and the model get version 1; a cancelled build returns
// nullopt and the caller keeps the previous model.
// rebuild_chunks (task 6.4): the chunks meeting a dirty rectangle grown by
// 1 (a tile side on the dirty boundary belongs to the chunk next to it)
// are rebuilt from a flatten of their areas and the dirty areas, each
// grown by 1 so their boundary faces see their neighbours; new chunks are
// planned where changed paint fell outside every chunk; rebuilt chunks
// left empty are dropped. Untouched chunks are kept as they were.
// build_from_planes (MASKSRC, P2) is still the scaffold stub: an empty
// model over the clip area.
//
// Owner: MESH (design D3).

#include <algorithm>
#include <array>

#include "hefesto/view3d/mesh/build.hpp"
#include "internal.hpp"

namespace hefesto::view3d {

namespace {

MeshModel empty_model(const LayerStack& stack, const ClipArea& clip) {
  MeshModel model;
  model.mode = stack.mode();
  model.region = clip.area;
  return model;
}

Chunk build_chunk(const detail::FlatLayout& flat, const detail::HeightTable& heights, const detail::ChunkCell& cell,
                  const db::Rect& clip, std::uint64_t version) {
  Chunk chunk;
  chunk.id = cell.id;
  chunk.version = version;
  chunk.area = cell.area;
  chunk.origin_x = cell.area.x0;
  chunk.origin_y = cell.area.y0;
  for (std::size_t i = 0; i < heights.layers().size() && i < flat.of_layer.size(); ++i) {
    const std::ptrdiff_t index = flat.of_layer[i];
    if (index < 0) {
      continue;
    }
    detail::emit_layer(flat.types[static_cast<std::size_t>(index)], static_cast<LayerId>(i), heights, clip, chunk);
  }
  return chunk;
}

[[nodiscard]] bool chunk_empty(const Chunk& c) noexcept {
  return c.rects.empty() && c.sides.empty() && c.triangles.empty();
}

// The squared distance from `p` to the centre of `r` (doubled coordinates).
[[nodiscard]] double distance2(const db::Rect& r, db::Point p) noexcept {
  const double dx = (static_cast<double>(r.x0) + static_cast<double>(r.x1)) - (2.0 * static_cast<double>(p.x));
  const double dy = (static_cast<double>(r.y0) + static_cast<double>(r.y1)) - (2.0 * static_cast<double>(p.y));
  return (dx * dx) + (dy * dy);
}

// `plan` nearest `centre` first; ties in Morton order.
void order_by_centre(std::vector<detail::ChunkCell>& plan, db::Point centre) {
  std::stable_sort(plan.begin(), plan.end(), [centre](const detail::ChunkCell& a, const detail::ChunkCell& b) {
    const double da = distance2(a.area, centre);
    const double db = distance2(b.area, centre);
    return da != db ? da < db : a.id < b.id;
  });
}

[[nodiscard]] db::Point centre_of(const db::Rect& r) noexcept {
  return db::Point{static_cast<db::Coord>((std::int64_t{r.x0} + r.x1) / 2),
                   static_cast<db::Coord>((std::int64_t{r.y0} + r.y1) / 2)};
}

void sort_by_id(std::vector<Chunk>& chunks) {
  std::sort(chunks.begin(), chunks.end(), [](const Chunk& a, const Chunk& b) { return a.id < b.id; });
}

// Emits the chunks of `plan` nearest the view centre first, reporting each
// and the progress, into `out` (in Morton order). False when cancelled.
bool emit_chunks(const detail::FlatLayout& flat, const detail::HeightTable& heights,
                 std::vector<detail::ChunkCell> plan, const db::Rect& clip, std::uint64_t version, BuildContext& ctx,
                 std::vector<Chunk>& out) {
  order_by_centre(plan, ctx.centre.value_or(centre_of(clip)));
  for (std::size_t i = 0; i < plan.size(); ++i) {
    if (ctx.progress.cancelled()) {
      return false;
    }
    out.push_back(build_chunk(flat, heights, plan[i], clip, version));
    if (ctx.on_chunk) {
      ctx.on_chunk(out.back());
    }
    ctx.progress.update(static_cast<double>(i + 1) / static_cast<double>(plan.size()));
  }
  sort_by_id(out);
  return true;
}

// Each dirty rectangle grown by 1 and clipped to `region`, without the
// ones that fall outside it.
std::vector<db::Rect> grown_dirty(std::span<const db::Rect> dirty, const db::Rect& region) {
  std::vector<db::Rect> out;
  for (const db::Rect& d : dirty) {
    const db::Rect g = detail::intersect(db::expand(d, 1), region);
    if (!g.is_null()) {
      out.push_back(g);
    }
  }
  return out;
}

[[nodiscard]] bool meets_any(const db::Rect& r, std::span<const db::Rect> areas) {
  return std::any_of(areas.begin(), areas.end(), [&r](const db::Rect& a) { return !detail::intersect(r, a).is_null(); });
}

// The rebuild plan: the previous chunks meeting `changed`, flagged in
// `affected`, then the new leaves (from `flat`).
struct RebuildPlan {
  std::vector<bool> affected;
  std::vector<db::Rect> flatten_areas;
  std::vector<db::Rect> existing;
};

RebuildPlan affected_chunks(const MeshModel& previous, const std::vector<db::Rect>& changed) {
  RebuildPlan plan;
  plan.flatten_areas = changed;
  plan.affected.reserve(previous.chunks.size());
  for (const Chunk& c : previous.chunks) {
    const bool hit = meets_any(c.area, changed);
    plan.affected.push_back(hit);
    plan.existing.push_back(c.area);
    if (hit) {
      plan.flatten_areas.push_back(detail::intersect(db::expand(c.area, 1), previous.region));
    }
  }
  return plan;
}

}  // namespace

namespace detail {

std::optional<MeshModel> build_layout(const LayerStack& stack, const db::CellDef& cell, const ClipArea& clip,
                                      BuildContext& ctx, std::size_t max_tiles) {
  MeshModel model = empty_model(stack, clip);
  model.version = 1;
  FlatLayout flat;
  const std::array<db::Rect, 1> areas{clip.area};
  if (!flatten_snapshot(stack, cell, areas, ctx, flat)) {
    return std::nullopt;
  }
  const HeightTable heights(stack, ctx);
  model.layer_heights = heights.layers();
  if (!emit_chunks(flat, heights, plan_chunks(flat, clip.area, max_tiles), clip.area, 1, ctx, model.chunks)) {
    return std::nullopt;
  }
  return model;
}

std::optional<MeshModel> rebuild_layout(const MeshModel& previous, const LayerStack& stack, const db::CellDef& cell,
                                        std::span<const db::Rect> dirty, BuildContext& ctx, std::size_t max_tiles) {
  const std::vector<db::Rect> changed = grown_dirty(dirty, previous.region);
  if (changed.empty()) {
    return previous;
  }
  const RebuildPlan plan = affected_chunks(previous, changed);
  FlatLayout flat;
  if (!flatten_snapshot(stack, cell, plan.flatten_areas, ctx, flat)) {
    return std::nullopt;
  }
  std::vector<ChunkCell> cells = plan_new_chunks(flat, previous.region, plan.existing, changed, max_tiles);
  for (std::size_t i = 0; i < previous.chunks.size(); ++i) {
    if (plan.affected[i]) {
      cells.push_back(ChunkCell{previous.chunks[i].area, previous.chunks[i].id});
    }
  }
  MeshModel model;
  model.mode = previous.mode;
  model.region = previous.region;
  model.version = previous.version + 1;
  const HeightTable heights(stack, ctx);
  model.layer_heights = heights.layers();
  std::vector<Chunk> fresh;
  if (!emit_chunks(flat, heights, std::move(cells), previous.region, model.version, ctx, fresh)) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < previous.chunks.size(); ++i) {
    if (!plan.affected[i]) {
      model.chunks.push_back(previous.chunks[i]);
    }
  }
  for (Chunk& c : fresh) {
    if (!chunk_empty(c)) {
      model.chunks.push_back(std::move(c));
    }
  }
  sort_by_id(model.chunks);
  return model;
}

}  // namespace detail

std::optional<MeshModel> build_from_layout(const LayerStack& stack, const db::CellDef& cell, const ClipArea& clip,
                                           BuildContext& ctx) {
  return detail::build_layout(stack, cell, clip, ctx, detail::kMaxChunkTiles);
}

std::optional<MeshModel> build_from_planes(const LayerStack& stack, const mask::LayerPlanes& /*planes*/,
                                           const ClipArea& clip, BuildContext& ctx) {
  MeshModel model = empty_model(stack, clip);
  model.layer_heights = detail::HeightTable(stack, ctx).layers();
  return model;
}

std::optional<MeshModel> rebuild_chunks(const MeshModel& previous, const LayerStack& stack, const db::CellDef& cell,
                                        std::span<const db::Rect> dirty, BuildContext& ctx) {
  return detail::rebuild_layout(previous, stack, cell, dirty, ctx, detail::kMaxChunkTiles);
}

}  // namespace hefesto::view3d
