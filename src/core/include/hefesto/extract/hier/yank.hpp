// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Yanking a subtree into a scratch cell (capability extract-hierarchy
// "Hierarchical label names"): the paint of every level under an area and
// the node-name labels with their instance path. The scratch cells are
// plain db::CellDefs owned by the extraction context, never registered in a
// cell table.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtYank.c (extHierYankFunc,
// extHierLabelFunc, extHierCopyLabels, extHierFreeLabels) and
// database/DBcellcopy.c (DBCellCopyAllPaint); Magic is Copyright (C) 1985,
// 1990 Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::extract {

/// A new empty scratch cell with `plane_count` planes.
[[nodiscard]] std::unique_ptr<db::CellDef> new_yank_cell(std::string name, std::size_t plane_count);

/// DBCellClearDef on a scratch cell: no paint, no labels.
void clear_yank_cell(db::CellDef& cell);

/// DBCellCopyAllPaint: the paint of `def` and every level below it within
/// `area` (def coordinates), transformed by `to_target`, painted into
/// `target`.
void copy_tree_paint(const db::CellDef& def, const db::Rect& area, const db::Transform& to_target,
                     db::CellDef& target, const tech::Technology& technology);

/// extHierYankFunc for element (x, y) of `use`: the paint under `area`
/// (parent coordinates; clipped to the child's bounding box) and the
/// node-name labels of every level, each put at the head of `target`'s
/// label list with its path (`<element>/` first when `prefix`).
void yank_element(const db::CellUse& use, std::int32_t x, std::int32_t y, const db::Rect& area, db::CellDef& target,
                  const tech::Technology& technology, bool prefix);

/// DBArraySr(use, area, extHierYankFunc): every element of `use` that may
/// overlap `area`.
void yank_use(const db::CellUse& use, const db::Rect& area, db::CellDef& target, const tech::Technology& technology,
              bool prefix);

/// extHierCopyLabels: copies of `source`'s labels, in order, put before
/// `target`'s.
void copy_labels_to_front(const db::CellDef& source, db::CellDef& target);

}  // namespace hefesto::extract
