// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D -> 2D link (pick.hpp; openspec/changes/m10-3d-view/design.md D10,
// tasks.md 9.3). The selection root becomes the rendered cell and the
// selection is replaced, as a new `select area` or cell click does
// (SelectClear first):
// - a tile of the rendered cell: its type's paint under the source tile,
//   as `select area <type>` with the box on that tile would;
// - a tile under a subcell: the rendered cell's child use on the target's
//   path, as Magic selects unexpanded material (SelectCell).
// The change is one undo event and redraws the highlights
// (select::record_change).
//
// Owner: INSPECT (design D3).

#include <string>
#include <utility>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/select/selection.hpp"
#include "hefesto/view3d/pick/pick.hpp"

namespace hefesto::view3d {

namespace {

bool is_loaded(const editor::EditorState& editor, const db::CellDef* cell) {
  return cell != nullptr && editor.cells.find(cell->name()) == cell;
}

/// The child use of `parent` named by one path element: its identifier, or
/// an element "id[x,y]" of an array.
db::CellUse* child_use(db::CellDef& parent, const std::string& element) {
  for (const auto& use : parent.uses()) {
    const std::string& id = use->id;
    if (element == id || (element.size() > id.size() && element.compare(0, id.size(), id) == 0 && element[id.size()] == '[')) {
      return use.get();
    }
  }
  return nullptr;
}

void select_paint(select::Selection& sel, const tech::Technology& tech, const PickTarget& target) {
  const tech::TypeMask types = tech::TypeMask::of(*target.type);
  (void)db::copy_paint(*target.cell, target.material, db::kIdentity, *sel.def(), tech, types);
  sel.provenance().push_back(
      select::Provenance{select::Provenance::Kind::paint, target.cell, std::string{}, target.material});
  sel.area_types = types;  // SelectArea remembers the requested types
}

void select_use(select::Selection& sel, const PickTarget& target, const db::CellUse& use) {
  db::CellUse* added = sel.add_use(*use.def, use.id, use.transform, use.array, use.locked);
  if (added != nullptr) {
    sel.provenance().push_back(select::Provenance{select::Provenance::Kind::use, target.cell, std::string{}, use.bbox});
  }
  sel.cycle.last_selected_use = added;
}

}  // namespace

std::optional<SelectionLink> link_to_2d(editor::EditorState& editor, const PickTarget& target) {
  if (!target.type.has_value() || target.material.is_null() || !is_loaded(editor, target.cell)) {
    return std::nullopt;
  }
  const db::CellUse* use = target.use_path.empty() ? nullptr : child_use(*target.cell, target.use_path.front());
  if (!target.use_path.empty() && use == nullptr) {
    return std::nullopt;  // the hierarchy changed since the pick
  }
  const tech::Technology& tech = *editor.technology;
  select::Selection& sel = editor.selection;
  select::Snapshot before = sel.snapshot();
  sel.set_root(target.cell, tech.plane_count());
  sel.clear();
  SelectionLink link{target.cell, target.material, use != nullptr};
  if (use == nullptr) {
    select_paint(sel, tech, target);
  } else {
    select_use(sel, target, *use);
    link.area = use->bbox;
  }
  db::recompute_bbox(*sel.def());
  select::record_change(editor, std::move(before));
  return link;
}

diag::Status to_selection(editor::EditorState& editor, const PickTarget& target) {
  (void)link_to_2d(editor, target);
  return diag::Done{};
}

}  // namespace hefesto::view3d
