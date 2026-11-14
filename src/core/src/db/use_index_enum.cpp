// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Enumeration of the cell-use index (gds-write "Structure ordering",
// Q-MASK-204). Ported from Magic VLSI 8.3.694 (fc5daa9e) bplane/bpEnum.c
// (BPEnumInit) and bplane/bpEnum.h (bpEnumMatchQ, bpEnumPushInside,
// bpEnumPush, bpEnumNextBin1, bpEnumNextBin, bpEnumNextBINS,
// bpEnumNextINBOX), Copyright (c) 1995-2002 Juniper Networks, Inc.
// (permissive license, THIRD_PARTY_NOTICES.md).
//
// Magic's enumeration is a lazy state machine over a stack of bin arrays;
// this port runs it to the end and collects the elements, in the same
// order. The side effects are Magic's: a non-surrounding BPE_OVERLAP
// search rebuilds the bins first (bpBinsUpdate), and leaving a bin that
// rejected 10 or more elements sub-bins it on the fly. Each on-the-fly
// sub-binning is recorded, so a caller whose visitor stops early can undo
// the ones Magic's lazy walk would not have reached. Owner: INDEX.

#include <vector>

#include "use_index_impl.hpp"

namespace hefesto::db::use_index_detail {

namespace {

// bpEnumMatchQ: the element touches the (shrunk) search area.
bool matches(const Rect& r, const Rect& area) noexcept {
  return r.x1 >= area.x0 && r.x0 <= area.x1 && r.y1 >= area.y0 && r.y0 <= area.y1;
}

// GEO_SURROUND.
bool surrounds(const Rect& outer, const Rect& inner) noexcept {
  return inner.x0 >= outer.x0 && inner.x1 <= outer.x1 && inner.y0 >= outer.y0 && inner.y1 <= outer.y1;
}

// bpOpaque.h BPStack: one bin array being walked.
struct Frame {
  BinArray* node = nullptr;
  bool inside = false;  // BPS_BINS_INSIDE
  int i = 0;
  int row_max = 0;
  int row_delta = 0;
  int max = 0;
  int dim_x = 0;
  bool subbin = false;
  int rejects = 0;
};

class Walk {
 public:
  Walk(Plane& plane, const Rect& area, bool all) : plane_(plane), area_(area), all_(all) {}

  WalkResult run() {
    bool inside = all_;
    if (!all_) {
      area_ = Rect{area_.x0 + 1, area_.y0 + 1, area_.x1 - 1, area_.y1 - 1};  // GEO_EXPAND(-1)
      inside = surrounds(area_, plane_.bbox);
      if (!inside) {
        sub_min_x_ = (area_.x1 - area_.x0) / 2;
        sub_min_y_ = (area_.y1 - area_.y0) / 2;
        result_.rebuilt = bins_update(plane_);
      }
    }
    if (plane_.root != nullptr) {
      push(*plane_.root, inside);
      inside = walk_bins();
    }
    walk_inbox(inside);
    return std::move(result_);
  }

 private:
  // bpEnumPushInside / bpEnumPush.
  void push(BinArray& ba, bool inside) {
    Frame f;
    f.node = &ba;
    if (inside || surrounds(area_, ba.bbox)) {
      f.inside = true;
      f.i = -1;
      f.max = ba.num_bins;
      stack_.push_back(f);
      return;
    }
    set_range(f, ba);
    stack_.push_back(f);
  }

  // bpEnumPush: the bins of `ba` the search area can reach.
  void set_range(Frame& f, const BinArray& ba) const {
    Rect r{area_.x0 - ba.dx, area_.y0 - ba.dy, area_.x1 + 1, area_.y1 + 1};
    r = Rect{std::max(r.x0, ba.bbox.x0), std::max(r.y0, ba.bbox.y0), std::min(r.x1, ba.bbox.x1),
             std::min(r.y1, ba.bbox.y1)};  // GEOCLIP
    if (r.x0 >= r.x1 || r.y0 >= r.y1) {   // GEO_RECTNULL: only the oversized bin
      f.i = 0;
      f.row_max = 0;
      f.max = 0;
      return;
    }
    const int x0 = (r.x0 - ba.bbox.x0) / ba.dx;
    const int x1 = (r.x1 - ba.bbox.x0) / ba.dx;
    const int y0 = (r.y0 - ba.bbox.y0) / ba.dy;
    const int y1 = (r.y1 - ba.bbox.y0) / ba.dy;
    const int first = y0 * ba.dim_x + x0;
    f.i = first - 1;
    f.row_max = first + x1 - x0;
    f.max = y1 * ba.dim_x + x1;
    f.row_delta = ba.dim_x + x0 - x1;
    f.dim_x = ba.dim_x;
    f.subbin = ba.dx >= sub_min_x_ || ba.dy >= sub_min_y_;
  }

