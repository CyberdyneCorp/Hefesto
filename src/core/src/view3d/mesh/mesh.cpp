// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mesh builders (openspec/changes/m10-3d-view/design.md D6). Scaffold stubs
// (M10 task 2.3): every build returns an empty model of the stack's mode
// over the clip area; rebuild_chunks returns the previous model unchanged.
// MESH replaces them in section 6 (build_from_planes: MASKSRC, P2).
//
// Owner: MESH (design D3).

#include "hefesto/view3d/mesh/build.hpp"

namespace hefesto::view3d {

namespace {

MeshModel empty_model(const LayerStack& stack, const ClipArea& clip) {
  MeshModel model;
  model.mode = stack.mode();
  model.region = clip.area;
  return model;
}

}  // namespace

std::optional<MeshModel> build_from_layout(const LayerStack& stack, const db::CellDef& /*cell*/,
                                           const ClipArea& clip, BuildContext& /*ctx*/) {
  return empty_model(stack, clip);
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
