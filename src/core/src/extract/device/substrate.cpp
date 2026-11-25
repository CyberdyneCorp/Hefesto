// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The substrate: the tiles of the global substrate node, its name, and the
// isolated substrate plane. Capability extract-substrate "Backward-
// compatible mode without a substrate declaration", "Isolated substrate
// regions", "Global substrate node discovery", "Substrate node naming"
// (Q-EXTRACT-9); openspec/changes/m7-extraction/design.md D10.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c:880-925
// (extSubsName), :5057-5300 (extFindNodes' first pass, extSubsFunc,
// extSubsFunc2), extract/ExtCell.c:288-335 (extPrepSubstrate) and
// database/DBcellcopy.c:842-1160 (DBCellGenerateSubstrate), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "device_internal.hpp"
#include "hefesto/db/transform.hpp"

namespace hefesto::extract {

namespace {

bool enumerate_under(const db::Plane& plane, const db::TileView& tile, const tech::TypeMask& mask,
                     const std::function<bool(const db::TileView&)>& visit) {
  if (tile.split) {
    return plane.enumerate_triangle(tile.area, tile.diagonal, tile.right_side, mask, visit);
  }
  return plane.enumerate(tile.area, mask, visit);
}

// Some type of `mask` under `tile` on any plane that has one.
bool covered(const db::CellDef& cell, const tech::Technology& technology, const db::TileView& tile,
             const tech::TypeMask& mask) {
  if (mask.empty()) {
    return false;
  }
  for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    if (technology.plane_types(pnum).intersects(mask) &&
        !enumerate_under(cell.plane(pnum), tile, mask, [](const db::TileView&) { return false; })) {
      return true;
    }
  }
  return false;
}

// Anything but space on the substrate plane under `area` (extSubsFunc3).
bool not_space_under(const db::Plane& plane, const db::Rect& area) {
  tech::TypeMask all = tech::TypeMask::all();
  all.clear_type(tech::kSpace);
  return !plane.enumerate(area, all, [](const db::TileView&) { return false; });
}

// extSubsFunc / extSubsFunc2: a substrate-type tile not covered by a
// shield type and, off the substrate plane when space is substrate, over
// nothing but space there.
bool substrate_tile(const db::CellDef& cell, const tech::Technology& technology, const SubstrateDecl& sub,
                    const db::TileView& t, bool below_space) {
  if (t.split && t.type == tech::kSpace) {
    return false;
  }
  if (covered(cell, technology, t, sub.shield_types)) {
    return false;
  }
  return !below_space || !not_space_under(cell.plane(*sub.plane), t.area);
}

}  // namespace

SubstrateSeeds substrate_seeds(const db::CellDef& cell, const tech::Technology& technology, const ExtStyle& style,
                               bool full_rc) {
  SubstrateSeeds out;
  const SubstrateDecl& sub = style.substrate;
  const bool space_is_substrate = sub.types.test(tech::kSpace);
  tech::TypeMask nonspace = sub.types;
  nonspace.clear_type(tech::kSpace);
  if (!full_rc && sub.default_type) {
    nonspace.clear_type(*sub.default_type);  // the default type marks isolated regions only
  }
  const auto collect = [&](tech::PlaneId pnum, bool below_space) {
    (void)cell.plane(pnum).enumerate(db::whole_plane(), nonspace, [&](const db::TileView& t) {
      if (substrate_tile(cell, technology, sub, t, below_space)) {
        out.tiles.emplace_back(pnum, t);
      }
      return true;
    });
  };
  if (sub.plane && technology.plane_types(*sub.plane).intersects(nonspace)) {
    collect(*sub.plane, false);
  }
  for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    if ((!sub.plane || pnum != *sub.plane) && technology.plane_types(pnum).intersects(nonspace)) {
      collect(pnum, space_is_substrate && sub.plane.has_value());
    }
  }
  if (out.tiles.empty()) {
    out.tileless = sub.plane.has_value();
    out.none = !sub.plane.has_value();
  }
  return out;
}

std::string substrate_name(std::string_view declared,
                           const std::function<std::optional<std::string>(std::string_view)>& lookup) {
  if (declared.size() > 1 && declared[0] == '$' && declared[1] != '$') {
    const std::string_view var = declared.substr(1);
    if (lookup) {
      if (auto value = lookup(var)) {
        return *value;
      }
    }
    return std::string(var);  // undefined: the name without its `$`
  }
  return std::string(declared);
}

diag::Outcome<SubstrateNodes> prepare_substrate(const db::CellDef& /*cell*/, const ExtStyle& style,
                                               const app::GlobalNets& nets) {
  SubstrateNodes out;
  const auto lookup = [&nets](std::string_view var) -> std::optional<std::string> {
    if (var == "SUB") {
      return nets.sub;
    }
    if (var == "VDD") {
      return nets.vdd;
    }
    if (var == "GND") {
      return nets.gnd;
    }
    return std::nullopt;
  };
  if (!style.substrate.node_name.empty()) {
    out.global_name = substrate_name(style.substrate.node_name, lookup);
  }
  return out;
}

