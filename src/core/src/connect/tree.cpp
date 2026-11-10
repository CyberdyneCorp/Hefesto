// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchical tile, label and cell searches (connect/tree.hpp). Owner: SELECT.
// (Magic ref: database/DBcellsrch.c DBTreeSrTiles, DBTreeSrLabels,
// DBTreeSrCells; database/DBcell.c DBCellSrArea)

#include "hefesto/connect/tree.hpp"

#include <algorithm>

#include "hefesto/db/hierarchy.hpp"

namespace hefesto::connect {

namespace {

/// What every recursive search carries.
struct Walk {
  const Descend* descend = nullptr;
  [[nodiscard]] bool descends(const db::CellUse& use) const { return !*descend || (*descend)(use); }
};

/// Calls `f(x, y, element_transform)` for every array element of `use` whose
/// bounding box overlaps `area` (parent coordinates). Stops when f returns
/// false; returns false then.
template <typename F>
bool for_each_element(const db::CellUse& use, const db::Rect& area, F&& f) {
  if (use.def == nullptr || !db::overlap(use.bbox, area)) {
    return true;
  }
  const db::ArrayInfo& a = use.array;
  for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
    for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
      const db::Transform et = db::element_transform(use, x, y);
      if (db::overlap(db::apply(et, use.def->bbox()), area) && !f(x, y, et)) {
        return false;
      }
    }
  }
  return true;
}

bool tiles_in(const db::CellDef& def, const db::Rect& area, const tech::TypeMask& types,
              const std::optional<db::TriangleSide>& triangle, const TreeTile& base, const TileFn& visit) {
  TreeTile found = base;
  found.def = &def;
  const auto report = [&](const db::TileView& t) {
    found.tile = t;
    return visit(found);
  };
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    found.plane = static_cast<tech::PlaneId>(p);
    const bool done = triangle ? def.plane(found.plane).enumerate_triangle(area, triangle->diagonal,
                                                                            triangle->right_side, types, report)
                               : def.plane(found.plane).enumerate(area, types, report);
    if (!done) {
      return false;
    }
  }
  return true;
}

bool tree_tiles(const Walk& w, const db::CellDef& def, const db::Rect& area, const tech::TypeMask& types,
                const std::optional<db::TriangleSide>& triangle, const db::Transform& to_root,
                const std::string& path, const TileFn& visit) {
  TreeTile base;
  base.to_root = to_root;
  base.path = &path;
  if (!tiles_in(def, area, types, triangle, base, visit)) {
    return false;
  }
  for (const db::CellUse* u : db::uses_in_search_order(def)) {
    if (!w.descends(*u)) {
      continue;
    }
    const bool go_on = for_each_element(*u, area, [&](std::int32_t x, std::int32_t y, const db::Transform& et) {
      const db::Transform inv = db::invert(et);  // the triangle in the child's frame
      std::optional<db::TriangleSide> child_triangle;
      if (triangle) {
        child_triangle = db::transform_triangle(inv, triangle->diagonal, triangle->right_side);
      }
      return tree_tiles(w, *u->def, to_local(et, area), types, child_triangle, db::compose(to_root, et),
                        child_path(path, *u, x, y), visit);
    });
    if (!go_on) {
      return false;
    }
  }
  return true;
}

bool tree_labels(const Walk& w, const db::CellDef& def, const db::Rect& area, const tech::TypeMask& types,
                 const db::Transform& to_root, const std::string& path, const LabelFn& visit) {
  TreeLabel found;
  found.def = &def;
  found.to_root = to_root;
  found.path = &path;
  for (const db::Label& l : def.labels()) {
    if (types.test(l.type) && db::touch(l.rect, area)) {
      found.label = &l;
      if (!visit(found)) {
        return false;
      }
    }
  }
  for (const db::CellUse* u : db::uses_in_search_order(def)) {
    if (!w.descends(*u)) {
      continue;
    }
    const db::Rect grown = db::expand(area, 1);  // labels on a subcell's edge touch the area
    const bool go_on = for_each_element(*u, grown, [&](std::int32_t x, std::int32_t y, const db::Transform& et) {
      return tree_labels(w, *u->def, to_local(et, area), types, db::compose(to_root, et),
                         child_path(path, *u, x, y), visit);
    });
    if (!go_on) {
      return false;
    }
  }
  return true;
}

