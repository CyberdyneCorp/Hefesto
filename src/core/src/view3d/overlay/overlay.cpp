// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Overlay datasets and color maps (openspec/changes/m10-3d-view/design.md
// D16, P3). Scaffold stub (M10 task 2.3): every value maps to black.
// OVERLAY replaces it in task 15.1.
//
// Owner: OVERLAY (design D3).

#include "hefesto/view3d/overlay/dataset.hpp"

namespace hefesto::view3d::overlay {

Rgb color_of(const ColorMap& /*map*/, double /*value*/, double /*min*/, double /*max*/) {
  return Rgb{0, 0, 0};
}

}  // namespace hefesto::view3d::overlay
