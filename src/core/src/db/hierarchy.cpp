// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Instances, identifiers, arrays and bounding boxes (layout-hierarchy).
// Ported from Magic VLSI 8.3.694 (fc5daa9e): database/DBcellname.c
// (DBLinkCell, dbLinkFunc, DBReLinkCell, DBFindUse, DBGenerateUniqueIds),
// database/DBcellbox.c (DBPrintUseId, DBComputeUseBbox, DBMakeArray,
// DBGetArrayTransform, dbReComputeBboxFunc) and database/DBbound.c
// (DBBoundCellPlane). Flattening lives in flatten.cpp. Magic is Copyright
// (C) 1985, 1990 Regents of the University of California (permissive
// license, THIRD_PARTY_NOTICES.md).

#include "hefesto/db/hierarchy.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/change_recorder.hpp"
#include "hefesto/db/diag_codes.hpp"

namespace hefesto::db {

namespace {

bool contains_def(const CellDef& root, const CellDef& target) {
  if (&root == &target) {
    return true;
  }
  return std::any_of(root.uses().begin(), root.uses().end(),
                     [&](const auto& u) { return u->def != nullptr && contains_def(*u->def, target); });
}

std::string_view last_component(std::string_view name) {
  const auto slash = name.rfind('/');
  return slash == std::string_view::npos ? name : name.substr(slash + 1);
}

// dbLinkFunc + the DBLinkCell loop: "<base>_<n>" with the smallest free n.
std::string default_use_id(const CellDef& parent, std::string_view base) {
  std::set<std::string, std::less<>> taken;
  for (const auto& u : parent.uses()) {
    const std::string_view id = u->id;
    if (id.size() > base.size() + 1 && id.substr(0, base.size()) == base && id[base.size()] == '_') {
      taken.insert(u->id);
    }
  }
  for (std::int64_t n = 0;; ++n) {
    std::string candidate = std::string{base} + "_" + std::to_string(n);
    if (!taken.contains(candidate)) {
      return candidate;
    }
  }
}

CellUse* find_exact(const CellDef& parent, std::string_view id) {
  for (const auto& u : parent.uses()) {
    if (u->id == id) {
      return u.get();
    }
  }
  return nullptr;
}

Rect raw_label_bounds(const Rect& area, const Rect& label) {
  return Rect{std::min(area.x0, label.x0), std::min(area.y0, label.y0), std::max(area.x1, label.x1),
              std::max(area.y1, label.y1)};
}

void fix_degenerate(Rect& r) {
  if (r.x0 == r.x1) {
    r.x1 = r.x0 + 1;
  }
  if (r.y0 == r.y1) {
    r.y1 = r.y0 + 1;
  }
}

struct BoxState {
  bool found = false;
  Rect area;      // GeoNullRect
  Rect extended;  // GeoNullRect
};

// DBBoundCellPlane: the union of the use boxes.
void include_uses(const CellDef& def, BoxState& s) {
  for (const auto& u : def.uses()) {
    if (s.found) {
      s.extended = include(s.extended, u->extended);
      s.area = include(s.area, u->bbox);
    } else {
      s.extended = u->extended;
      s.area = u->bbox;
      s.found = true;
    }
  }
  if (!s.found) {
    s.extended = Rect{};
  }
}

void include_paint(const CellDef& def, bool magic_compat, BoxState& s) {
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    if (p == tech::kPlaneDrcCheck || (!magic_compat && p < tech::kFirstUserPlane)) {
      continue;
    }
    const Rect r = def.plane(static_cast<tech::PlaneId>(p)).bounding_box();
    if (r.is_null()) {
      continue;
    }
    s.area = s.found ? include(s.area, r) : r;
    s.found = true;
  }
}

void include_labels(const CellDef& def, BoxState& s) {
  for (const Label& l : def.labels()) {
    s.area = s.found ? raw_label_bounds(s.area, l.rect) : l.rect;
    s.found = true;
  }
  s.extended = include(s.extended, s.area);
  if (s.found) {
    for (const Label& l : def.labels()) {
      if (l.font >= 0 && l.text_bbox) {
        s.extended = include(s.extended, *l.text_bbox);
      }
    }
  }
}

void visit_elements(const CellUse& use, const std::function<void(std::int32_t, std::int32_t)>& f) {
  const ArrayInfo& a = use.array;
  for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
    for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
      f(x, y);
    }
  }
}

