// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::check_this (drc-engine "Marking changed areas"). Ported from Magic
// 8.3.694 drc/DRCcontin.c (DRCCheckThis) and database/DBcellbox.c
// (DBComputeArrayArea); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/engine/marking.hpp"

#include "hefesto/db/cell.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "internal.hpp"

namespace hefesto::drc {

void check_this(editor::EditorState& editor, db::CellDef& cell, std::optional<db::Rect> area) {
  if (cell.flags().internal) {
    return;
  }
  (void)editor.drc.pending.append(cell);
  if (editor.drc.worker) {
    editor.drc.worker->bump_generation();
  }
  if (!area) {
    return;
  }
  const db::PaintRow row = db::paint_row(*editor.technology, tech::kPlaneDrcCheck, tech::kCheckPaint);
  (void)cell.plane(tech::kPlaneDrcCheck).paint(db::expand(*area, current_halo(editor)), row);
  // cd_parents: every use of the cell, the most recent first; a use with
  // no parent stands for a window.
  const std::vector<db::CellUse*> uses = cell.instances();
  for (const db::CellUse* u : uses) {
    if (u->parent == nullptr) {
      continue;
    }
    db::Rect parent_area = db::apply(u->transform, *area);
    if (u->array.is_array()) {
      const db::Rect last = detail::array_area(*area, *u, u->array.xhi, u->array.yhi);
      parent_area = db::include(parent_area, db::apply(u->transform, last));
    }
    check_this(editor, *u->parent, parent_area);
  }
}

void paint_check_area(const tech::Technology& tech, db::CellDef& cell, const db::Rect& area) {
  (void)cell.plane(tech::kPlaneDrcCheck).paint(area, db::paint_row(tech, tech::kPlaneDrcCheck, tech::kCheckPaint));
}

}  // namespace hefesto::drc
