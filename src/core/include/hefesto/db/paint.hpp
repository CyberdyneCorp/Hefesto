// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Painting, erasing and copying paint in a cell (capability
// layout-paint-semantics): applies the technology's result tables on every
// affected plane, re-merges split tiles, resolves contact images and marks
// the cell modified. Label adjustment ("Label adjustment after paint
// changes", db/labels.hpp) and bounding boxes (db/hierarchy.hpp) are the
// caller's next steps, as in Magic's commands.
// (Magic ref: database/DBpaint2.c, database/DBcellcopy.c)
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <optional>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db {

struct PaintOptions {
  /// Types the user has made inactive ("Active (locked) layers"); painting
  /// or erasing them is skipped and contacts with an inactive residue are
  /// painted/erased through their active residues only.
  tech::TypeMask locked;
  bool magic_compat = true;
};

/// One side of a split rectangle (a triangle) to paint instead of the
/// whole rectangle.
struct TriangleSpec {
  Diagonal diagonal = Diagonal::ne_sw;
  bool right_side = false;
};

/// The paint / erase result row of `type` on `plane` (R[plane][type][*]).
[[nodiscard]] PaintRow paint_row(const tech::Technology& tech, tech::PlaneId plane, tech::TileType type);
[[nodiscard]] PaintRow erase_row(const tech::Technology& tech, tech::PlaneId plane, tech::TileType type);

/// DBPaint ("Paint operation"): paints `type` (or one triangle of `area`)
/// on every affected plane, merges split tiles and resolves contact images.
/// Marks the cell modified even when nothing changes.
[[nodiscard]] diag::Status paint_type(CellDef& def, const tech::Technology& tech, const Rect& area,
                                      tech::TileType type, std::optional<TriangleSpec> triangle = std::nullopt);

/// DBErase ("Paint operation", erase): erases `type`; erasing space applies
/// the space paint table (a no-op with default tables, Q-MODEL-29).
[[nodiscard]] diag::Status erase_type(CellDef& def, const tech::Technology& tech, const Rect& area,
                                      tech::TileType type, std::optional<TriangleSpec> triangle = std::nullopt);

/// DBPaintValid for the mask {type}: the user-level paint ("Paint
/// operation", "Active (locked) layers"); a stacked type paints its residues.
[[nodiscard]] diag::Status paint(CellDef& def, const tech::Technology& tech, const Rect& area, tech::TileType type,
                                 const PaintOptions& options = {});

/// DBEraseValid: erases every active type of `types` (contacts with an
/// inactive residue through their active residues). Stacked types in
/// `types` are ignored (Q-MODEL-22) unless magic_compat is off, which
/// decomposes them; space in `types` is ignored (Q-MODEL-29) unless
/// magic_compat is off, which erases every type.
[[nodiscard]] diag::Status erase(CellDef& def, const tech::Technology& tech, const Rect& area,
                                 const tech::TypeMask& types, const PaintOptions& options = {});

/// Paints a triangle (`splitpaint`) through the valid entry point:
/// "Non-Manhattan painting".
[[nodiscard]] diag::Status paint_triangle(CellDef& def, const tech::Technology& tech, const Rect& area,
                                          Diagonal diagonal, bool right_side, tech::TileType type,
                                          const PaintOptions& options = {});

/// "Copying paint between cells" (DBCellCopyPaint for one level): paint of
/// `source` within `area` (source coordinates), transformed by `transform`,
/// painted plane by plane into `target` without image resolution. Split
/// tiles that cross the boundary of `area` are skipped (see the MODEL
/// conformance notes); whole-cell copies never clip.
[[nodiscard]] diag::Status copy_paint(const CellDef& source, const Rect& area, const Transform& transform,
                                      CellDef& target, const tech::Technology& tech,
                                      const tech::TypeMask& types = tech::TypeMask::all());

}  // namespace hefesto::db
