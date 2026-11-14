// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell-use index (gds-write "Structure ordering", Q-MASK-204,
// Q-MASK-323; openspec/changes/m5-mask-gds-cif/design.md D4, D7). Ported
// from Magic VLSI 8.3.694 (fc5daa9e) bplane/bpMain.c (BPNew, BPAdd,
// BPDelete), database/DBexpand.c (DBCellReadArea, dbReadAreaFunc),
// database/DBcellsrch.c (dbCellSrFunc, DBMoveCell's fresh plane) and
// database/DBcellbox.c (DBArrayOverlap); Magic is Copyright (C) 1985, 1990
// Regents of the University of California and (c) 1995-2002 Juniper
// Networks, Inc. (permissive licenses, THIRD_PARTY_NOTICES.md). The bins
// are in use_index_bins.cpp, the enumeration in use_index_enum.cpp.
// Owner: INDEX.

#include "hefesto/db/use_index.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "hefesto/db/hierarchy.hpp"
#include "use_index_impl.hpp"

namespace hefesto::db {

namespace detail = use_index_detail;

struct UseIndex::State {
  detail::Plane plane;
  std::unordered_map<const CellUse*, std::unique_ptr<detail::Element>> elements;
  // Insertion order of each use (the magic-compat OFF order's tie-break).
  std::unordered_map<const CellUse*, std::uint64_t> inserted;
  std::uint64_t next_insertion = 0;
  // Bumped by every change of the structure; an early-stopped search
  // undoes its late sub-binnings only when nothing changed since.
  std::uint64_t generation = 0;
};

namespace {

Rect include_rect(const Rect& a, const Rect& b) noexcept {
  return Rect{std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

std::size_t list_length(const detail::Element* e) noexcept {
  std::size_t n = 0;
  for (; e != nullptr; e = e->link) {
    ++n;
  }
  return n;
}

std::size_t max_population(const detail::BinArray& ba) noexcept {
  std::size_t most = 0;
  for (const detail::Bin& bin : ba.bins) {
    most = std::max(most, bin.sub != nullptr ? max_population(*bin.sub) : list_length(bin.list));
  }
  return most;
}

// The sub-binnings Magic's lazy walk would not have reached when the
// visitor stopped at the `stopped_at`-th match (1-based): those made after
// that many matches had been handed out.
void undo_late_subbins(const std::vector<detail::SubbinRecord>& subbins, std::size_t stopped_at) {
  for (auto it = subbins.rbegin(); it != subbins.rend(); ++it) {
    if (it->matches_before >= stopped_at) {
      detail::undo_subbin(*it);
    }
  }
}

}  // namespace

UseIndex::UseIndex(const CellDef* owner) : owner_(owner), state_(std::make_unique<State>()) {}

UseIndex::~UseIndex() = default;

// BPAdd.
void UseIndex::add_locked(CellUse& use) const {
  State& s = *state_;
  auto element = std::make_unique<detail::Element>();
  element->use = &use;
  element->serial = use.serial;
  element->rect = use.bbox;
  detail::Element& e = *element;
  s.elements.emplace(&use, std::move(element));
  s.inserted.try_emplace(&use, s.next_insertion++);
  detail::Plane& p = s.plane;
  ++p.count;
  p.equal.add(e);
  p.bbox = p.count == 1 ? e.rect : include_rect(p.bbox, e.rect);
  ++s.generation;
  if (p.root != nullptr && surround(p.bin_area, e.rect)) {
    detail::bin_add(*p.root, e);
    return;
  }
  detail::push_front(p.inbox, e);
}

// BPDelete (the bounding box is never shrunk; bp_bbox_exact only matters
// to BPBBox, which Magic never calls).
void UseIndex::delete_locked(const CellUse& use) const {
  State& s = *state_;
  const auto it = s.elements.find(&use);
  if (it == s.elements.end()) {
    return;
  }
  detail::Element& e = *it->second;
  --s.plane.count;
  s.plane.equal.remove(e);
  detail::unlink(e);
  s.elements.erase(it);
  ++s.generation;
}

// Follows the owner's use list: uses removed from it (or destroyed and
// replaced at the same address) leave the index, new uses are placed and
// moved uses re-placed, in uses() order. Runs only when uses() was taken
// for writing or the counts differ.
void UseIndex::sync_locked() const {
  if (owner_ == nullptr) {
    return;
  }
  const auto& uses = owner_->uses();
  if (!owner_->take_uses_touched() && uses.size() == state_->elements.size()) {
    return;
  }
  std::unordered_map<const CellUse*, std::uint64_t> present;
  present.reserve(uses.size());
  for (const auto& u : uses) {
    present.emplace(u.get(), u->serial);
  }
  std::vector<const CellUse*> gone;
  for (const auto& [use, element] : state_->elements) {
    const auto it = present.find(use);
    if (it == present.end() || it->second != element->serial) {
      gone.push_back(use);
    }
  }
  for (const CellUse* use : gone) {
    delete_locked(*use);
    state_->inserted.erase(use);
  }
  for (const auto& u : uses) {
    const auto it = state_->elements.find(u.get());
    if (it == state_->elements.end()) {
      add_locked(*u);
    } else if (it->second->rect != u->bbox) {
      delete_locked(*u);
      add_locked(*u);
    }
  }
}

void UseIndex::place(CellUse& use) {
  const std::lock_guard lock(mutex_);
  delete_locked(use);
  add_locked(use);
}

void UseIndex::remove(const CellUse& use) {
  const std::lock_guard lock(mutex_);
  delete_locked(use);
  state_->inserted.erase(&use);
}

void UseIndex::rebuild() {
  const std::lock_guard lock(mutex_);
  sync_locked();
  std::vector<detail::Element*> order = detail::walk_all(state_->plane);
  auto fresh = std::make_unique<State>();
  fresh->inserted = std::move(state_->inserted);
  fresh->next_insertion = state_->next_insertion;
  fresh->generation = state_->generation + 1;
  std::vector<CellUse*> uses;
  uses.reserve(order.size());
  for (const detail::Element* e : order) {
    uses.push_back(e->use);
  }
  state_ = std::move(fresh);
  for (auto it = uses.rbegin(); it != uses.rend(); ++it) {  // luhead is a stack
    add_locked(**it);
  }
}

bool UseIndex::search(const Rect& area, UseIndexVisitor visit) const {
  std::vector<CellUse*> found;
  std::vector<detail::SubbinRecord> subbins;
  std::uint64_t generation = 0;
  {
    const std::lock_guard lock(mutex_);
    sync_locked();
    detail::WalkResult r = detail::walk_overlap(state_->plane, area);
    state_->generation += 1;  // the walk may have rebuilt or sub-binned
    generation = state_->generation;
    found.reserve(r.matches.size());
    for (const detail::Element* e : r.matches) {
      found.push_back(e->use);
    }
    subbins = std::move(r.subbins);
  }
  for (std::size_t k = 0; k < found.size(); ++k) {
    if (!visit(*found[k])) {
      const std::lock_guard lock(mutex_);
      if (state_->generation == generation) {
        undo_late_subbins(subbins, k + 1);
      }
      return false;
    }
  }
  return true;
}

bool UseIndex::find_equal(const Rect& rect, UseIndexVisitor visit) const {
  std::vector<CellUse*> found;
  {
    const std::lock_guard lock(mutex_);
    sync_locked();
    for (const detail::Element* e : state_->plane.equal.lookup(rect)) {
      found.push_back(e->use);
    }
  }
  return std::all_of(found.begin(), found.end(), [&](CellUse* u) { return visit(*u); });
}

bool UseIndex::enumerate_all(UseIndexVisitor visit) const {
  const std::vector<CellUse*> all = order(compat::CompatSettings{});
  return std::all_of(all.begin(), all.end(), [&](CellUse* u) { return visit(*u); });
}

std::vector<CellUse*> UseIndex::order(const compat::CompatSettings& compat) const {
  const std::lock_guard lock(mutex_);
  sync_locked();
  std::vector<CellUse*> out;
  out.reserve(state_->elements.size());
  if (compat.magic_compat) {
    for (const detail::Element* e : detail::walk_all(state_->plane)) {
      out.push_back(e->use);
    }
    return out;
  }
  // The reverse of insertion order: newest creation first (CellUse::serial,
  // the `.mag` or GDS read order), then the newest insertion.
  std::vector<std::pair<std::pair<std::uint64_t, std::uint64_t>, CellUse*>> keyed;
  keyed.reserve(state_->elements.size());
  for (const auto& [use, element] : state_->elements) {
    keyed.push_back({{element->serial, state_->inserted.at(use)}, element->use});
  }
  std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  for (const auto& entry : keyed) {
    out.push_back(entry.second);
  }
  return out;
}

std::size_t UseIndex::size() const {
  const std::lock_guard lock(mutex_);
  sync_locked();
  return state_->elements.size();
}

UseIndexState UseIndex::state_for_test() const {
  const std::lock_guard lock(mutex_);
  sync_locked();
  const detail::Plane& p = state_->plane;
  UseIndexState out;
  out.binned = p.root != nullptr;
  for (const detail::Element* e = p.inbox; e != nullptr; e = e->link) {
    out.unbinned.push_back(e->use);
  }
  out.bbox = p.bbox;
  out.bin_area = p.bin_area;
  out.count = static_cast<std::size_t>(p.count);
  out.max_bin_population = p.root != nullptr ? max_population(*p.root) : 0;
  return out;
}

std::vector<CellUse*> uses_in_search_order(const CellDef& def, const compat::CompatSettings& compat) {
  return def.use_index().order(compat);
}

bool uses_overlapping(const CellDef& def, const Rect& area, UseIndexVisitor visit) {
  return def.use_index().search(area, visit);
}

namespace {

struct ElementRange {
  std::int32_t xlo = 0;
  std::int32_t xhi = 0;
  std::int32_t ylo = 0;
  std::int32_t yhi = 0;
};

// DBArrayOverlap, one axis: the element range whose images may overlap
// [lo, hi] (child coordinates, flipped for a negative separation).
void axis_range(Coord sep, Coord lo, Coord hi, Coord box_lo, Coord box_hi, std::int32_t& out_lo,
                std::int32_t& out_hi) {
  const std::int32_t first = std::min(out_lo, out_hi);
  const std::int32_t last = std::max(out_lo, out_hi);
  if (sep < 0) {
    sep = -sep;
    std::swap(lo, hi);
    lo = -lo;
    hi = -hi;
    std::swap(box_lo, box_hi);
    box_lo = -box_lo;
    box_hi = -box_hi;
  }
  std::int32_t a = first;
  std::int32_t b = last;
  if (sep != 0) {
    a = first + (lo - box_hi + sep - 1) / sep;
    b = first + (hi - box_lo) / sep;
  }
  a = std::max(a, first);
  b = std::min(b, last);
  if (out_lo > out_hi) {  // back to the use's own index direction
    const std::int32_t na = last + first - b;
    b = last + first - a;
    a = na;
  }
  out_lo = a;
  out_hi = b;
}

ElementRange array_overlap(const CellUse& use, const Rect& parent_rect) {
  const ArrayInfo& a = use.array;
  ElementRange r{a.xlo, a.xhi, a.ylo, a.yhi};
  if (!a.is_array()) {
    return ElementRange{a.xlo, a.xlo, a.ylo, a.ylo};
  }
  const Rect child = apply(invert(use.transform), parent_rect);
  const Rect box = use.def->bbox();
  axis_range(a.xsep, child.x0, child.x1, box.x0, box.x1, r.xlo, r.xhi);
  axis_range(a.ysep, child.y0, child.y1, box.y0, box.y1, r.ylo, r.yhi);
  return r;
}

int read_area(const CellDef& def, const Rect& area);

// dbCellSrFunc with dbReadAreaFunc as the client: every element of `use`
// near `area`, until one reports that it covered its whole definition.
void read_use(CellUse& use, const Rect& area) {
  if (use.def == nullptr) {
    return;
  }
  const ElementRange r = array_overlap(use, area);
  for (std::int32_t y = r.ylo; y <= r.yhi; ++y) {
    for (std::int32_t x = r.xlo; x <= r.xhi; ++x) {
      const Rect child_area = apply(invert(element_transform(use, x, y)), area);
      if (read_area(*use.def, child_area) == 2 && use.array.is_array()) {
        return;
      }
    }
  }
}

// dbReadAreaFunc: 2 when `area` covers the whole definition.
int read_area(const CellDef& def, const Rect& area) {
  (void)uses_overlapping(def, area, [&](CellUse& u) {
    read_use(u, area);
    return true;
  });
  return surround(area, def.bbox()) ? 2 : 0;
}

}  // namespace

void search_subtree(const CellDef& root, const Rect& area) {
  (void)read_area(root, area);
}

void rebuild_use_index(CellDef& def) {
  def.use_index().rebuild();
}

}  // namespace hefesto::db
