// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Boundary output of one mask layer of a GDS structure (capability
// gds-write "Unmerged boundary output", "Merged polygon output";
// openspec/changes/m5-mask-gds-cif/design.md D12). The structure body
// writer calls, for each written non-label mask layer in style order:
// - emit_tile_boundaries with `gds merge no`: one BOUNDARY per solid tile
//   (each solid side of a split tile) in the plane's area enumeration order
//   ("Tile decomposition and enumeration order"), coordinates times
//   OutputScale::paint (calmaWritePaintFunc);
// - emit_merged_polygons with `gds merge yes`: the merge and collinear
//   passes of calmaMergePaintFunc, slits included (Q-MASK-217), up to 200
//   points per BOUNDARY.
// Both write through the structure's ElementWriter and return its first
// error (an I4 overflow, Q-MASK-222, or a stream error).
//
// Owner: GDSW (design D2): emit_tile_boundaries is in
// src/core/src/gds/write/tiles.cpp (task 10.3); emit_merged_polygons is in
// src/core/src/gds/write/merge.cpp (task 10.4), which until then writes
// unmerged boundaries.

#pragma once

#include "hefesto/diag/outcome.hpp"
#include "hefesto/gds/write/emit.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::gds {

/// "Unmerged boundary output" of `plane` on `layer`.
[[nodiscard]] diag::Status emit_tile_boundaries(const mask::MaskPlane& plane, const mask::GdsLayer& layer,
                                                ElementWriter& out);

/// "Merged polygon output" of `plane` on `layer`.
[[nodiscard]] diag::Status emit_merged_polygons(const mask::MaskPlane& plane, const mask::GdsLayer& layer,
                                                ElementWriter& out);

}  // namespace hefesto::gds
