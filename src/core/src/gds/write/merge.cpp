// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Merged polygon output (gds-write "Merged polygon output", Q-MASK-217; see
// hefesto/gds/write/polygons.hpp): the structure body's entry point to the
// calmaMergePaintFunc port of merge_plane.hpp. calmaOutFunc always passes
// TiPlaneRect as the clip rectangle on this path, so no clip is given.
// Owner: GDSW.

#include "hefesto/gds/write/merge_plane.hpp"
#include "hefesto/gds/write/polygons.hpp"
#include "hefesto/gds/write/write.hpp"

namespace hefesto::gds {

std::vector<mask::Polygon> merge_polygons(const mask::MaskPlane& plane) {
  return merge_plane_polygons(plane);
}

diag::Status emit_merged_polygons(const mask::MaskPlane& plane, const mask::GdsLayer& layer, ElementWriter& out) {
  return emit_merged_boundaries(out.records(), plane, layer.layer, layer.datatype, out.scale().paint, std::nullopt,
                                out.cell().name());
}

}  // namespace hefesto::gds
