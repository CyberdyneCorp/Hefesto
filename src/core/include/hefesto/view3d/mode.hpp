// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The two drawing modes of a 3D window (view-3d "Opening the 3D window",
// "Rendered geometry"; openspec/changes/m10-3d-view/design.md D1, D4):
// layout-layer mode draws tile types with the extract-style heights,
// mask-layer mode draws the current output style's mask layers with their
// `render` heights (Magic's W3DclientRec.cif). The mode is kept per window
// (Q-MISC-76, fix).
//
// Owner: SCAFFOLD (shared by stack, mesh, camera and client).

#pragma once

#include <cstdint>

namespace hefesto::view3d {

enum class Mode : std::uint8_t { layout_layer, mask_layer };

/// The index of a drawable layer in a LayerStack (the 16-bit layer index
/// of a mesh instance, design D6).
using LayerId = std::uint16_t;

}  // namespace hefesto::view3d
