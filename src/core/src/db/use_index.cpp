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

#include "hefesto/db/array_overlap.hpp"
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
  // The magic-compat ON order of `walk_generation` (enumerations repeat
  // often between changes, e.g. on the DRC path).
  std::vector<CellUse*> walk;
  std::uint64_t walk_generation = ~std::uint64_t{0};
};

namespace {

Rect include_rect(const Rect& a, const Rect& b) noexcept {
  return Rect{std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

bool in_list(const detail::Element* head, const detail::Element& e) noexcept {
  for (; head != nullptr; head = head->link) {
    if (head == &e) {
      return true;
    }
  }
  return false;
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
// Returns whether it undid any.
bool undo_late_subbins(const std::vector<detail::SubbinRecord>& subbins, std::size_t stopped_at) {
  bool undone = false;
  for (auto it = subbins.rbegin(); it != subbins.rend(); ++it) {
    if (it->matches_before >= stopped_at) {
      detail::undo_subbin(*it);
      undone = true;
    }
  }
  return undone;
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

// Whether every use of the owner's list is in the index with the serial and
// box it was placed with, and nothing else is: then there is nothing to
// reconcile (a list taken for writing but only read, or changed only
// through place, remove and refresh, security.md M5-S3).
bool UseIndex::in_sync_locked() const {
  const auto& uses = owner_->uses();
  if (uses.size() != state_->elements.size()) {
    return false;
  }
  return std::all_of(uses.begin(), uses.end(), [&](const std::unique_ptr<CellUse>& u) {
    const auto it = state_->elements.find(u.get());
    return it != state_->elements.end() && it->second->serial == u->serial && it->second->rect == u->bbox;
  });
}

// Follows the owner's use list: uses removed from it (or destroyed and
// replaced at the same address) leave the index, new uses are placed and
// moved uses re-placed, in uses() order. Runs only when uses() was taken
// for writing or the counts differ, and reconciles only when a check
// without allocation finds a difference.
void UseIndex::sync_locked() const {
  if (owner_ == nullptr) {
    return;
  }
  const auto& uses = owner_->uses();
  if (!owner_->take_uses_touched() && uses.size() == state_->elements.size()) {
    return;
  }
  if (in_sync_locked()) {
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

void UseIndex::refresh(const CellUse& use) {
  const std::lock_guard lock(mutex_);
  State& s = *state_;
  const auto it = s.elements.find(&use);
  if (it == s.elements.end() || it->second->rect == use.bbox) {
    return;
  }
  detail::Element& e = *it->second;
  detail::Plane& p = s.plane;
  const bool in_box = in_list(p.inbox, e);
  p.equal.remove(e);
  e.rect = use.bbox;
  p.equal.add(e);
  p.bbox = include_rect(p.bbox, e.rect);
  ++s.generation;
  if (in_box) {
    return;
  }
  detail::unlink(e);
  if (p.root != nullptr && surround(p.bin_area, e.rect)) {
    detail::bin_add(*p.root, e);
    return;
  }
  detail::push_front(p.inbox, e);
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
    if (r.rebuilt || !r.subbins.empty()) {
      ++state_->generation;
    }
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
      if (state_->generation == generation && undo_late_subbins(subbins, k + 1)) {
        ++state_->generation;
      }
      return false;
    }
  }
  return true;
}

bool UseIndex::search(const Rect& area, const compat::CompatSettings& compat, UseIndexVisitor visit) const {
  if (compat.magic_compat) {
    return search(area, visit);
  }
  std::vector<CellUse*> found;
  {
    const std::lock_guard lock(mutex_);
    sync_locked();
    const detail::WalkResult r = detail::walk_overlap(state_->plane, area);
    if (r.rebuilt || !r.subbins.empty()) {
      ++state_->generation;
    }
    found.reserve(r.matches.size());
    for (const detail::Element* e : r.matches) {
      found.push_back(e->use);
    }
    sort_canonical_locked(found);
  }
  return std::all_of(found.begin(), found.end(), [&](CellUse* u) { return visit(*u); });
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
  State& s = *state_;
  if (compat.magic_compat) {
    if (s.walk_generation != s.generation) {
      s.walk.clear();
      for (const detail::Element* e : detail::walk_all(s.plane)) {
        s.walk.push_back(e->use);
      }
      s.walk_generation = s.generation;
    }
    return s.walk;
  }
  std::vector<CellUse*> out;
  out.reserve(s.elements.size());
  for (const auto& entry : s.elements) {
    out.push_back(entry.second->use);
  }
  sort_canonical_locked(out);
  return out;
}

// The magic-compat OFF order: newest creation first (CellUse::serial, the
// `.mag` or GDS read order), then the newest insertion. Every use is in the
// index (the caller holds the lock since it found them).
void UseIndex::sort_canonical_locked(std::vector<CellUse*>& uses) const {
  using Key = std::pair<std::uint64_t, std::uint64_t>;
  std::vector<std::pair<Key, CellUse*>> keyed;
  keyed.reserve(uses.size());
  for (CellUse* use : uses) {
    const auto it = state_->inserted.find(use);
    keyed.push_back({{use->serial, it != state_->inserted.end() ? it->second : 0}, use});
  }
  std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  for (std::size_t k = 0; k < keyed.size(); ++k) {
    uses[k] = keyed[k].second;
  }
}

std::size_t UseIndex::size() const {
  const std::lock_guard lock(mutex_);
  sync_locked();
  return state_->elements.size();
}

std::uint64_t UseIndex::generation() const {
  const std::lock_guard lock(mutex_);
  return state_->generation;
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

bool uses_overlapping(const CellDef& def, const Rect& area, const compat::CompatSettings& compat,
                      UseIndexVisitor visit) {
  return def.use_index().search(area, compat, visit);
}

namespace {

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

namespace {

// selEnumCFunc2's search; `chain` holds the elements from the root down to
// the one being visited.
struct SelectedUseSearch {
  const CellUse& selected;
  const compat::CompatSettings& compat;
  const DefReader& read;
  std::vector<UseElement> chain;

  // DBCellSrArea over `def` (`area` in its coordinates, `to_root` mapping
  // them to the root's); false once found.
  bool search(const CellDef& def, const Rect& area, const Transform& to_root) {
    return uses_overlapping(def, area, compat, [&](CellUse& u) {
      if (u.def == nullptr) {
        return true;
      }
      const ElementRange r = array_overlap(u, area);  // dbCellSrFunc's elements
      for (std::int64_t y = r.ylo; y <= r.yhi; ++y) {
        for (std::int64_t x = r.xlo; x <= r.xhi; ++x) {
          if (!element(u, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), area, to_root)) {
            return false;
          }
        }
      }
      return true;
    });
  }

  // One element: the match, else checkChildren (the child's own search).
  bool element(CellUse& u, std::int32_t x, std::int32_t y, const Rect& area, const Transform& to_root) {
    const Transform et = element_transform(u, x, y);
    const Transform t = compose(to_root, et);
    chain.push_back(UseElement{&u, x, y});
    if (u.def == selected.def && t == selected.transform && u.array == selected.array) {
      return false;
    }
    if (read && !u.def->flags().available) {
      read(*u.def);
    }
    if (!u.def->flags().not_found && !search(*u.def, apply(invert(et), area), t)) {
      return false;
    }
    chain.pop_back();
    return true;
  }
};

}  // namespace

std::vector<UseElement> find_selected_use(const CellDef& root, const CellUse& selected,
                                          const compat::CompatSettings& compat, const DefReader& read) {
  if (selected.def == nullptr) {
    return {};
  }
  const Rect box = apply(selected.transform, selected.def->bbox());
  SelectedUseSearch s{selected, compat, read, {}};
  if (s.search(root, Rect{box.x0, box.y0, box.x0 + 1, box.y0 + 1}, kIdentity)) {
    return {};
  }
  return std::move(s.chain);
}

void search_subtree(const CellDef& root, const Rect& area) {
  (void)read_area(root, area);
}

void rebuild_use_index(CellDef& def) {
  def.use_index().rebuild();
}

}  // namespace hefesto::db
