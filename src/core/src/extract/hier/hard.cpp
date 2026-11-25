// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hard-way node names (extract-hierarchy "Hard-way node names"): the name
// of an unlabelled node found by tracing the subcell geometry under its tile,
// in the deepest cell that has a node-name label, or generated from the
// first node found. Ported from Magic 8.3.694 (fc5daa9e) extract/ExtHard.c
// (extHardProc, extHardSetLabel, extHardGenerateLabel), extract/ExtSubtree.c
// (extSubtreeHardNode, extSubtreeHardSearch, extSubtreeHardUseFunc) and
// extract/ExtArray.c (extArrayHardNode, extArrayHardSearch); Magic is
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).
//
// Not ported: the default-substrate retry of extHardProc (a region of a
// substrate type off the substrate plane taking the substrate labels).
//
// Owner: EXTHIER.

#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hier_context.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract::hier {

namespace {

/// One step of the search (SearchContext): an element of a use, the area
/// in the element's child coordinates and the transform to the parent.
struct Scope {
  const db::CellUse* use = nullptr;
  std::int32_t x = 0;
  std::int32_t y = 0;
  db::Transform trans;
  db::Rect area;
};

/// HardWay.
struct HardWay {
  Hier* h = nullptr;
  tech::TypeMask mask;
  db::Rect area;  // the tile, parent coordinates
  std::string path;
  bool prefix = true;
  bool autogen = false;
  std::optional<db::Label> label;
};

struct Regions {
  RegionFinder finder;
  NodeRegionList nodes;
  std::vector<TileRef> seeds;  // by discovery order
};

// extHardSetLabel's position: the label clipped to the search area, moved
// to a corner that lies in the region when the label leaves the area.
db::Rect label_spot(const db::CellDef& def, const Regions& r, std::size_t reg, const db::Label& lab,
                    const db::Rect& area, const tech::Technology& tech) {
  if (db::surround(area, lab.rect)) {
    return lab.rect;
  }
  const db::Rect c = clip(lab.rect, area);
  const auto home = tech.home_plane(lab.type);
  if (!home) {
    return lab.rect;
  }
  const db::Plane& plane = def.plane(*home);
  const auto region_at = [&](db::Point p) { return r.finder.node_of(plane, plane.tile_at(p), r.nodes.size()); };
  if (region_at({c.x0, c.y0}) == reg) {
    return db::Rect{c.x0, c.y0, c.x0 + 1, c.y0 + 1};
  }
  if (region_at({c.x1, c.y1}) == reg) {
    return db::Rect{c.x1, c.y1, c.x1, c.y1};
  }
  return lab.rect;
}

bool set_label(HardWay& arg, const Scope& s, const Regions& r, std::size_t reg) {
  const LabelRegion* found = name_label(r.nodes[reg]);
  if (found == nullptr || found->label >= s.use->def->labels().size()) {
    return false;
  }
  const db::Label& old = s.use->def->labels()[found->label];
  db::Label lab = old;
  lab.rect = db::apply(s.trans, label_spot(*s.use->def, r, reg, old, s.area, *arg.h->env.tech));
  lab.justification = db::kNorth;
  lab.text = arg.path + old.text;
  arg.label = std::move(lab);
  return true;
}

void generate_label(HardWay& arg, const Scope& s, const Regions& r) {
  const NodeRegion& reg = r.nodes.front();
  const TileRef& seed = r.seeds.back();  // the region found last heads the list
  db::Label lab;
  const db::Rect at{seed.tile.area.x0, seed.tile.area.y0, seed.tile.area.x0 + 1, seed.tile.area.y0 + 1};
  lab.rect = db::apply(s.trans, clip(at, s.area));
  lab.type = seed.tile.type;
  lab.justification = db::kNorth;
  lab.text = arg.path + generated_name(reg, *arg.h->env.tech);
  arg.label = std::move(lab);
}

bool try_cell(HardWay& arg, const Scope& s) {
  const Env& env = arg.h->env;
  const db::CellDef& def = *s.use->def;
  Regions r{RegionFinder(*env.tech), {}, {}};
  r.finder.keep_clients = true;
  RegionCallbacks cb;
  cb.first = [&](const db::Rect& tile, tech::TileType type, tech::PlaneId plane) {
    db::TileView v;
    v.area = tile;
    v.type = type;
    r.seeds.push_back(TileRef{plane, v});
  };
  auto found = r.finder.find(def, s.area, arg.mask, ConnectTable{env.style->node_conn}, cb);
  bool ok = false;
  if (found.ok() && !found.value().empty()) {
    r.nodes = std::move(found).value();
    if (arg.autogen) {
      generate_label(arg, s, r);
      ok = true;
    } else {
      LabelPassOptions lo;
      lo.sticky = false;
      attach_labels(def, r.finder, r.nodes, *env.style, *env.tech, lo, *env.compat);
      for (std::size_t i = 0; i < r.nodes.size() && !ok; ++i) {
        ok = !r.nodes[i].labels.empty() && set_label(arg, s, r, i);
      }
    }
  }
  reset(def);
  return ok;
}

// extHardProc.
bool hard_proc(HardWay& arg, const Scope& s) {
  const bool top = s.use->parent == arg.h->parent;
  const std::size_t saved = arg.path.size();
  if (arg.prefix || !top) {
    arg.path += db::array_element_name(*s.use, s.x, s.y) + "/";
  }
  if (try_cell(arg, s)) {
    return true;
  }
  for (const db::CellUse* u : uses_in(*s.use->def, s.area)) {
    const bool done = array_elements(*u, s.area, [&](const Element& e) {
      const Scope child{u, e.x, e.y, db::compose(s.trans, e.to_parent), db::apply(db::invert(e.to_parent), s.area)};
      return hard_proc(arg, child);
    });
    if (done) {
      return true;
    }
  }
  arg.path.resize(saved);
  return false;
}

// extSubtreeHardUseFunc over DBArraySr(use, area).
bool search_use(HardWay& arg, const db::CellUse& use) {
  return array_elements(use, arg.area, [&](const Element& e) {
    return hard_proc(arg, Scope{&use, e.x, e.y, e.to_parent, db::apply(db::invert(e.to_parent), arg.area)});
  });
}

// extSubtreeHardSearch.
void subtree_search(HardWay& arg, const FlatTree& et) {
  Hier& h = *arg.h;
  if (&et != &h.cum) {
    (void)search_use(arg, *h.sub_use);
    return;
  }
  for (const auto& one : h.subs) {
    if (one->real_use != nullptr && search_use(arg, *one->real_use)) {
      return;
    }
  }
}

// extArrayHardSearch.
void array_search(HardWay& arg, const FlatTree& et) {
  const ArrayState& a = arg.h->array;
  const bool primary = &et == a.primary;
  const db::Transform& t = primary ? a.ptrans : a.itrans;
  const Scope s{a.use, primary ? a.px : a.ix, primary ? a.py : a.iy, t, db::apply(db::invert(t), arg.area)};
  (void)hard_proc(arg, s);
}

void run_search(HardWay& arg, const FlatTree& et) {
  arg.path.clear();
  if (arg.h->naming == Naming::array) {
    array_search(arg, et);
  } else {
    subtree_search(arg, et);
  }
}

}  // namespace

bool hard_node(Hier& h, const TileRef& t, FlatTree& et) {
  const auto n = node_at(et, t);
  if (!n || *n >= et.nodes.size()) {
    return false;
  }
  HardWay arg;
  arg.h = &h;
  arg.mask = h.env.tech->plane_types(t.plane) & h.env.tech->connects_to(side_type(t.tile));
  arg.area = t.tile.area;
  arg.prefix = h.naming == Naming::subtree;
  run_search(arg, et);
  if (!arg.label) {
    arg.autogen = true;
    run_search(arg, et);
  }
  if (!arg.label) {
    return false;
  }
  const db::Label& lab = *arg.label;
  LabelRegion lr;
  lr.text = lab.text;
  lr.position = {lab.rect.x0, lab.rect.y0};
  lr.type = lab.type;
  lr.port = lab.is_port();
  lr.rect = lab.rect;
  et.nodes[*n].labels = {lr};
  auto& labels = et.def->labels();
  labels.insert(labels.begin(), lab);
  return true;
}

}  // namespace hefesto::extract::hier