bool tree_cells(const Walk& w, const db::CellDef& def, const db::Rect& area, const db::Transform& to_root,
                const std::string& path, const CellFn& visit) {
  for (const db::CellUse* u : db::uses_in_search_order(def)) {
    if (!w.descends(*u)) {
      if (u->def != nullptr && db::overlap(u->bbox, area) && !visit(TreeCell{u, to_root, &path})) {
        return false;
      }
      continue;
    }
    const bool go_on = for_each_element(*u, area, [&](std::int32_t x, std::int32_t y, const db::Transform& et) {
      return tree_cells(w, *u->def, to_local(et, area), db::compose(to_root, et),
                        child_path(path, *u, x, y), visit);
    });
    if (!go_on) {
      return false;
    }
  }
  return true;
}

bool tree_defs(const Walk& w, const db::CellDef& def, const db::Rect& area, const db::Transform& to_root,
               const std::string& path, const DefFn& visit) {
  if (!visit(def, to_root, path)) {
    return false;
  }
  for (const db::CellUse* u : db::uses_in_search_order(def)) {
    if (!w.descends(*u)) {
      continue;
    }
    const bool go_on = for_each_element(*u, area, [&](std::int32_t x, std::int32_t y, const db::Transform& et) {
      return tree_defs(w, *u->def, to_local(et, area), db::compose(to_root, et),
                       child_path(path, *u, x, y), visit);
    });
    if (!go_on) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool search_tree_defs(const db::CellDef& root, const db::Rect& area, const Descend& descend, const DefFn& visit) {
  const Walk w{&descend};
  return tree_defs(w, root, area, db::kIdentity, std::string{}, visit);
}

db::Rect to_local(const db::Transform& to_root, const db::Rect& root_area) {
  const db::Rect r = db::apply(db::invert(to_root), root_area);
  const db::Rect w = db::whole_plane();
  return db::Rect{std::max(r.x0, w.x0), std::max(r.y0, w.y0), std::min(r.x1, w.x1), std::min(r.y1, w.y1)};
}

db::Label transform_label(const db::Label& label, const db::Transform& t, bool magic_compat) {
  db::Label out = label;
  out.rect = db::apply(t, label.rect);
  out.justification = db::transform_position(t, label.justification);
  out.rotation = db::transform_angle(t, label.rotation, magic_compat);
  const db::Transform delta{t.a, t.b, 0, t.d, t.e, 0};
  out.offset = db::apply(delta, label.offset);
  if (label.text_bbox) {
    out.text_bbox = db::apply(t, *label.text_bbox);
  }
  return out;
}

std::string child_path(const std::string& parent_path, const db::CellUse& use, std::int32_t x, std::int32_t y) {
  std::string name = db::array_element_name(use, x, y);
  return parent_path.empty() ? name : parent_path + "/" + name;
}

bool search_tree_tiles(const db::CellDef& root, const db::Rect& area, const tech::TypeMask& types,
                       const Descend& descend, const TileFn& visit, std::optional<db::TriangleSide> triangle) {
  const Walk w{&descend};
  return tree_tiles(w, root, area, types, triangle, db::kIdentity, std::string{}, visit);
}

bool search_tree_labels(const db::CellDef& root, const db::Rect& area, const tech::TypeMask& types,
                        const Descend& descend, const LabelFn& visit) {
  const Walk w{&descend};
  return tree_labels(w, root, area, types, db::kIdentity, std::string{}, visit);
}

bool search_tree_cells(const db::CellDef& root, const db::Rect& area, const Descend& descend,
                       const CellFn& visit) {
  const Walk w{&descend};
  return tree_cells(w, root, area, db::kIdentity, std::string{}, visit);
}

}  // namespace hefesto::connect
