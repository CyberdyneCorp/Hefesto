// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Array interactions (extract-hierarchy "Array interactions"). Ported from
// Magic 8.3.694 (fc5daa9e) extract/ExtArray.c (extArray, extArrayFunc,
// extArrayProcess, extArrayPrimaryFunc, extArrayInterFunc, extArrayAdjust,
// extArrayNodeName); Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include <string>

#include "hefesto/db/paint.hpp"
#include "hefesto/extract/hier/yank.hpp"
#include "hier_context.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract::hier {

namespace {

constexpr const char* kNone = "(none)";

enum class Area : std::uint8_t { a, b, c };

// extArrayNodeName: a cumulative node named through `et1`, else `et2`.
std::string array_node_name(Hier& h, const NodeRegion& np, FlatTree& et1, FlatTree& et2) {
  for (FlatTree* et : {&et1, &et2}) {
    const TileRef tp = tile_of_node(*et, np);
    if (np.plane < et->def->plane_count() && side_type(tp.tile) != tech::kSpace && node_at(*et, tp)) {
      return *tile_to_node(h, tp, *et, true);
    }
  }
  return kNone;
}

// extArrayAdjust(ha, et1 = the element, et2 = the primary).
void array_adjust(Hier& h, FlatTree& et1, FlatTree& et2) {
  for (const NodeRegion& np : h.cum.nodes) {
    if (const auto n = h.table.find(array_node_name(h, np, et1, et2))) {
      HierNode& node = h.table.node(*n);
      node.cap = np.substrate_cap;
      for (std::size_t c = 0; c < node.classes.size() && c < np.classes.size(); ++c) {
        node.classes[c] = np.classes[c];
      }
    }
  }
  adjust_tree(h, h.cum, et1, et1);
  adjust_tree(h, h.cum, et2, et2);
  if (!h.env.options->coupling) {
    return;
  }
  const double cscale = h.env.style->cscale;
  const auto pairs = h.cum.coupling.pairs();
  for (const auto& [key, value] : pairs) {
    const double cap = value / cscale;
    if (cap == 0.0) {
      continue;
    }
    const std::string a = array_node_name(h, h.cum.nodes[key.first], et1, et2);
    const std::string b = array_node_name(h, h.cum.nodes[key.second], et1, et2);
    h.writer->cap(a, b, cap, CapClass::array);
  }
}

// Whether element (x, y) belongs to the area being processed
// (extArrayInterFunc's switch).
bool in_area(const Hier& h, Area which, const Element& e) {
  const ArrayState& a = h.array;
  const bool same_axes = e.to_parent.a != 0;
  switch (which) {
    case Area::a:
      if (e.x == a.px || e.y == a.py) {
        return same_axes ? e.x == a.px : e.y == a.py;
      }
      return true;
    case Area::b:
      return same_axes ? e.y == a.py : e.x == a.px;
    case Area::c:
      return e.x != a.px && e.y != a.py;
  }
  return false;
}

std::unique_ptr<FlatTree> yank_flat(Hier& h, const Element& e) {
  auto flat = new_flat(h.env, h.parent->plane_count());
  h.sub_area = clip(db::apply(e.to_parent, e.use->def->bbox()), h.inter_area);
  yank_element(*e.use, e.x, e.y, h.sub_area, *flat->def, *h.env.tech, false);
  extract_flat(*flat, h.env, h.clip_area, true, h.env.couple_adjust());
  return flat;
}

void element_substrate(Hier& h, const Element& e) {
  const db::CellUse& use = *e.use;
  if (use.array.xlo == use.array.xhi) {
    (void)connect_substrate(h, use, -1, e.y);
  } else if (use.array.ylo == use.array.yhi) {
    (void)connect_substrate(h, use, e.x, -1);
  } else {
    (void)connect_substrate(h, use, e.x, e.y);
  }
}

// extArrayInterFunc.
void inter_element(Hier& h, Area which, const Element& e) {
  ArrayState& a = h.array;
  if ((e.x == a.px && e.y == a.py) || !in_area(h, which, e)) {
    return;
  }
  a.ix = e.x;
  a.iy = e.y;
  a.itrans = e.to_parent;
  auto one = yank_flat(h, e);
  connect_trees(h, *a.primary, *one);
  element_substrate(h, e);
  if (h.env.options->adjust) {
    const tech::TypeMask paint = ~tech::TypeMask::of(tech::kSpace);
    (void)db::copy_paint(*one->def, db::whole_plane(), db::kIdentity, *h.cum.def, *h.env.tech, paint);
    (void)db::copy_paint(*a.primary->def, db::whole_plane(), db::kIdentity, *h.cum.def, *h.env.tech, paint);
    extract_flat(h.cum, h.env, h.clip_area, false, h.env.options->coupling);
    array_adjust(h, *one, *a.primary);
  }
  clear_flat(*one);
  drop_nodes(h.cum);
  h.cum.coupling = CouplingTable{};
  clear_yank_cell(*h.cum.def);
}

// extArrayProcess.
void process(Hier& h, const db::Rect& primary, Area which) {
  const db::CellUse& use = *h.array.use;
  std::unique_ptr<FlatTree> prim;
  const bool found = array_elements(use, primary, [&](const Element& e) {
    h.array.px = e.x;
    h.array.py = e.y;
    h.array.ptrans = e.to_parent;
    prim = yank_flat(h, e);
    return true;
  });
  if (!found) {
    feedback(h, primary, "System error: expected array element but none found", true);
    ++h.result->errors;
    return;
  }
  h.array.primary = prim.get();
  (void)array_elements(use, h.inter_area, [&](const Element& e) {
    inter_element(h, which, e);
    return false;
  });
  h.array.primary = nullptr;
  clear_flat(*prim);
}

void set_area(Hier& h, const db::Rect& area) {
  h.clip_area = area;
  h.inter_area = area;
}

// extArrayFunc.
void array_func(Hier& h, const db::CellUse& use) {
  if (!use.array.is_array()) {
    return;
  }
  const db::CellDef& def = *use.def;
  const std::int32_t halo = h.env.halo;
  const db::Coord tx = has_x(use) ? use.array.xsep : static_cast<db::Coord>(def.bbox().width() + 2);
  const db::Coord ty = has_y(use) ? use.array.ysep : static_cast<db::Coord>(def.bbox().height() + 2);
  const db::Rect sep = db::apply(use.transform, db::Rect{0, 0, tx, ty});
  const db::Rect size = db::apply(use.transform, def.bbox());
  const auto xsep = static_cast<db::Coord>(sep.width());
  const auto ysep = static_cast<db::Coord>(sep.height());
  const auto xsize = static_cast<db::Coord>(size.width());
  const auto ysize = static_cast<db::Coord>(size.height());
  const db::Rect& b = use.bbox;
  h.sub_use = &use;
  h.array.use = &use;
  const db::Rect lower_left{b.x0, b.y0, b.x0 + 1, b.y0 + 1};
  if (ysep <= ysize) {
    set_area(h, db::Rect{b.x0, b.y0 + ysep - halo, b.x0 + xsize + halo, b.y0 + ysize + halo});
    process(h, lower_left, Area::a);
  }
  if (xsep <= xsize) {
    set_area(h, db::Rect{b.x0 + xsep - halo, b.y0, b.x0 + xsize + halo, b.y0 + ysize + halo});
    process(h, lower_left, Area::b);
  }
  if (ysep <= ysize && xsep <= xsize) {
    set_area(h, db::Rect{b.x0 + xsep - halo, b.y1 - ysize - halo, b.x0 + xsize + halo, b.y1 - ysep + halo});
    process(h, db::Rect{b.x0, b.y1 - 1, b.x0 + 1, b.y1}, Area::c);
  }
}

}  // namespace

void extract_arrays(Hier& h) {
  h.table = HierNodeTable(h.env.style->resist_classes.size());
  h.naming = Naming::array;
  h.cap_class = CapClass::array;
  for (const db::CellUse* use : uses_in(*h.parent, db::whole_plane())) {
    if (h.env.cancelled()) {
      break;  // extArrayFunc's SigInterruptPending (ExtArray.c:295, 309)
    }
    array_func(h, *use);
  }
  h.table.write(*h.writer, MergeClass::array, h.env.style->cscale);
  h.naming = Naming::subtree;
  h.cap_class = CapClass::subcell;
}

}  // namespace hefesto::extract::hier
