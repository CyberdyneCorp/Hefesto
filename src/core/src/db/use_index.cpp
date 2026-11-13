// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell-use index, scaffold version (openspec/changes/m5-mask-gds-cif
// design.md D3/D4): the unbinned LIFO in-box list only, so every order is
// today's db::uses_in_search_order order and no search rebuilds. INDEX
// replaces it with the bplane port (tasks.md section 5). Owner: INDEX.

#include "hefesto/db/use_index.hpp"

#include <algorithm>

#include "hefesto/db/hierarchy.hpp"

namespace hefesto::db {

namespace {

// bpEnum.c:104-106: BPE_OVERLAP shrinks the search area by 1 on each side
// and bpEnumMatchQ accepts a bbox that touches the shrunk area.
bool matches_overlap(const Rect& bbox, const Rect& area) {
  const Rect shrunk{area.x0 + 1, area.y0 + 1, area.x1 - 1, area.y1 - 1};
  return touch(bbox, shrunk);
}

Rect bounding_box(const std::vector<CellUse*>& uses) {
  if (uses.empty()) {
    return Rect{};
  }
  Rect box = uses.front()->bbox;
  for (const CellUse* use : uses) {
    box = Rect{std::min(box.x0, use->bbox.x0), std::min(box.y0, use->bbox.y0), std::max(box.x1, use->bbox.x1),
               std::max(box.y1, use->bbox.y1)};
  }
  return box;
}

}  // namespace

void UseIndex::place(CellUse& use) {
  const std::lock_guard lock(mutex_);
  inbox_.erase(std::remove(inbox_.begin(), inbox_.end(), &use), inbox_.end());
  inbox_.push_back(&use);
}

void UseIndex::remove(const CellUse& use) {
  const std::lock_guard lock(mutex_);
  inbox_.erase(std::remove(inbox_.begin(), inbox_.end(), &use), inbox_.end());
}

bool UseIndex::search(const Rect& area, UseIndexVisitor visit) const {
  for (CellUse* use : order(compat::CompatSettings{})) {
    if (matches_overlap(use->bbox, area) && !visit(*use)) {
      return false;
    }
  }
  return true;
}

bool UseIndex::find_equal(const Rect& rect, UseIndexVisitor visit) const {
  for (CellUse* use : order(compat::CompatSettings{})) {
    if (use->bbox == rect && !visit(*use)) {
      return false;
    }
  }
  return true;
}

bool UseIndex::enumerate_all(UseIndexVisitor visit) const {
  for (CellUse* use : order(compat::CompatSettings{})) {
    if (!visit(*use)) {
      return false;
    }
  }
  return true;
}

std::vector<CellUse*> UseIndex::order(const compat::CompatSettings& /*compat*/) const {
  const std::lock_guard lock(mutex_);
  return {inbox_.rbegin(), inbox_.rend()};
}

std::size_t UseIndex::size() const {
  const std::lock_guard lock(mutex_);
  return inbox_.size();
}

UseIndexState UseIndex::state_for_test() const {
  const std::lock_guard lock(mutex_);
  return UseIndexState{false, {inbox_.rbegin(), inbox_.rend()}, bounding_box(inbox_)};
}

std::vector<CellUse*> uses_in_search_order(const CellDef& def, const compat::CompatSettings& /*compat*/) {
  return uses_in_search_order(def);
}

bool uses_overlapping(const CellDef& def, const Rect& area, UseIndexVisitor visit) {
  for (CellUse* use : uses_in_search_order(def)) {
    if (matches_overlap(use->bbox, area) && !visit(*use)) {
      return false;
    }
  }
  return true;
}

}  // namespace hefesto::db