bool walk(const CellDef& def, const Transform& to_root, int depth, const UseVisitor& visit) {
  for (const auto& u : def.uses()) {
    bool stop = false;
    visit_elements(*u, [&](std::int32_t x, std::int32_t y) {
      if (stop) {
        return;
      }
      const Transform t = compose(to_root, element_transform(*u, x, y));
      if (visit(*u, t, depth) && u->def != nullptr) {
        stop = !walk(*u->def, t, depth + 1, visit);
      }
    });
    if (stop) {
      return false;
    }
  }
  return true;
}

struct LabelSearch {
  Descend mode;
  LabelMatch match;
  const std::function<bool(const LabelHit&)>& visit;

  /// GEO_TOUCH at the root, GEO_OVERLAP with the area grown by one in
  /// subcells (DBTreeFindUse / dbCellLabelSrFunc).
  [[nodiscard]] bool hit(const Label& l, const Rect& area, bool root) const {
    const auto meets = [&area, root](const Rect& r) { return root ? touch(r, area) : overlap(r, expand(area, 1)); };
    if (meets(l.rect)) {
      return true;
    }
    return match == LabelMatch::display && l.font >= 0 && l.text_bbox && meets(*l.text_bbox);
  }
};

bool search(const CellDef& def, const Rect& area, const std::string& path, const Transform& to_root, bool root,
            const LabelSearch& s) {
  const auto& visit = s.visit;
  const Descend mode = s.mode;
  for (const Label& l : def.labels()) {
    if (s.hit(l, area, root) && !visit(LabelHit{&l, path, to_root})) {
      return false;
    }
  }
  for (const auto& u : def.uses()) {
    if (u->def == nullptr || !descends(*u, mode)) {
      continue;
    }
    bool go_on = true;
    visit_elements(*u, [&](std::int32_t x, std::int32_t y) {
      if (!go_on) {
        return;
      }
      const Transform et = element_transform(*u, x, y);
      const Rect child_area = apply(invert(et), area);
      go_on = search(*u->def, child_area, path + array_element_name(*u, x, y) + "/", compose(to_root, et), false, s);
    });
    if (!go_on) {
      return false;
    }
  }
  return true;
}

}  // namespace

diag::Outcome<CellUse*> place(CellDef& parent, CellDef& child, const PlaceOptions& options) {
  if (contains_def(child, parent)) {
    return diag::make(codes::kUseRecursive,
                      "placing \"" + child.name() + "\" in \"" + parent.name() + "\" would make it contain itself");
  }
  std::string id;
  if (options.id) {
    if (options.id->empty()) {
      return diag::make(codes::kUseIdInvalid, "an instance identifier cannot be empty");
    }
    if (find_use(parent, *options.id) != nullptr) {
      return diag::make(codes::kUseIdTaken, "instance \"" + *options.id + "\" already exists in \"" +
                                                parent.name() + "\"");
    }
    id = *options.id;
  } else {
    id = default_use_id(parent, last_component(child.name()));
  }
  auto use = std::make_unique<CellUse>();
  use->serial = next_use_serial();
  use->id = std::move(id);
  use->def = &child;
  use->parent = &parent;
  use->transform = options.transform;
  use->array = options.array;
  use->locked = options.locked;
  compute_use_bbox(*use);
  CellUse* raw = use.get();
  parent.link_use_id(raw->id);  // DBLinkCell
  parent.uses().push_back(std::move(use));
  child.instances().insert(child.instances().begin(), raw);
  if (ChangeRecorder* r = parent.recorder()) {
    r->use_placed(parent, *raw);
  }
  mark_modified(parent);
  return raw;
}

diag::Status remove_use(CellUse& use) {
  CellDef* parent = use.parent;
  if (use.def != nullptr) {
    auto& inst = use.def->instances();
    inst.erase(std::remove(inst.begin(), inst.end(), &use), inst.end());
  }
  if (parent == nullptr) {
    return diag::make(codes::kUseNotFound, "instance \"" + use.id + "\" has no parent");
  }
  if (ChangeRecorder* r = parent->recorder()) {
    r->use_deleted(*parent, use);
  }
  mark_modified(*parent);
  parent->unlink_use_id(use.id);  // DBUnLinkCell
  auto& uses = parent->uses();
  uses.erase(std::remove_if(uses.begin(), uses.end(), [&](const auto& u) { return u.get() == &use; }), uses.end());
  return diag::Done{};
}

CellUse* find_use(const CellDef& parent, std::string_view id) {
  const auto bracket = id.rfind('[');
  if (bracket != std::string_view::npos) {
    if (CellUse* u = find_exact(parent, id.substr(0, bracket))) {
      return u;
    }
  }
  return find_exact(parent, id);
}

