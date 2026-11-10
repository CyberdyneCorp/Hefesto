// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::PendingList (drc-engine "Pending list and background scheduling").
// Ported from Magic 8.3.694 drc/DRCcontin.c (DRCCheckThis's append loop,
// DRCContinuous, DRCRemovePending); Magic is (c) the Regents of the
// University of California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/engine/pending.hpp"

#include <algorithm>

namespace hefesto::drc {

bool PendingList::append(db::CellDef& cell) {
  // `while (plast->dpc_next != NULL) { if (plast->dpc_def == celldef) break; ... }`
  // never looks at the last entry.
  if (!cells_.empty() && std::find(cells_.begin(), cells_.end() - 1, &cell) != cells_.end() - 1) {
    return false;
  }
  cells_.push_back(&cell);
  return true;
}

bool PendingList::remove(const db::CellDef& cell) {
  return std::erase(cells_, &cell) > 0;
}

void PendingList::pop_front() {
  if (!cells_.empty()) {
    cells_.erase(cells_.begin());
  }
}

bool PendingList::contains(const db::CellDef& cell) const noexcept {
  return std::find(cells_.begin(), cells_.end(), &cell) != cells_.end();
}

db::CellDef* PendingList::front() const noexcept {
  return cells_.empty() ? nullptr : cells_.front();
}

}  // namespace hefesto::drc
