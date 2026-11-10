// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pending-cell list of the continuous checker (drc-engine "Pending
// list and background scheduling"): cells with `checkpaint` tiles, in the
// order they were marked (children before the parents that copy their
// errors up). Not thread-safe: changed by the command thread only (marking,
// commits) while it holds the database lock exclusively; the worker reads
// it under the shared lock.
//
// append() keeps a quirk of DRCCheckThis: the search for the cell stops
// before the last entry, so a cell that is already last is appended again.
// The duplicate is harmless (it is knocked off the list when reached, with
// no check tiles left) but it keeps the order Magic's list has when a
// later edit marks that cell again.
// (Magic ref: drc/DRCcontin.c DRCPendingRoot, DRCCheckThis, DRCContinuous,
// DRCRemovePending)
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2/D7).

#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace hefesto::db {
class CellDef;
}

namespace hefesto::drc {

class PendingList {
 public:
  /// DRCCheckThis's append: adds `cell` at the end unless it is listed
  /// before the last entry (see the file comment); true when added.
  bool append(db::CellDef& cell);
  /// Removes every entry of `cell`; true when it was listed. Must be called
  /// before a listed cell is destroyed (DRCRemovePending).
  bool remove(const db::CellDef& cell);
  /// Knocks the first entry off the list (DRCContinuous).
  void pop_front();
  [[nodiscard]] bool contains(const db::CellDef& cell) const noexcept;
  /// The first pending cell, or nullptr.
  [[nodiscard]] db::CellDef* front() const noexcept;
  [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return cells_.size(); }
  /// The listed cells in order.
  [[nodiscard]] std::span<db::CellDef* const> cells() const noexcept { return cells_; }
  void clear() noexcept { cells_.clear(); }

 private:
  std::vector<db::CellDef*> cells_;
};

}  // namespace hefesto::drc
