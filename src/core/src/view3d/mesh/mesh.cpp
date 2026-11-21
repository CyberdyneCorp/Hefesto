// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mesh builders (openspec/changes/m10-3d-view/design.md D6).
// build_from_layout: flatten and clip (flatten.cpp), plan the chunks
// (chunks.cpp), then emit each chunk in Morton order, and in each chunk the
// layers in stack order, each layer's tiles in its plane's enumeration
// order (faces.cpp), so two builds of the same cell are equal. Every chunk
// and the model get version 1; a cancelled build returns nullopt.
// build_from_planes (MASKSRC, P2) and rebuild_chunks (task 6.4) are still
// the scaffold stubs: an empty model over the clip area, and the previous
// model unchanged.
//
// Owner: MESH (design D3).

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

Chunk build_chunk(const detail::FlatLayout& flat, const LayerStack& stack, const detail::ChunkCell& cell,
                  const db::Rect& clip) {
  Chunk chunk;
  chunk.id = cell.id;
  chunk.version = 1;
  chunk.area = cell.area;
  chunk.origin_x = cell.area.x0;
  chunk.origin_y = cell.area.y0;
  const auto layers = stack.layers();
  for (std::size_t i = 0; i < layers.size() && i < flat.of_layer.size(); ++i) {
    const std::ptrdiff_t index = flat.of_layer[i];
    if (index < 0) {
      continue;
    }
    // Zero thickness: top faces only (W3Dmain.c:287).
    const bool sides = stack.draw_height(static_cast<LayerId>(i)).thickness != 0.0F;
    detail::emit_layer(flat.types[static_cast<std::size_t>(index)], static_cast<LayerId>(i), sides, clip, chunk);
  }
  return chunk;
}

}  // namespace

namespace detail {

std::optional<MeshModel> build_layout(const LayerStack& stack, const db::CellDef& cell, const ClipArea& clip,
                                      BuildContext& ctx, std::size_t max_tiles) {
  MeshModel model = empty_model(stack, clip);
  model.version = 1;
  detail::FlatLayout flat;
  if (!detail::flatten_layout(stack, cell, clip.area, ctx.progress, flat)) {
    return std::nullopt;
  }
  const std::vector<detail::ChunkCell> plan = detail::plan_chunks(flat, clip.area, max_tiles);
  for (const detail::ChunkCell& cellplan : plan) {
    if (ctx.progress.cancelled()) {
      return std::nullopt;
    }
    model.chunks.push_back(build_chunk(flat, stack, cellplan, clip.area));
    if (ctx.on_chunk) {
      ctx.on_chunk(model.chunks.back());
    }
    ctx.progress.update(static_cast<double>(model.chunks.size()) / static_cast<double>(plan.size()));
  }
  return model;
}

}  // namespace detail

std::optional<MeshModel> build_from_layout(const LayerStack& stack, const db::CellDef& cell, const ClipArea& clip,
                                           BuildContext& ctx) {
  return detail::build_layout(stack, cell, clip, ctx, detail::kMaxChunkTiles);
}

std::optional<MeshModel> build_from_planes(const LayerStack& stack, const mask::LayerPlanes& /*planes*/,
                                           const ClipArea& clip, BuildContext& /*ctx*/) {
  return empty_model(stack, clip);
}

std::optional<MeshModel> rebuild_chunks(const MeshModel& previous, const LayerStack& /*stack*/,
                                        const db::CellDef& /*cell*/, std::span<const db::Rect> /*dirty*/,
                                        BuildContext& /*ctx*/) {
  return previous;
}

}  // namespace hefesto::view3d
