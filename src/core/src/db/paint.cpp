// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell-level paint operations (layout-paint-semantics "Paint operation",
// "Active (locked) layers", "Copying paint between cells").
// Ported from Magic VLSI 8.3.694 (fc5daa9e): database/DBpaint2.c (DBPaint,
// dbResolveImages, DBErase, DBPaintValid, DBEraseValid) and
// database/DBcellcopy.c (dbCopyAllPaint, DBPaintPlaneWrapper). Magic is
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).

#include "hefesto/db/paint.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "db/tech_masks.hpp"
#include "hefesto/db/change_recorder.hpp"
#include "hefesto/db/diag_codes.hpp"

namespace hefesto::db {

namespace {

diag::Status check_area(const Rect& area) {
  if (area.is_null() || in_usable_plane(area)) {
    return diag::Done{};
  }
  return diag::make(codes::kGeometryOutsidePlane,
                    "rectangle (" + std::to_string(area.x0) + "," + std::to_string(area.y0) + ")-(" +
                        std::to_string(area.x1) + "," + std::to_string(area.y1) +
                        ") lies outside the usable plane (-1073741818..1073741818)");
}

bool on_plane(tech::PlaneMask mask, std::size_t p) noexcept {
  return (mask & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0;
}

Rect clip(const Rect& a, const Rect& b) noexcept {
  return Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

// The change of one enumerated tile (or split side) whose type `row`
// changes: a Manhattan tile clipped to `area`; a split side as the whole
// triangle (painting its old type back over it restores it exactly, as
// nothing outside `area` changed). `paint_side` is the triangle being
// painted, if any: a Manhattan piece under it changes on that side only.
TileChange tile_change(tech::PlaneId id, const TileView& t, const Rect& area, const PaintRow& row,
                       const std::optional<TriangleSpec>& paint_side) {
  TileChange c{id, clip(t.area, area), t.type, row[t.type], std::nullopt};
  if (t.split) {
    c.area = t.area;
    c.triangle = TriangleSide{t.diagonal, t.right_side};
  } else if (paint_side) {
    c.triangle = TriangleSide{paint_side->diagonal, paint_side->right_side};
  }
  return c;
}

// Reports to the cell's recorder each tile (or split side) under `area`
// (or under its triangle) whose type `row` changes (Magic's DBPaintPlane /
// DBNMPaintPlane undo records; layout-undo "Recorded event kinds").
void record_tiles(CellDef& def, tech::PlaneId id, const Rect& area, const PaintRow& row,
                  const std::optional<TriangleSpec>& tri) {
  ChangeRecorder* recorder = def.recorder();
  if (recorder == nullptr || area.is_null()) {
    return;
  }
  std::vector<TileChange> changes;
  const auto collect = [&](const TileView& t) {
    if (row[t.type] != t.type) {
      changes.push_back(tile_change(id, t, area, row, tri));
    }
    return true;
  };
  const Plane& plane = def.plane(id);
  if (tri) {
    plane.enumerate_triangle(area, tri->diagonal, tri->right_side, tech::TypeMask::all(), collect);
  } else {
    plane.enumerate(area, tech::TypeMask::all(), collect);
  }
  for (const TileChange& change : changes) {
    recorder->tile_changed(def, change);
  }
}

diag::Status apply_row(CellDef& def, tech::PlaneId id, const Rect& area, const PaintRow& row,
                       const std::optional<TriangleSpec>& tri) {
  if (in_usable_plane(area)) {
    record_tiles(def, id, area, row, tri);
  }
  Plane& plane = def.plane(id);
  if (tri) {
    return plane.paint_triangle(area, tri->diagonal, tri->right_side, row);
  }
  return plane.paint(area, row);
}

struct Image {
  Rect area;
  std::optional<TriangleSpec> triangle;
};

// The areas where `itype` is present on `plane` within the operand area
// (DBSrPaintNMArea with dbResolveImages collecting instead of painting).
std::vector<Image> find_images(const Plane& plane, const Rect& area, const std::optional<TriangleSpec>& tri,
                               tech::TileType itype) {
  std::vector<Image> found;
  const auto collect = [&](const TileView& t) {
    std::optional<TriangleSpec> side;
    if (t.split) {
      side = TriangleSpec{t.diagonal, t.right_side};
    }
    found.push_back(Image{t.area, side});
    return true;
  };
  const tech::TypeMask mask = tech::TypeMask::of(itype);
  if (tri) {
    plane.enumerate_triangle(area, tri->diagonal, tri->right_side, mask, collect);
  } else {
    plane.enumerate(area, mask, collect);
  }
  return found;
}

// Repaints the images of contact `itype` found on every plane it paints.
diag::Status repaint_images(CellDef& def, const tech::Technology& tech, const Rect& area,
                            const std::optional<TriangleSpec>& tri, tech::TileType itype) {
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    if (!on_plane(tech.paint_planes(itype), p)) {
      continue;
    }
    for (const Image& img : find_images(def.plane(static_cast<tech::PlaneId>(p)), area, tri, itype)) {
      if (auto s = paint_type(def, tech, img.area, itype, img.triangle); !s.ok()) {
        return s;
      }
    }
  }
  return diag::Done{};
}

// dbResolveImages: repaints every user contact image that has `type` as a
// residue over the areas where it is present on any of its planes.
diag::Status resolve_images(CellDef& def, const tech::Technology& tech, const Rect& area,
                            const std::optional<TriangleSpec>& tri, tech::TileType type) {
  const TechMasks masks(tech, def.plane_count());
  for (std::size_t i = tech::kMagnet; i < tech.user_type_end(); ++i) {
    const auto itype = static_cast<tech::TileType>(i);
    if (itype == type || !masks.residues(itype).test(type)) {
      continue;
    }
    if (auto s = repaint_images(def, tech, area, tri, itype); !s.ok()) {
      return s;
    }
  }
  return diag::Done{};
}

tech::TypeMask active_layers(const PaintOptions& options) {
  return all_but_space_and_drc() & ~options.locked;
}

using TypeOp = diag::Status (*)(CellDef&, const tech::Technology&, const Rect&, tech::TileType,
                                std::optional<TriangleSpec>);

// Applies `op` with each type of `types`, stopping at the first failure.
diag::Status apply_each(CellDef& def, const tech::Technology& tech, const Rect& area, const tech::TypeMask& types,
                        const std::optional<TriangleSpec>& tri, TypeOp op) {
  for (std::size_t r = tech::kMagnet; r < tech.user_type_end(); ++r) {
    const auto t = static_cast<tech::TileType>(r);
    if (!types.test(t)) {
      continue;
    }
    if (auto s = op(def, tech, area, t, tri); !s.ok()) {
      return s;
    }
  }
  return diag::Done{};
}

// One type of DBPaintValid/DBEraseValid: a contact whose residues are not all
// active is replaced by its active residues.
diag::Status apply_valid_type(CellDef& def, const tech::Technology& tech, const TechMasks& masks, const Rect& area,
                              tech::TileType t, const tech::TypeMask& active, const std::optional<TriangleSpec>& tri,
                              TypeOp op) {
  if (!tech.is_contact(t)) {
    return op(def, tech, area, t, tri);
  }
  const tech::TypeMask residues = masks.residues(t);
  const tech::TypeMask rmask = residues & active;
  if (rmask == residues) {
    return op(def, tech, area, t, tri);
  }
  return apply_each(def, tech, area, rmask, tri, op);
}

// The contact handling shared by DBPaintValid and DBEraseValid.
diag::Status apply_valid(CellDef& def, const tech::Technology& tech, const Rect& area, const tech::TypeMask& mmask,
                         const tech::TypeMask& active, const std::optional<TriangleSpec>& tri, TypeOp op) {
  const TechMasks masks(tech, def.plane_count());
  for (std::size_t i = tech::kMagnet; i < tech.user_type_end(); ++i) {
    const auto t = static_cast<tech::TileType>(i);
    if (!mmask.test(t)) {
      continue;
    }
    if (auto s = apply_valid_type(def, tech, masks, area, t, active, tri, op); !s.ok()) {
      return s;
    }
  }
  return diag::Done{};
}

// Decomposes the stacked types of `mask` into their residues.
tech::TypeMask decompose_stacked(const tech::Technology& tech, const tech::TypeMask& mask, std::size_t planes) {
  const TechMasks masks(tech, planes);
  tech::TypeMask out = mask;
  for (std::size_t t = tech.user_type_end(); t < tech.type_count(); ++t) {
    if (mask.test(static_cast<tech::TileType>(t))) {
      out |= masks.residues(static_cast<tech::TileType>(t));
    }
  }
  return out;
}

diag::Status paint_valid(CellDef& def, const tech::Technology& tech, const Rect& area, tech::TileType type,
                         const PaintOptions& options, const std::optional<TriangleSpec>& tri) {
  tech::TypeMask mmask = decompose_stacked(tech, tech::TypeMask::of(type), def.plane_count());
  const tech::TypeMask active = active_layers(options);
  mmask &= active;
  return apply_valid(def, tech, area, mmask, active, tri, &paint_type);
}

// dbCopyAllPaint: the type painted for a tile of `type` on plane `p`, or
// space when nothing is copied.
tech::TileType copy_type(const tech::Technology& tech, const TechMasks& masks, tech::TileType type,
                         tech::PlaneId p, const tech::TypeMask& types) {
  if (types.test(type)) {
    return type;
  }
  const tech::TypeMask rmask = types & masks.residues(type) & masks.plane_types(p);
  if (!rmask.empty()) {
    for (std::size_t t = tech::kFirstUserType; t < tech.user_type_end(); ++t) {
      if (rmask.test(static_cast<tech::TileType>(t))) {
        return static_cast<tech::TileType>(t);
      }
    }
    return tech::kSpace;
  }
  const tech::TileType residue = tech.residue_on(type, p);
  return types.test(residue) ? residue : tech::kSpace;
}

}  // namespace

PaintRow paint_row(const tech::Technology& tech, tech::PlaneId plane, tech::TileType type) {
  PaintRow row{};
  for (std::size_t have = 0; have < row.size(); ++have) {
    row[have] = tech.paint_result(plane, static_cast<tech::TileType>(have), type);
  }
  return row;
}

PaintRow erase_row(const tech::Technology& tech, tech::PlaneId plane, tech::TileType type) {
  PaintRow row{};
  for (std::size_t have = 0; have < row.size(); ++have) {
    row[have] = tech.erase_result(plane, static_cast<tech::TileType>(have), type);
  }
  return row;
}

diag::Status paint_type(CellDef& def, const tech::Technology& tech, const Rect& area, tech::TileType type,
                        std::optional<TriangleSpec> triangle) {
  if (auto s = check_area(area); !s.ok()) {
    return s;
  }
  mark_modified(def);
  const tech::PlaneMask planes = tech.paint_planes(type);
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    if (on_plane(planes, p)) {
      const auto id = static_cast<tech::PlaneId>(p);
      if (auto s = apply_row(def, id, area, paint_row(tech, id, type), triangle); !s.ok()) {
        return s;
      }
    }
  }
  if (type < tech.user_type_end()) {
    return resolve_images(def, tech, area, triangle, type);
  }
  return diag::Done{};
}

diag::Status erase_type(CellDef& def, const tech::Technology& tech, const Rect& area, tech::TileType type,
                        std::optional<TriangleSpec> triangle) {
  if (auto s = check_area(area); !s.ok()) {
    return s;
  }
  mark_modified(def);
  const bool space = type == tech::kSpace;
  const tech::PlaneMask planes = tech.erase_planes(type);
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    if (space || on_plane(planes, p)) {
      const auto id = static_cast<tech::PlaneId>(p);
      const PaintRow row = space ? paint_row(tech, id, type) : erase_row(tech, id, type);
      if (auto s = apply_row(def, id, area, row, triangle); !s.ok()) {
        return s;
      }
    }
  }
  return diag::Done{};
}

