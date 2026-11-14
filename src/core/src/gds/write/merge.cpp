// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Merged polygon output (gds-write "Merged polygon output", Q-MASK-217; see
// hefesto/gds/write/polygons.hpp). Placeholder until task 10.4 ports
// calmaMergePaintFunc: `gds merge yes` writes the unmerged boundaries.
// Owner: GDSW (task 10.4 replaces this file).

#include "hefesto/gds/write/polygons.hpp"

namespace hefesto::gds {

diag::Status emit_merged_polygons(const mask::MaskPlane& plane, const mask::GdsLayer& layer, ElementWriter& out) {
  return emit_tile_boundaries(plane, layer, out);
}

}  // namespace hefesto::gds
