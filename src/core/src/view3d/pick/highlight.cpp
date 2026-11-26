// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Highlight sets (pick.hpp; openspec/changes/m10-3d-view/design.md D10,
// tasks.md 9.4, 9.5): the instances of a mesh lying over some paint held in
// root coordinates, per layer. The paint is the selection's cell (2D -> 3D
// link) or a scratch cell the net's tiles are painted into (net
// highlighting through connect::search_flat or search_hierarchical,
// detail::search_net in net.cpp). An
// instance is highlighted when paint of its layer's tile type overlaps its
// probe area (detail::probe_rect): its rectangle, a triangle's bounding
// box, or the unit strip inside a side face.
//
// Owner: INSPECT (design D3).

#include <algorithm>
#include <functional>
#include <limits>

#include "hefesto/connect/search.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/select/selection.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "view3d/pick/internal.hpp"

namespace hefesto::view3d {

namespace {

/// The types whose paint highlights an instance of a layer (empty: none).
using LayerTypes = std::function<tech::TypeMask(LayerId)>;

LayerTypes types_from_stack(const LayerStack& stack) {
  return [&stack](LayerId layer) {
    if (layer >= stack.size() || !stack.layers()[layer].type.has_value()) {
      return tech::TypeMask{};
    }
    return tech::TypeMask::of(*stack.layers()[layer].type);
  };
}

LayerTypes any_user_type() {
  tech::TypeMask mask = tech::TypeMask::all();
  for (tech::TileType t = 0; t < tech::kFirstUserType; ++t) {
    mask.clear_type(t);
  }
  return [mask](LayerId) { return mask; };
}

/// Whether paint of `types` in `paint` overlaps `probe` with positive area.
bool covered(const db::CellDef& paint, const db::Rect& probe, const tech::TypeMask& types) {
  for (std::size_t p = tech::kFirstUserPlane; p < paint.plane_count(); ++p) {
    const bool stopped = !paint.plane(static_cast<tech::PlaneId>(p)).enumerate(
        probe, types, [&](const db::TileView& tile) { return !db::overlap(tile.area, probe); });
    if (stopped) {
      return true;
    }
  }
  return false;
}

/// The instance of a chunk at chunk-local index `local` (rects, sides, triangles).
PickRef ref_at(const Chunk& chunk, std::size_t chunk_index, std::size_t local) {
  if (local < chunk.rects.size()) {
    return PickRef{chunk_index, PickKind::rect, local};
  }
  local -= chunk.rects.size();
  if (local < chunk.sides.size()) {
    return PickRef{chunk_index, PickKind::side, local};
  }
  return PickRef{chunk_index, PickKind::triangle, local - chunk.sides.size()};
}

HighlightSet overlapping(const MeshModel& mesh, const db::CellDef& paint, const LayerTypes& types_of) {
  HighlightSet out;
  std::uint64_t base = 1;  // the id of the chunk's first instance
  for (std::size_t c = 0; c < mesh.chunks.size(); ++c) {
    const Chunk& chunk = mesh.chunks[c];
    const std::uint64_t count = pick_instance_count(chunk);
    for (std::size_t i = 0; i < count && base + i <= std::numeric_limits<PickId>::max(); ++i) {
      const PickRef ref = ref_at(chunk, c, i);
      const tech::TypeMask types = types_of(detail::layer_of(chunk, ref));
      if (!types.empty() && covered(paint, detail::probe_rect(chunk, ref), types)) {
        out.ids.push_back(static_cast<PickId>(base + i));
      }
    }
    base += count;
  }
  return out;
}

}  // namespace

HighlightSet from_selection(const MeshModel& mesh, const select::Selection& selection) {
  if (selection.def() == nullptr) {
    return HighlightSet{};
  }
  return overlapping(mesh, *selection.def(), any_user_type());
}

HighlightSet from_selection(const MeshModel& mesh, const LayerStack& stack, const select::Selection& selection) {
  if (selection.def() == nullptr) {
    return HighlightSet{};
  }
  return overlapping(mesh, *selection.def(), types_from_stack(stack));
}

HighlightSet from_selection(const MeshModel& mesh, const PickContext& context, const select::Selection& selection) {
  if (context.cell != nullptr && selection.root() != context.cell) {
    return HighlightSet{};
  }
  return context.stack != nullptr ? from_selection(mesh, *context.stack, selection) : from_selection(mesh, selection);
}

HighlightSet net_highlight(const MeshModel& mesh, const editor::EditorState& editor, const PickTarget& seed) {
  return net_highlight(mesh, editor, seed, NetOptions{});
}

HighlightSet net_highlight(const MeshModel& mesh, const editor::EditorState& editor, const PickTarget& seed,
                           const NetOptions& options) {
  if (!detail::usable_seed(seed, options)) {
    return HighlightSet{};
  }
  detail::NetPaint net(*editor.technology);
  if (!detail::search_net(*seed.cell, seed.material, *seed.type, options, net)) {
    return HighlightSet{};
  }
  if (options.stack != nullptr) {
    return overlapping(mesh, net.cell(), types_from_stack(*options.stack));
  }
  const LayerStack stack = layout_stack(editor, options.magic_compat);
  return overlapping(mesh, net.cell(), types_from_stack(stack));
}

HighlightSet isolate_others(const MeshModel& mesh, const HighlightSet& keep) {
  HighlightSet out;
  std::uint64_t total = 0;
  for (const Chunk& chunk : mesh.chunks) {
    total += pick_instance_count(chunk);
  }
  total = std::min<std::uint64_t>(total, std::numeric_limits<PickId>::max());
  for (std::uint64_t id = 1; id <= total; ++id) {
    if (!std::binary_search(keep.ids.begin(), keep.ids.end(), static_cast<PickId>(id))) {
      out.ids.push_back(static_cast<PickId>(id));
    }
  }
  return out;
}

}  // namespace hefesto::view3d