diag::Status paint(CellDef& def, const tech::Technology& tech, const Rect& area, tech::TileType type,
                   const PaintOptions& options) {
  if (auto s = check_area(area); !s.ok()) {
    return s;
  }
  mark_modified(def);
  return paint_valid(def, tech, area, type, options, std::nullopt);
}

diag::Status erase(CellDef& def, const tech::Technology& tech, const Rect& area, const tech::TypeMask& types,
                   const PaintOptions& options) {
  if (auto s = check_area(area); !s.ok()) {
    return s;
  }
  mark_modified(def);
  tech::TypeMask mask = types;
  if (!options.magic_compat) {
    mask = decompose_stacked(tech, mask, def.plane_count());  // Q-MODEL-22
    if (mask.test(tech::kSpace)) {
      mask |= all_but_space_and_drc();  // Q-MODEL-29
    }
  }
  const tech::TypeMask active = active_layers(options);
  return apply_valid(def, tech, area, mask & active, active, std::nullopt, &erase_type);
}

diag::Status paint_triangle(CellDef& def, const tech::Technology& tech, const Rect& area, Diagonal diagonal,
                            bool right_side, tech::TileType type, const PaintOptions& options) {
  if (auto s = check_area(area); !s.ok()) {
    return s;
  }
  mark_modified(def);
  return paint_valid(def, tech, area, type, options, TriangleSpec{diagonal, right_side});
}