diag::Status rename_use(CellUse& use, std::string_view new_id) {
  if (use.id == new_id) {
    return diag::Done{};
  }
  if (new_id.empty()) {
    return diag::make(codes::kUseIdInvalid, "an instance identifier cannot be empty");
  }
  if (use.parent != nullptr && find_use(*use.parent, new_id) != nullptr) {
    return diag::make(codes::kUseIdTaken, "instance \"" + std::string{new_id} + "\" already exists");
  }
  const std::string old_id = std::move(use.id);
  use.id = std::string{new_id};
  if (use.parent != nullptr) {
    use.parent->unlink_use_id(old_id);  // DBReLinkCell
    use.parent->link_use_id(use.id);
    if (ChangeRecorder* r = use.parent->recorder()) {
      r->use_id_cleared(*use.parent, use, old_id);
      r->use_id_set(*use.parent, use, use.id);
    }
    set_modified_flag(*use.parent);
  }
  return diag::Done{};
}

void set_use_locked(CellUse& use, bool locked) {
  const bool changed = use.locked != locked;
  use.locked = locked;
  if (changed && use.parent != nullptr) {
    if (ChangeRecorder* r = use.parent->recorder()) {
      r->use_locked(*use.parent, use, locked);
    }
    set_modified_flag(*use.parent);  // Q-MODEL-13 (fix)
  }
}

std::vector<CellUse*> uses_in_search_order(const CellDef& def) {
  std::vector<CellUse*> out;
  out.reserve(def.uses().size());
  for (auto it = def.uses().rbegin(); it != def.uses().rend(); ++it) {
    out.push_back(it->get());
  }
  return out;
}

// DBGenerateUniqueIds: dbFindNamesFunc then dbGenerateUniqueIdsFunc, both in
// subcell enumeration order (database/DBcellname.c:2640-2770).
std::size_t make_use_ids_unique(CellDef& parent, bool magic_compat) {
  std::set<std::string, std::less<>> names;
  const std::vector<CellUse*> order = uses_in_search_order(parent);
  for (CellUse* u : order) {
    if (!u->id.empty() && !names.insert(u->id).second) {
      parent.unlink_use_id(u->id);  // dbFindNamesFunc
      u->id.clear();
    }
  }
  std::map<const CellDef*, std::int64_t> next_suffix;
  std::size_t assigned = 0;
  for (CellUse* u : order) {
    if (!u->id.empty()) {
      parent.link_use_id(u->id);  // dbGenerateUniqueIdsFunc's setHash
      continue;
    }
    if (u->def == nullptr) {
      continue;
    }
    const std::string_view base = magic_compat ? std::string_view{u->def->name()} : last_component(u->def->name());
    std::int64_t suffix = next_suffix[u->def];
    std::string name;
    for (;; ++suffix) {
      name = std::string{base} + "_" + std::to_string(suffix);
      if (!names.contains(name)) {
        break;
      }
    }
    if (!magic_compat) {
      names.insert(name);  // distinct children may share a base name
    }
    u->id = std::move(name);
    parent.link_use_id(u->id);
    next_suffix[u->def] = suffix + 1;
    ++assigned;
  }
  return assigned;
}

std::string array_element_name(const CellUse& use, std::int32_t x, std::int32_t y) {
  const ArrayInfo& a = use.array;
  if (!a.is_array()) {
    return use.id;
  }
  if (a.xlo == a.xhi) {
    return use.id + "[" + std::to_string(y) + "]";
  }
  if (a.ylo == a.yhi) {
    return use.id + "[" + std::to_string(x) + "]";
  }
  return use.id + "[" + std::to_string(y) + "," + std::to_string(x) + "]";
}

Transform element_transform(const CellUse& use, std::int32_t x, std::int32_t y) {
  const ArrayInfo& a = use.array;
  const Coord xsep = a.xlo > a.xhi ? -a.xsep : a.xsep;
  const Coord ysep = a.ylo > a.yhi ? -a.ysep : a.ysep;
  return compose(use.transform, translation(xsep * (x - a.xlo), ysep * (y - a.ylo)));
}

void make_array(CellUse& use, const Transform& root_to_child, const ArrayInfo& root_array) {
  ArrayInfo a = root_array;
  a.xsep = root_to_child.a * root_array.xsep + root_to_child.b * root_array.ysep;
  a.ysep = root_to_child.d * root_array.xsep + root_to_child.e * root_array.ysep;
  if (root_to_child.a == 0) {
    std::swap(a.xlo, a.ylo);
    std::swap(a.xhi, a.yhi);
  }
  use.array = a;
  compute_use_bbox(use);
  if (use.parent != nullptr) {
    mark_modified(*use.parent);
  }
}