bool writes_substrate_record(const db::CellDef& cell) {
  return cell.properties().find("LEFview") == nullptr;
}

CouplingTable relocate_substrate_coupling(const CouplingTable& table, std::size_t substrate, NodeRegionList& nodes) {
  CouplingTable rest;
  for (const auto& [pair, cap] : table.pairs()) {
    const auto [a, b] = pair;
    if (cap != 0.0 && (a == substrate || b == substrate)) {
      const std::size_t other = a == substrate ? b : a;
      if (other < nodes.size()) {
        nodes[other].substrate_cap += cap;
      }
      continue;
    }
    rest.add(a, b, cap);
  }
  return rest;
}

// --- extPrepSubstrate / DBCellGenerateSubstrate ---

namespace {

constexpr int kMaxDepth = 256;  // a guard against cyclic hierarchies

// The tiles of `mask` of one cell on every plane, in root coordinates,
// overlapping `area`.
void own_tiles(const db::CellDef& cell, const db::Transform& t, const db::Rect& area, const tech::TypeMask& mask,
               const std::function<void(const db::Rect&)>& fn) {
  for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
    (void)cell.plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), mask, [&](const db::TileView& tile) {
      if (tile.split && tile.type == tech::kSpace) {
        return true;
      }
      const db::Rect r = db::apply(t, tile.area);
      if (db::overlap(r, area)) {
        fn(r);
      }
      return true;
    });
  }
}

// DBTreeSrTiles: the cell's tiles and its subcells' (every array element).
void tree_tiles(const db::CellDef& cell, const db::Transform& t, const db::Rect& area, const tech::TypeMask& mask,
                const std::function<void(const db::Rect&)>& fn, int depth) {
  if (depth > kMaxDepth) {
    return;
  }
  own_tiles(cell, t, area, mask, fn);
  for (const auto& use : cell.uses()) {
    if (use == nullptr || use->def == nullptr) {
      continue;
    }
    const db::ArrayInfo& a = use->array;
    for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
      for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
        const db::Transform elem = db::compose(db::translation((x - a.xlo) * a.xsep, (y - a.ylo) * a.ysep), use->transform);
        tree_tiles(*use->def, db::compose(t, elem), area, mask, fn, depth + 1);
      }
    }
  }
}

db::PaintRow row_to(tech::TileType to) {
  db::PaintRow row{};
  row.fill(to);
  return row;
}

db::PaintRow erase_row(tech::TileType erased) {
  db::PaintRow row{};
  for (std::size_t i = 0; i < row.size(); ++i) {
    row[i] = static_cast<tech::TileType>(i);
  }
  row[erased] = tech::kSpace;
  return row;
}

}  // namespace

std::optional<db::Plane> isolated_substrate_plane(const db::CellDef& cell, const tech::Technology& technology,
                                                  const ExtStyle& style) {
  const SubstrateDecl& sub = style.substrate;
  if (!sub.plane || !sub.default_type) {
    return std::nullopt;
  }
  const tech::TileType sub_type = *sub.default_type;
  const tech::TypeMask not_sub = technology.plane_types(*sub.plane) & ~sub.types;
  const db::Rect area = cell.bbox();
  db::Plane temp;
  bool modified = false;
  const db::Transform id{};
  const auto paint = [&](const db::PaintRow& row, bool mark) {
    return [&temp, &modified, row, mark](const db::Rect& r) {
      modified = modified || mark;
      (void)temp.paint(r, row, db::PaintMerge::none);
    };
  };
  // The default type now marks isolated substrate only: erase it, paint it
  // under the shields, erase it from the non-substrate types.
  tree_tiles(cell, id, area, tech::TypeMask::of(sub_type), paint(erase_row(sub_type), true), 0);
  tree_tiles(cell, id, area, sub.shield_types, paint(row_to(sub_type), true), 0);
  if (!modified) {
    return std::nullopt;
  }
  tree_tiles(cell, id, area, not_sub, paint(erase_row(sub_type), false), 0);
  // Then the cell's own paint of the plane, except the default type.
  tech::TypeMask others = tech::TypeMask::all();
  others.clear_type(tech::kSpace);
  others.clear_type(sub_type);
  (void)cell.plane(*sub.plane).enumerate(db::whole_plane(), others, [&](const db::TileView& t) {
    if (!t.split || t.type != tech::kSpace) {
      (void)temp.paint(t.area, row_to(t.type), db::PaintMerge::none);
    }
    return true;
  });
  return temp;
}

}  // namespace hefesto::extract
