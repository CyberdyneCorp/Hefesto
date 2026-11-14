// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Merged polygon output of one mask plane (capability gds-write "Merged
// polygon output", `gds merge yes`; openspec/changes/m5-mask-gds-cif/design.md
// D12, Q-MASK-217 replicate). A port of Magic 8.3.694 calmaMergePaintFunc
// (calma/CalmaWrite.c:2394-2700) driven by the plane's area enumeration
// (DBSrPaintArea over CIFSolidBits), and of calmaProcessBoundary's and
// calmaWritePaintFunc's element bytes.
//
// For each solid tile in enumeration order that is not yet processed, the
// connected tiles are collected depth first (a LIFO stack, neighbours pushed
// in the counterclockwise side order top, left, bottom, right); each tile's
// corner ring is cut into segments against its neighbours and spliced into
// the component's bounds (merge_rings.hpp). A tile with no internal edge is
// written at once as a plain rectangle or triangle (clipped to `clip`, as
// calmaWritePaintFunc does); the bounds of a component are written after its
// degenerate and colinear passes, newest first, unclipped. The marks
// (pending, processed) persist for the whole plane, as Magic's tile client
// fields do.
//
// The result is pure: the plane is only read, and the polygons are in mask
// units without the closing point. The writers multiply by the paint scale
// (calmaPaintScale) and repeat the first point (write_boundary).
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/mask/plane/plane.hpp"

namespace hefesto::gds {

/// The merged polygons of `plane` in output order (`gds merge yes`). `clip`
/// is calmaOutFunc's cliprect: nullopt for the whole plane (TiPlaneRect),
/// else the enumeration area and the clip of isolated tiles.
[[nodiscard]] std::vector<mask::Polygon> merge_plane_polygons(const mask::MaskPlane& plane,
                                                              const std::optional<db::Rect>& clip = std::nullopt);

/// An XY record of `coords` (x, y pairs), each checked to fit an I4 (a
/// value outside fails the write with GDS-WRITE-201, Q-MASK-222).
[[nodiscard]] diag::Status write_xy(RecordWriter& out, std::span<const std::int64_t> coords);

/// One BOUNDARY element: BOUNDARY, LAYER, DATATYPE (each an I2 of the low
/// 16 bits, as calmaOutI2), XY with every point times `scale` and then the
/// first point again, ENDEL. A polygon without points writes nothing.
[[nodiscard]] diag::Status write_boundary(RecordWriter& out, std::int32_t layer, std::int32_t datatype,
                                          std::span<const db::Point> polygon, std::int64_t scale);

/// The merged boundary elements of one mask layer: merge_plane_polygons
/// written with write_boundary on `layer`/`datatype` at `paint_scale`.
[[nodiscard]] diag::Status emit_merged_boundaries(RecordWriter& out, const mask::MaskPlane& plane, std::int32_t layer,
                                                  std::int32_t datatype, std::int64_t paint_scale,
                                                  const std::optional<db::Rect>& clip = std::nullopt);

}  // namespace hefesto::gds