namespace {

// One copied tile, transformed into the target and clipped to `target_area`
// (a split tile is copied only when it lies wholly inside: clipped triangles
// are not supported, as documented).
diag::Status copy_tile(CellDef& target, const tech::Technology& tech, tech::PlaneId id, const TileView& tile,
                       tech::TileType type, const Transform& transform, const Rect& target_area) {
  const Rect r = apply(transform, tile.area);
  std::optional<TriangleSpec> tri;
  Rect clipped = r;
  if (tile.split) {
    if (!surround(target_area, r)) {
      return diag::Done{};
    }
    const TriangleSide side = transform_triangle(transform, tile.diagonal, tile.right_side);
    tri = TriangleSpec{side.diagonal, side.right_side};
  } else {
    clipped = Rect{std::max(r.x0, target_area.x0), std::max(r.y0, target_area.y0), std::min(r.x1, target_area.x1),
                   std::min(r.y1, target_area.y1)};
  }
  return apply_row(target, id, clipped, paint_row(tech, id, type), tri);
}

}  // namespace

diag::Status copy_paint(const CellDef& source, const Rect& area, const Transform& transform, CellDef& target,
                        const tech::Technology& tech, const tech::TypeMask& types) {
  const Rect target_area = apply(transform, area);
  if (auto s = check_area(target_area); !s.ok()) {
    return s;
  }
  mark_modified(target);
  const TechMasks masks(tech, source.plane_count());
  const tech::TypeMask paint_types = ~tech::TypeMask::of(tech::kSpace);
  for (std::size_t p = 1; p < source.plane_count() && p < target.plane_count(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    std::vector<std::pair<TileView, tech::TileType>> tiles;
    source.plane(id).enumerate(area, paint_types, [&](const TileView& t) {
      const tech::TileType type = copy_type(tech, masks, t.type, id, types);
      if (type != tech::kSpace) {
        tiles.emplace_back(t, type);
      }
      return true;
    });
    for (const auto& [tile, type] : tiles) {
      if (auto s = copy_tile(target, tech, id, tile, type, transform, target_area); !s.ok()) {
        return s;
      }
    }
  }
  return diag::Done{};
}

}  // namespace hefesto::db