  // The BINS states: scan the current list, then advance, until the stack
  // empties. Returns the state of the root frame when it was popped, which
  // decides whether the in-box list is matched (bpEnumNextBin).
  bool walk_bins() {
    bool last_inside = false;
    while (!stack_.empty()) {
      scan_list();
      last_inside = stack_.back().inside;
      next_bin();
    }
    return last_inside;
  }

  // bpEnumNextBINS: the matching elements of the current list.
  void scan_list() {
    Frame& f = stack_.back();
    for (Element* e = next_; e != nullptr; e = e->link) {
      if (f.inside || matches(e->rect, area_)) {
        result_.matches.push_back(e);
      } else {
        ++f.rejects;
      }
    }
    next_ = nullptr;
  }

  // bpEnumNextBin: sub-bin the bin just left if it rejected enough, then
  // move to the next bin, push into a sub-array, or pop.
  void next_bin() {
    Frame& f = stack_.back();
    if (!f.inside) {
      maybe_subbin(f);
      f.rejects = 0;
    }
    if (!advance(f)) {
      stack_.pop_back();
      return;
    }
    Bin& bin = f.node->bins[static_cast<std::size_t>(f.i)];
    if (bin.sub == nullptr) {
      next_ = bin.list;  // bpEnumNextBin1
    } else {
      push(*bin.sub, f.inside);
    }
  }

  void maybe_subbin(const Frame& f) {
    const bool oversized = f.i == f.node->num_bins;
    if (f.rejects < kMinBinArrayPopulation || !(f.subbin || oversized)) {
      return;
    }
    Bin& bin = f.node->bins[static_cast<std::size_t>(f.i)];
    SubbinRecord record{f.node, f.i, {}, result_.matches.size()};
    for (Element* e = bin.list; e != nullptr; e = e->link) {
      record.list.push_back(e);
    }
    auto sub = bin_array_build(bin_area(*f.node, f.i), bin.list, false);  // not recursively
    if (sub != nullptr) {
      bin.list = nullptr;
      bin.sub = std::move(sub);
      result_.subbins.push_back(std::move(record));
    }
  }

  // The index arithmetic of bpEnumNextBin; false when the array is done.
  static bool advance(Frame& f) noexcept {
    if (f.inside) {
      if (f.i < f.max) {
        ++f.i;
        return true;
      }
      return false;
    }
    if (f.i < f.row_max) {
      ++f.i;
    } else if (f.i < f.max) {
      f.i += f.row_delta;
      f.row_max += f.dim_x;
    } else if (f.i == f.max) {
      f.i = f.node->num_bins;
    } else {
      return false;
    }
    return true;
  }

  // bpEnumNextINBOX.
  void walk_inbox(bool inside) {
    for (Element* e = plane_.inbox; e != nullptr; e = e->link) {
      if (inside || matches(e->rect, area_)) {
        result_.matches.push_back(e);
      }
    }
  }

  Plane& plane_;
  Rect area_;
  bool all_;
  int sub_min_x_ = 0;
  int sub_min_y_ = 0;
  std::vector<Frame> stack_;
  Element* next_ = nullptr;
  WalkResult result_;
};

}  // namespace

WalkResult walk_overlap(Plane& plane, const Rect& area) {
  return Walk(plane, area, false).run();
}

std::vector<Element*> walk_all(const Plane& plane) {
  // BPE_ALL never rebuilds nor sub-bins (every frame is inside), so the
  // plane is not modified.
  return Walk(const_cast<Plane&>(plane), Rect{}, true).run().matches;
}

}  // namespace hefesto::db::use_index_detail
