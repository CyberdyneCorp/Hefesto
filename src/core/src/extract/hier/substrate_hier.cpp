// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Subcell substrate connections (extract-substrate "Subcell substrate
// connections"). Ported from Magic 8.3.694 (fc5daa9e) extract/ExtHier.c
// (extHierSubstrate, extHierSubShieldFunc, extHierSubInteractFunc) and
// extract/ExtSubtree.c (extSubstrateFunc and the end of extSubtreeFunc);
// Magic is Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).
//
// The element loops copy Magic's: an array whose indices run downwards
// (xlo > xhi or ylo > yhi) visits no element, so its substrate is not
// connected.
//
// Owner: EXTHIER.

#include <string>

#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hier_context.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract::hier {

namespace {

constexpr const char* kNone = "(none)";

// DBTreeSrTiles over `def` and its subtree: a tile of `types` under `area`.
bool subtree_has(const db::CellDef& def, const db::Rect& area, const tech::TypeMask& types) {
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    const bool stopped = !def.plane(static_cast<tech::PlaneId>(p)).enumerate(area, types, [](const db::TileView&) {
      return false;
    });
    if (stopped) {
      return true;
    }
  }
  for (const db::CellUse* u : uses_in(def, area)) {
    const bool found = array_elements(*u, area, [&](const Element& e) {
      return subtree_has(*u->def, db::apply(db::invert(e.to_parent), area), types);
    });
    if (found) {
      return true;
    }
  }
  return false;
}

// extHierSubShieldFunc over the parent's shield paint under `sub_area`.
bool shielded(const Hier& h, const db::CellUse& use, const db::Rect& sub_area) {
  const tech::TypeMask& shields = h.env.style->substrate.shield_types;
  const db::Transform to_child = db::invert(use.transform);
  for (std::size_t p = tech::kFirstUserPlane; p < h.parent->plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    if (!h.env.tech->plane_types(pnum).intersects(shields)) {
      continue;
    }
    const bool stopped = !h.parent->plane(pnum).enumerate(sub_area, shields, [&](const db::TileView& v) {
      return !subtree_has(*use.def, db::apply(to_child, v.area), h.env.style->substrate_device_types);
    });
    if (stopped) {
      return true;
    }
  }
  return false;
}

std::string child_name(const db::CellUse& use, std::int32_t x, std::int32_t y, const std::string& name) {
  if (x >= 0 && y >= 0) {
    return use.id + "[" + std::to_string(y) + "," + std::to_string(x) + "]/" + name;
  }
  if (x >= 0 || y >= 0) {
    return use.id + "[" + std::to_string(x >= 0 ? x : y) + "]/" + name;
  }
  return use.id + "/" + name;
}

NodePassOptions substrate_pass(const Hier& h, const db::CellDef& def) {
  NodePassOptions o;
  o.substrate_only = true;
  o.resistance = h.env.options->lumped;
  o.magic_compat = h.env.compat->magic_compat;
  const SubstrateSeeds seeds = substrate_seeds(def, *h.env.tech, *h.env.style, h.env.options->extresist);
  for (const auto& [plane, tile] : seeds.tiles) {
    o.substrate_seeds.push_back(RegionSeed{plane, {tile.area.x0, tile.area.y0}, tile.right_side});
  }
  if (seeds.tiles.empty() && seeds.tileless) {
    o.substrate_without_tiles = h.env.style->substrate.plane;
  }
  return o;
}

// The child's substrate node name, or nullopt when it has none or is
// shielded (the caller's 1).
std::optional<std::string> child_substrate(const Hier& h, const db::CellUse& use, bool& none) {
  const db::CellDef& def = *use.def;
  RegionFinder finder(*h.env.tech);
  finder.keep_clients = true;
  auto found = finder.find_nodes(def, *h.env.style, substrate_pass(h, def));
  if (!found.ok() || found.value().empty()) {
    reset(def);
    none = true;
    return std::nullopt;
  }
  NodeRegionList nodes = std::move(found).value();
  const db::Point ref = nodes.front().reference;
  const db::Rect sub_area =
      db::encloses(def.bbox(), ref)
          ? db::Rect{db::apply(use.transform, ref).x, db::apply(use.transform, ref).y,
                     db::apply(use.transform, ref).x + 1, db::apply(use.transform, ref).y + 1}
          : db::apply(use.transform, def.bbox());
  if (shielded(h, use, sub_area)) {
    reset(def);
    return std::nullopt;
  }
  attach_labels(def, finder, nodes, *h.env.style, *h.env.tech, LabelPassOptions{}, *h.env.compat);
  reset(def);
  return flat_node_name(nodes.back(), h);
}

}  // namespace

bool connect_substrate(Hier& h, const db::CellUse& use, std::int32_t x, std::int32_t y) {
  const ParentNodes& parent = h.inputs->parent;
  if (!parent.substrate || *parent.substrate >= parent.names.size() || h.sub_extracted.contains(&use)) {
    return true;
  }
  if (h.inputs->dont_use && h.inputs->dont_use(*use.def)) {
    return true;
  }
  const std::string& name1 = parent.names[*parent.substrate];
  if (name1 == kNone) {
    return true;
  }
  const std::size_t node1 = h.table.node_of(name1);
  bool none = false;
  const std::optional<std::string> name2 = child_substrate(h, use, none);
  if (!name2) {
    return none;
  }
  const std::size_t node2 = h.table.node_of(child_name(use, x, y, *name2));
  (void)h.table.unify(node1, node2, !h.env.compat->magic_compat);
  return true;
}

void connect_use_substrate(Hier& h, const db::CellUse& use, bool mark_always) {
  const db::ArrayInfo& a = use.array;
  bool ok = false;
  if (a.xhi == a.xlo && a.yhi == a.ylo) {
    ok = connect_substrate(h, use, -1, -1);
  } else if (a.xhi == a.xlo && a.yhi > a.ylo) {
    for (std::int32_t y = a.ylo; y <= a.yhi; ++y) {
      ok = connect_substrate(h, use, -1, y) || ok;
    }
  } else if (a.xhi > a.xlo && a.yhi == a.ylo) {
    for (std::int32_t x = a.xlo; x <= a.xhi; ++x) {
      ok = connect_substrate(h, use, x, -1) || ok;
    }
  } else {
    for (std::int32_t x = a.xlo; x <= a.xhi; ++x) {
      for (std::int32_t y = a.ylo; y <= a.yhi; ++y) {
        ok = connect_substrate(h, use, x, y) || ok;
      }
    }
  }
  if (ok || mark_always) {
    h.sub_extracted.insert(&use);
  }
}

}  // namespace hefesto::extract::hier
