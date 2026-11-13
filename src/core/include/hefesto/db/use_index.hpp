// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell-use index: Magic's bplane emulation (capability gds-write
// "Structure ordering", Q-MASK-204, Q-MASK-323;
// openspec/changes/m5-mask-gds-cif/design.md D4 "Use order", D7). Each cell
// will own one; it models Magic's bplane state (the unbinned LIFO in-box
// list, the bins with recursive sub-binning and the oversized list, the
// "bins exist" flag, the doubled bounding box on rebuild), so GDS SREF
// order, structure order, CIF symbol numbering and CIF call order follow
// Magic's enumeration order.
//
// Operations (Magic ref: bplane/bpMain.c, bpBins.c, bpEnum.h, bpEnum.c):
// - place / remove: BPAdd / BPDelete (use creation, read, move, delete and
//   the DBReComputeBbox re-placement);
// - search: BPE_OVERLAP. The area is shrunk by 1 on each side and a use
//   matches when its bbox touches the shrunk area. A search whose shrunk
//   area does not surround the index's bounding box is non-surrounding and
//   rebuilds the bins at 10 or more unbinned uses (bpBins.c:651);
// - find_equal: BPE_EQUAL (DBCellFindDup), never rebuilds;
// - enumerate_all: DBCellEnum over the whole plane (the writers and `.mag`
//   write), never rebuilds;
// - order(compat): magic-compat ON the bin walk of the current state; OFF
//   the reverse of insertion order, independent of history.
//
// Rebuilding bins is a logically const mutation that happens during reads
// (the command thread, the DRC worker under the shared database lock), so
// the state is guarded by the index's own mutex and order() returns a
// snapshot.
//
// The M5 scaffold version keeps the unbinned LIFO list only: every order is
// today's db::uses_in_search_order order, and no search rebuilds.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); INDEX fills
// in the implementation in src/core/src/db/use_index*.cpp (design D2).

#pragma once

#include <cstddef>
#include <mutex>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/util/function_ref.hpp"

namespace hefesto::db {

/// Visits one use; returns false to stop the enumeration.
using UseIndexVisitor = util::FunctionRef<bool(CellUse&)>;

/// A snapshot of the index state, for trace tests.
struct UseIndexState {
  bool binned = false;                    // "bins exist"
  std::vector<const CellUse*> unbinned;   // the in-box list, head (newest) first
  Rect bbox;                              // the index's bounding box
};

class UseIndex {
 public:
  UseIndex() = default;
  UseIndex(const UseIndex&) = delete;
  UseIndex& operator=(const UseIndex&) = delete;

  /// BPAdd: puts `use` (with its current bbox) at the head of the index.
  void place(CellUse& use);
  /// BPDelete: takes `use` out of the index; no-op when absent.
  void remove(const CellUse& use);

  /// BPE_OVERLAP search over `area`; may rebuild the bins (see above).
  /// Returns false when the visitor stopped early.
  bool search(const Rect& area, UseIndexVisitor visit) const;
  /// BPE_EQUAL: the uses whose bbox equals `rect`; never rebuilds.
  bool find_equal(const Rect& rect, UseIndexVisitor visit) const;
  /// Every use in enumeration order; never rebuilds.
  bool enumerate_all(UseIndexVisitor visit) const;

  /// The enumeration order under `compat` (a snapshot).
  [[nodiscard]] std::vector<CellUse*> order(const compat::CompatSettings& compat) const;

  /// The number of uses in the index.
  [[nodiscard]] std::size_t size() const;

  /// The current state, for trace tests.
  [[nodiscard]] UseIndexState state_for_test() const;

 private:
  mutable std::mutex mutex_;
  std::vector<CellUse*> inbox_;  // insertion order; the head is the back
};

/// The enumeration order of `def`'s uses under `compat` (the INDEX
/// replacement of db::uses_in_search_order).
[[nodiscard]] std::vector<CellUse*> uses_in_search_order(const CellDef& def, const compat::CompatSettings& compat);

/// The uses of `def` found by a BPE_OVERLAP search over `area`, in search
/// order. Returns false when the visitor stopped early.
bool uses_overlapping(const CellDef& def, const Rect& area, UseIndexVisitor visit);

}  // namespace hefesto::db