/// `base + delta` held to the plane's coordinate range. Array extents come
/// from files, so a corrupt one must not overflow (found by fuzz_mag_reader).
Coord shifted(Coord base, std::int64_t delta) noexcept {
  const std::int64_t v = std::clamp<std::int64_t>(std::int64_t{base} + delta, kMinusInfinity, kInfinity);
  return static_cast<Coord>(v);
}

/// |sep * (hi - lo)| in 64 bits.
std::int64_t array_extent(Coord sep, Coord lo, Coord hi) noexcept {
  return std::abs(std::int64_t{sep} * (std::int64_t{hi} - std::int64_t{lo}));
}

void compute_use_bbox(CellUse& use) {
  if (use.def == nullptr) {
    return;
  }
  const ArrayInfo& a = use.array;
  const std::int64_t xdelta = array_extent(a.xsep, a.xlo, a.xhi);
  const std::int64_t ydelta = array_extent(a.ysep, a.ylo, a.yhi);
  const auto grow = [&](const Rect& box) {
    Rect r = box;
    if (a.xsep < 0) {
      r.x0 = shifted(r.x0, -xdelta);
    } else {
      r.x1 = shifted(r.x1, xdelta);
    }
    if (a.ysep < 0) {
      r.y0 = shifted(r.y0, -ydelta);
    } else {
      r.y1 = shifted(r.y1, ydelta);
    }
    return r;
  };
  use.bbox = apply(use.transform, grow(use.def->bbox()));
  use.extended = apply(use.transform, grow(use.def->extended_bbox()));
}

namespace {

std::atomic<std::uint64_t> g_use_serial{0};

// Whether Magic's cd_parents list of `def` ends with a window's root use:
// root uses are never freed, so this holds when `def` was shown in a window
// before every use that still places it.
bool window_use_is_oldest(const CellDef& def) {
  const std::uint64_t window = def.first_window_serial();
  if (window == 0) {
    return false;
  }
  return std::all_of(def.instances().begin(), def.instances().end(),
                     [window](const CellUse* use) { return use->parent == nullptr || use->serial > window; });
}

}  // namespace

std::uint64_t next_use_serial() noexcept {
  return g_use_serial.fetch_add(1, std::memory_order_relaxed) + 1;
}

void CellDef::note_window_root() noexcept {
  if (first_window_serial_ == 0) {
    first_window_serial_ = next_use_serial();
  }
}

void recompute_bbox(CellDef& def, bool magic_compat) {
  BoxState s;
  include_uses(def, s);
  include_paint(def, magic_compat, s);
  include_labels(def, s);
  if (!s.found) {
    s.area = Rect{0, 0, 1, 1};
    s.extended = s.area;
  }
  fix_degenerate(s.area);
  fix_degenerate(s.extended);
  def.set_bboxes(s.area, s.extended);
  // dbReComputeBboxFunc walks cd_parents (most recent use first) and
  // recomputes a parent when the next use has another one. Its final call is
  // guarded by the parent of the *last* use: when that is a window's root
  // use, the pending parent keeps its old bounding box (Magic ref:
  // database/DBcellbox.c:755-775). Replicated with magic-compat ON; OFF
  // recomputes it (Q-MODEL-32, resolution: compat).
  CellDef* last = nullptr;
  for (CellUse* use : def.instances()) {
    compute_use_bbox(*use);
    CellDef* parent = use->parent;
    if (parent == nullptr) {
      continue;
    }
    parent->flags().child_bboxes_changed = true;
    if (last != parent) {
      if (last != nullptr) {
        recompute_bbox(*last, magic_compat);
      }
      last = parent;
    }
  }
  if (last != nullptr && (!magic_compat || !window_use_is_oldest(def))) {
    recompute_bbox(*last, magic_compat);
  }
}

void update_bounding_boxes(CellDef& def, const tech::Technology& /*tech*/) {
  recompute_bbox(def);
}

bool descends(const CellUse& use, Descend mode) {
  return use.def != nullptr && descends(*use.def, mode);
}

bool descends(const CellDef& def, Descend mode) {
  switch (mode) {
    case Descend::all:
      return true;
    case Descend::no_subcircuit:
      return !is_subcircuit(def);
    case Descend::no_vendor:
      return !def.flags().vendor_gds;
    case Descend::property_flatten:
      return def.properties().find("flatten") != nullptr;
  }
  return false;
}

void walk_hierarchy(const CellDef& root, const UseVisitor& visit) {
  (void)walk(root, kIdentity, 0, visit);
}

void search_labels(const CellDef& root, const Rect& area, const std::function<bool(const LabelHit&)>& visit,
                   Descend descend, LabelMatch match) {
  (void)search(root, area, std::string{}, kIdentity, true, LabelSearch{descend, match, visit});
}

}  // namespace hefesto::db
