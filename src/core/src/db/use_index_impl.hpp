// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internals of the cell-use index (db/use_index.hpp): the bplane data
// structures and the bin and enumeration routines. Ported from Magic VLSI
// 8.3.694 (fc5daa9e) bplane/bpOpaque.h, bpMain.c, bpBins.c, bpEnum.h and
// bpEnum.c, which are Copyright (c) 1995-2002 Juniper Networks, Inc.
// (permissive license, THIRD_PARTY_NOTICES.md), and utils/ihash.c for the
// order of equal-rectangle lookups.
//
// The structures keep Magic's shape: an element carries its link and a
// back pointer to the link that points at it (e_link, e_linkp), a bin is
// either a list head or a sub-array, the last bin of an array is the
// oversized list, and a bin array's bounding box is pulled back by one on
// its top and right edges. Lists are pushed at the head, so every walk
// sees Magic's order.
//
// Owner: INDEX (openspec/changes/m5-mask-gds-cif/design.md D2, D7).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"

namespace hefesto::db::use_index_detail {

/// bplane.h Tcl-linked parameters (bpBins.c:44-47).
inline constexpr int kMinBinArrayPopulation = 10;  // bpMinBAPop
inline constexpr double kMinAverageBinPopulation = 1.0;  // bpMinAvgBinPop

/// One use in the index (bpOpaque.h Element).
struct Element {
  CellUse* use = nullptr;
  std::uint64_t serial = 0;  // CellUse::serial when placed (identity check)
  Rect rect;                 // e_rect: the use's bbox when placed
  Element* link = nullptr;   // e_link
  Element** linkp = nullptr; // e_linkp
};

/// Pushes `e` at the head of the list `head` (BPAdd's in-box push,
/// bpBinAdd's bin push), keeping the back pointers.
void push_front(Element*& head, Element& e) noexcept;
/// Takes `e` out of the list it is linked in (BPDelete).
void unlink(Element& e) noexcept;

struct BinArray;

/// One bin: a list (BT_LIST) or a sub-array (BT_ARRAY).
struct Bin {
  Element* list = nullptr;
  std::unique_ptr<BinArray> sub;
};

/// bpOpaque.h BinArray. `bbox` is pulled back by one on the top and right
/// edges (bpBinArrayNew); `bins` holds num_bins regular bins and the
/// oversized bin last.
struct BinArray {
  Rect bbox;
  int dx = 0;
  int dy = 0;
  int dim_x = 0;
  int num_bins = 0;
  std::vector<Bin> bins;
};

/// bpBinArea: the area covered by bin `i` of `ba`.
[[nodiscard]] Rect bin_area(const BinArray& ba, int i) noexcept;
/// bpBinAdd: adds `e` to `ba` (recursing into sub-arrays).
void bin_add(BinArray& ba, Element& e) noexcept;
/// bpBinArrayBuild: a bin array over `bbox` holding the list `elements`,
/// sub-binned recursively when `subbin`; nullptr (and `elements` untouched)
/// when no binning makes progress.
[[nodiscard]] std::unique_ptr<BinArray> bin_array_build(const Rect& bbox, Element* elements, bool subbin);
/// bpBinArrayUnbuild: every element of `ba` as one singly linked list (the
/// back pointers are stale until relinked).
[[nodiscard]] Element* bin_array_unbuild(BinArray& ba) noexcept;
/// bpListExceedsQ: whether the list is longer than `n`.
[[nodiscard]] bool list_exceeds(const Element* e, int n) noexcept;

/// The hash table of BPE_EQUAL lookups (IHash over e_rect, 4 initial
/// buckets, grown 4x at 3 entries per bucket). Only the order of the
/// entries with equal rectangles is observable: the newest first, and every
/// resize reverses each chain (iHashResize re-adds at the head).
class EqualTable {
 public:
  void add(Element& e);
  void remove(const Element& e);
  /// The elements whose rectangle is `r`, in lookup order.
  [[nodiscard]] std::vector<Element*> lookup(const Rect& r) const;

 private:
  struct RectHash {
    std::size_t operator()(const Rect& r) const noexcept;
  };
  // Each group holds the chain order reversed: the head is the back.
  std::unordered_map<Rect, std::vector<Element*>, RectHash> groups_;
  std::size_t buckets_ = 4;  // BPNew: IHashInit(4, ...)
  std::size_t entries_ = 0;
};

/// bpOpaque.h BPlane.
struct Plane {
  Rect bbox;                // bp_bbox
  int count = 0;            // bp_count
  Element* inbox = nullptr; // bp_inBox
  Rect bin_area;            // bp_binArea
  std::unique_ptr<BinArray> root;  // bp_rootNode
  EqualTable equal;
};

/// An on-the-fly sub-binning made by a search (bpEnumNextBin), with what
/// undoes it: the bin's list in its original order.
struct SubbinRecord {
  BinArray* array = nullptr;
  int bin = 0;
  std::vector<Element*> list;  // head first
  std::size_t matches_before = 0;
};

/// What a walk over the plane found.
struct WalkResult {
  std::vector<Element*> matches;
  std::vector<SubbinRecord> subbins;
  bool rebuilt = false;  // bpBinsUpdate rebuilt the bins
};

/// BPE_OVERLAP enumeration (BPEnumInit with the area shrunk by one, then
/// BPEnumNext to the end): may rebuild the bins and sub-bin on the fly.
[[nodiscard]] WalkResult walk_overlap(Plane& plane, const Rect& area);
/// BPE_ALL enumeration: every element in bin-walk order, never rebuilds.
[[nodiscard]] std::vector<Element*> walk_all(const Plane& plane);
/// bpBinsUpdate: rebuilds the bins when the in-box list holds 10 or more;
/// returns whether it did.
bool bins_update(Plane& plane);
/// Puts a sub-binned bin back to its original list (undoes a SubbinRecord).
void undo_subbin(const SubbinRecord& record);

}  // namespace hefesto::db::use_index_detail
