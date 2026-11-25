// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Yank buffers (extract-hierarchy "Hierarchical label names"). Ported from
// Magic 8.3.694 (fc5daa9e) extract/ExtYank.c and database/DBcellcopy.c
// (DBCellCopyAllPaint); Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include "hefesto/extract/hier/yank.hpp"

#include <iterator>
#include <utility>
#include <vector>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract {

std::unique_ptr<db::CellDef> new_yank_cell(std::string name, std::size_t plane_count) {
  auto cell = std::make_unique<db::CellDef>(std::move(name), plane_count);
  cell->flags().internal = true;
  cell->flags().available = true;
  return cell;
}

void clear_yank_cell(db::CellDef& cell) {
  const std::size_t planes = cell.plane_count();
  cell.set_plane_count(0);
  cell.set_plane_count(planes);
  cell.labels().clear();
}

void copy_tree_paint(const db::CellDef& def, const db::Rect& area, const db::Transform& to_target,
                     db::CellDef& target, const tech::Technology& technology) {
  if (area.x0 > area.x1 || area.y0 > area.y1) {
    return;
  }
  (void)db::copy_paint(def, area, to_target, target, technology, ~tech::TypeMask::of(tech::kSpace));
  for (const db::CellUse* u : hier::uses_in(def, area)) {
    (void)hier::array_elements(*u, area, [&](const hier::Element& e) {
      const db::Rect child = hier::clip(db::apply(db::invert(e.to_parent), area), u->def->bbox());
      copy_tree_paint(*u->def, child, db::compose(to_target, e.to_parent), target, technology);
      return false;
    });
  }
}

void yank_element(const db::CellUse& use, std::int32_t x, std::int32_t y, const db::Rect& area, db::CellDef& target,
                  const tech::Technology& technology, bool prefix) {
  const db::Transform trans = db::element_transform(use, x, y);
  const db::Rect child_area = hier::clip(db::apply(db::invert(trans), area), use.def->bbox());
  copy_tree_paint(*use.def, child_area, trans, target, technology);

  const std::string head = prefix ? db::array_element_name(use, x, y) + "/" : std::string{};
  std::vector<db::Label> found;
  db::search_labels(*use.def, child_area, [&](const db::LabelHit& hit) {
    const db::Label& lab = *hit.label;
    if (lab.type == tech::kSpace || label_role(lab.text) != LabelRole::name) {
      return true;
    }
    db::Label copy = lab;
    const db::Transform to_parent = db::compose(trans, hit.to_root);
    copy.rect = db::apply(to_parent, lab.rect);
    copy.justification = db::transform_position(to_parent, lab.justification);
    copy.text = head + hit.path + lab.text;
    found.push_back(std::move(copy));
    return true;
  });
  // Each label goes to the head of the list (extHierLabelFunc).
  auto& labels = target.labels();
  labels.insert(labels.begin(), std::make_move_iterator(found.rbegin()), std::make_move_iterator(found.rend()));
}

void yank_use(const db::CellUse& use, const db::Rect& area, db::CellDef& target, const tech::Technology& technology,
              bool prefix) {
  (void)hier::array_elements(use, area, [&](const hier::Element& e) {
    yank_element(use, e.x, e.y, area, target, technology, prefix);
    return false;
  });
}

void copy_labels_to_front(const db::CellDef& source, db::CellDef& target) {
  auto& labels = target.labels();
  labels.insert(labels.begin(), source.labels().begin(), source.labels().end());
}

}  // namespace hefesto::extract
