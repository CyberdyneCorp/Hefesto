// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// View state and camera (openspec/changes/m10-3d-view/design.md D7).
// Scaffold stubs (M10 task 2.3): default_view answers ViewState{}, the
// operations leave the view unchanged, matrix is the identity and every
// preset is top-down. CAMERA replaces them in section 4.
//
// Owner: CAMERA (design D3).

#include "hefesto/view3d/camera/view_state.hpp"

namespace hefesto::view3d {

ViewState default_view(const db::Rect& /*bbox*/, Mode /*mode*/, float /*f*/) {
  return ViewState{};
}

void rescale(ViewState& /*view*/, float /*f*/) {}

Matrix4 matrix(const ViewState& /*view*/, int /*width*/, int /*height*/, const ProjectionParams& /*projection*/) {
  return Matrix4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
}

void apply(ViewState& /*view*/, const Gesture& /*gesture*/) {}

Angles preset(Preset /*which*/) {
  return Angles{};
}

}  // namespace hefesto::view3d
