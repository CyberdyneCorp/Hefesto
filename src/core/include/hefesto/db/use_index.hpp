// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell-use index: Magic's bplane emulation (capability gds-write
// "Structure ordering", Q-MASK-204, Q-MASK-323;
// openspec/changes/m5-mask-gds-cif/design.md D4 "Use order", D7). Each
// CellDef owns one (CellDef::use_index(), Magic's cd_cellPlane). It models
// Magic's bplane state: the unbinned LIFO in-box list, the bins with
// recursive sub-binning and the oversized lists, the "bins exist" flag and
// the doubled bounding box on rebuild. GDS SREF order, structure order, CIF
// symbol numbering and CIF call order follow its enumeration order.
//
// Operations (Magic ref: bplane/bpMain.c, bpBins.c, bpEnum.h, bpEnum.c):
// - place / remove: BPAdd / BPDelete (use creation, read, move, delete and
//   the DBReComputeBbox re-placement);
// - search: BPE_OVERLAP. The area is shrunk by 1 on each side and a use
//   matches when its bbox touches the shrunk area. A search whose shrunk
//   area does not surround the index's bounding box is non-surrounding and
//   rebuilds the bins at 10 or more unbinned uses (bpBins.c:651); leaving a
//   bin that rejected 10 or more uses sub-bins it (bpEnum.h:400-420);
// - find_equal: BPE_EQUAL (DBCellFindDup), never rebuilds;
// - enumerate_all: DBCellEnum over the whole plane (the writers and `.mag`
//   write), never rebuilds;
// - order(compat): magic-compat ON the bin walk of the current state; OFF
//   the reverse of insertion (creation) order, independent of history;
// - search(area, compat, visitor): the same search, which bins the index
//   in both modes; magic-compat ON hands the uses over in walk order, OFF
//   in the order of order(compat).
//
// The state is kept in both modes, so toggling magic-compat never rebuilds.
// Rebuilding bins is a logically const mutation that happens during reads
// (the command thread, the DRC worker under the shared database lock), so
// the state is guarded by the index's own mutex, and every enumeration
// hands its visitor a snapshot: a visitor may search the same index again
// and never sees a concurrent rebuild. A visitor that stops early leaves
// the index as Magic's lazy walk would (the sub-binnings Magic would not
// have reached are undone).
//
// An index owned by a CellDef also follows the def's use list: db::place,
// db::remove_use and db::recompute_bbox call place/remove explicitly in
// Magic's order, and a use added to, removed from or moved in uses() by
// other code is placed, removed or re-placed (in uses() order) the next
// time the index is read. Code that moves every use of a cell at once
// (DBMoveCell, DBScaleEverything) calls db::rebuild_use_index afterwards.
//
// Owner: INDEX (design D2): this header and src/core/src/db/use_index*.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
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
  Rect bbox;                              // the index's bounding box (bp_bbox)
  Rect bin_area;                          // the area the bins cover (bp_binArea)
  std::size_t count = 0;                  // bp_count
  std::size_t max_bin_population = 0;     // the longest list of the bins (in-box excluded)
};

class UseIndex {
 public:
  /// A standalone index (tests), or the index of `owner`, which follows
  /// owner->uses() (see above).
  explicit UseIndex(const CellDef* owner = nullptr);
  ~UseIndex();
  UseIndex(const UseIndex&) = delete;
  UseIndex& operator=(const UseIndex&) = delete;

  /// BPAdd: puts `use` (with its current bbox) at the head of the index. A
  /// use already in the index is re-placed (BPDelete, then BPAdd).
  void place(CellUse& use);
  /// BPDelete: takes `use` out of the index; no-op when absent.
  void remove(const CellUse& use);
  /// Records a changed bbox of `use` without re-placing it: an in-box use
  /// keeps its place in the list, and a binned use whose new bbox leaves
  /// its bin moves to the bin (or the in-box list) that holds it. For the
  /// box updates Magic makes no index change for (DBReComputeBbox when the
  /// new box equals the old extended one, database/DBcellbox.c:709-713).
  /// No-op when absent or unchanged.
  void refresh(const CellUse& use);
  /// DBMoveCell / DBScaleEverything: a fresh index holding the same uses
  /// with their current bboxes, added in reverse enumeration order.
  void rebuild();

  /// BPE_OVERLAP search over `area`; may rebuild the bins (see above).
  /// Returns false when the visitor stopped early.
  bool search(const Rect& area, UseIndexVisitor visit) const;
  /// The same search with the session's compat: magic-compat ON exactly
  /// search(area, visit); OFF the same matches (and the same index state
  /// as a full walk), visited in the order of order(compat), so the result
  /// never depends on history.
  bool search(const Rect& area, const compat::CompatSettings& compat, UseIndexVisitor visit) const;
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
  struct State;

  void sync_locked() const;
  void add_locked(CellUse& use) const;
  void delete_locked(const CellUse& use) const;
  void sort_canonical_locked(std::vector<CellUse*>& uses) const;

  const CellDef* owner_ = nullptr;
  mutable std::mutex mutex_;
  std::unique_ptr<State> state_;
};

/// The enumeration order of `def`'s uses under `compat` (the INDEX
/// replacement of db::uses_in_search_order).
[[nodiscard]] std::vector<CellUse*> uses_in_search_order(const CellDef& def, const compat::CompatSettings& compat);

/// The uses of `def` found by a BPE_OVERLAP search over `area`, in search
/// order. Returns false when the visitor stopped early.
bool uses_overlapping(const CellDef& def, const Rect& area, UseIndexVisitor visit);
/// The same with the session's compat (UseIndex::search(area, compat, ...)):
/// the area search of commands whose output lists the uses it finds.
bool uses_overlapping(const CellDef& def, const Rect& area, const compat::CompatSettings& compat,
                      UseIndexVisitor visit);

/// DBCellReadArea's searches (calma/CalmaWrite.c:316, cif/CIFwrite.c:137,
/// database/DBexpand.c:300-340): a BPE_OVERLAP search of `root` over
/// `area`, then of each found use's child over the area in the child's
/// coordinates, element by element, skipping the remaining elements of an
/// array once the transformed area surrounds the child's bounding box.
/// The writers' prepare phase makes it before enumerating (design D7).
void search_subtree(const CellDef& root, const Rect& area);

/// DBMoveCell / DBScaleEverything: rebuilds `def`'s index after every use
/// moved (UseIndex::rebuild).
void rebuild_use_index(CellDef& def);

}  // namespace hefesto::db
