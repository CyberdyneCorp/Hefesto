// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hierarchy search and the flattening of mask generation (capability
// mask-hierarchical-generation "Flattening for interaction processing",
// "Flat region generation"; design D8).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFhier.c (cifHierCopyFunc,
// CIFCopyMaskHints, cifFlatMaskHints, cifCopyMaskHintFunc),
// database/DBcellsrch.c (DBTreeSrTiles, DBTreeSrCells, DBCellSrArea,
// DBArraySr) and database/DBcellbox.c (DBArrayOverlap), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// cifHierCopyFunc's vendor test (`if (!CIFCurStyle) if (!(… see-no-vendor)))`)
// never returns early while a style is loaded, so vendor paint is always
// copied (Q-MASK-401, replicate).
//
// Owner: GEN.

#include <algorithm>
#include <optional>
#include <string>

#include "hefesto/db/paint.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/hints/hints.hpp"
#include "mask/gen/internal.hpp"

namespace hefesto::mask::gen_detail {

const tech::TypeMask& all_but_space_and_drc() noexcept {
  static const tech::TypeMask mask = [] {
    tech::TypeMask m = tech::TypeMask::all();
    for (const tech::TileType t : {tech::kSpace, tech::kCheckPaint, tech::kCheckSubcell, tech::kErrorP,
                                   tech::kErrorS, tech::kErrorPS}) {
      m &= ~tech::TypeMask::of(t);
    }
    return m;
  }();
  return mask;
}

// --- Hierarchy search --------------------------------------------------------

namespace {

std::int32_t clamp_index(std::int64_t v) noexcept {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(v, INT32_MIN, INT32_MAX));
}

// One axis of DBArrayOverlap on the normalised (positive separation) array.
void overlap_axis(std::int64_t lo_area, std::int64_t hi_area, std::int64_t box_lo, std::int64_t box_hi,
                  std::int64_t sep, std::int32_t base, std::int32_t& lo, std::int32_t& hi) {
  if (sep == 0) {
    return;
  }
  lo = clamp_index(base + (lo_area - box_hi + sep - 1) / sep);
  hi = clamp_index(base + (hi_area - box_lo) / sep);
}

}  // namespace

IndexRange array_overlap(const db::CellUse& use, const db::Rect& parent_area) {
  const db::ArrayInfo& a = use.array;
  if (!a.is_array()) {
    return IndexRange{a.xlo, a.xlo, a.ylo, a.ylo};
  }
  db::Rect box = use.def->bbox();
  db::Rect child = db::apply(db::invert(use.transform), parent_area);
  std::int64_t xsep = a.xsep;
  std::int64_t ysep = a.ysep;
  const std::int32_t xlo = std::min(a.xlo, a.xhi);
  const std::int32_t xhi = std::max(a.xlo, a.xhi);
  const std::int32_t ylo = std::min(a.ylo, a.yhi);
  const std::int32_t yhi = std::max(a.ylo, a.yhi);
  if (xsep < 0) {  // flip about the y axis
    xsep = -xsep;
    child = db::Rect{-child.x1, child.y0, -child.x0, child.y1};
    box = db::Rect{-box.x1, box.y0, -box.x0, box.y1};
  }
  if (ysep < 0) {
    ysep = -ysep;
    child = db::Rect{child.x0, -child.y1, child.x1, -child.y0};
    box = db::Rect{box.x0, -box.y1, box.x1, -box.y0};
  }
  IndexRange out{xlo, xhi, ylo, yhi};
  overlap_axis(child.x0, child.x1, box.x0, box.x1, xsep, xlo, out.xlo, out.xhi);
  overlap_axis(child.y0, child.y1, box.y0, box.y1, ysep, ylo, out.ylo, out.yhi);
  out.xlo = std::max(out.xlo, xlo);
  out.xhi = std::min(out.xhi, xhi);
  out.ylo = std::max(out.ylo, ylo);
  out.yhi = std::min(out.yhi, yhi);
  if (a.xlo > a.xhi) {  // back to the use's own index direction
    out = IndexRange{clamp_index(std::int64_t{a.xhi} + a.xlo - out.xhi),
                     clamp_index(std::int64_t{a.xhi} + a.xlo - out.xlo), out.ylo, out.yhi};
  }
  if (a.ylo > a.yhi) {
    out = IndexRange{out.xlo, out.xhi, clamp_index(std::int64_t{a.yhi} + a.ylo - out.yhi),
                     clamp_index(std::int64_t{a.yhi} + a.ylo - out.ylo)};
  }
  return out;
}

namespace {

// dbCellSrFunc for one use; false when the search must stop.
bool search_use(const db::CellUse& use, const db::Rect& area, const ElementVisitor& visit) {
  const IndexRange r = array_overlap(use, area);
  for (std::int64_t y = r.ylo; y <= r.yhi; ++y) {
    for (std::int64_t x = r.xlo; x <= r.xhi; ++x) {
      const auto xi = static_cast<std::int32_t>(x);
      const auto yi = static_cast<std::int32_t>(y);
      const db::Transform t = db::element_transform(use, xi, yi);
      const Visit v = visit(ElementRef{&use, xi, yi, t}, db::apply(db::invert(t), area));
      if (v == Visit::stop) {
        return false;
      }
      if (v == Visit::skip_array) {
        return true;
      }
    }
  }
  return true;
}

}  // namespace

bool search_elements(const db::CellDef& def, const db::Rect& area, const ElementVisitor& visit) {
  // The visitor may search the index again (nested searches), so the uses
  // are collected first, as DRC's search does.
  std::vector<const db::CellUse*> uses;
  (void)db::uses_overlapping(def, area, [&](db::CellUse& u) {
    if (u.def != nullptr) {
      uses.push_back(&u);
    }
    return true;
  });
  return std::ranges::all_of(uses, [&](const db::CellUse* u) { return search_use(*u, area, visit); });
}

void array_elements(const db::CellUse& use, const db::Rect& parent_area,
                    const std::function<void(const ElementRef&)>& visit) {
  const IndexRange r = array_overlap(use, parent_area);
  for (std::int64_t y = r.ylo; y <= r.yhi; ++y) {
    for (std::int64_t x = r.xlo; x <= r.xhi; ++x) {
      const auto xi = static_cast<std::int32_t>(x);
      const auto yi = static_cast<std::int32_t>(y);
      visit(ElementRef{&use, xi, yi, db::element_transform(use, xi, yi)});
    }
  }
}

// --- Scratch cells -------------------------------------------------------------

std::unique_ptr<db::CellDef> make_scratch(std::string_view name, const tech::Technology& technology) {
  auto def = std::make_unique<db::CellDef>(std::string{name}, technology.plane_count());
  def->flags().internal = true;
  def->flags().available = true;
  return def;
}

void clear_scratch(db::CellDef& scratch) {
  const std::size_t planes = scratch.plane_count();
  scratch.set_plane_count(0);
  scratch.set_plane_count(planes);
  scratch.properties().clear();
  scratch.labels().clear();
}

// --- Flattening ------------------------------------------------------------------

const db::PaintRow& Flattener::row(tech::PlaneId plane, tech::TileType type) {
  const std::uint32_t key = (std::uint32_t{plane} << 16U) | type;
  auto it = rows_.find(key);
  if (it == rows_.end()) {
    it = rows_.emplace(key, db::paint_row(technology_, plane, type)).first;
  }
  return it->second;
}

// cifHierCopyFunc: the whole tile, transformed, painted on every plane its
// type paints (a split tile's side as a triangle with its diagonal
// transformed).
void Flattener::copy_tile(const db::TileView& tile, const db::Transform& t, db::CellDef& target) {
  if (tile.type == tech::kSpace) {
    return;
  }
  const db::Rect r = db::apply(t, tile.area);
  const tech::PlaneMask planes = technology_.paint_planes(tile.type);
  const std::optional<db::TriangleSide> side =
      tile.split ? std::optional{db::transform_triangle(t, tile.diagonal, tile.right_side)} : std::nullopt;
  for (std::size_t p = tech::kPlaneMHint; p < target.plane_count(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    if ((planes & tech::plane_bit(id)) == 0) {
      continue;
    }
    db::Plane& plane = target.plane(id);
    if (side) {
      (void)plane.paint_triangle(r, side->diagonal, side->right_side, row(id, tile.type), magic_compat_);
    } else {
      (void)plane.paint(r, row(id, tile.type));
    }
  }
}

bool Flattener::copy_cell(const db::CellDef& def, const db::Rect& area, const db::Transform& t,
                          const tech::TypeMask& types, db::CellDef& target, int depth) {
  if (depth > kMaxDepth) {
    return false;
  }
  for (std::size_t p = tech::kPlaneMHint; p < def.plane_count(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    if (!technology_.plane_types(id).intersects(types)) {
      continue;
    }
    std::vector<db::TileView> tiles;
    def.plane(id).enumerate(area, types, [&](const db::TileView& tile) {
      tiles.push_back(tile);
      return true;
    });
    for (const db::TileView& tile : tiles) {
      copy_tile(tile, t, target);
    }
  }
  bool complete = true;
  (void)search_elements(def, area, [&](const ElementRef& e, const db::Rect& child_area) {
    complete = copy_cell(*e.use->def, child_area, db::compose(t, e.to_parent), types, target, depth + 1) && complete;
    return Visit::next;
  });
  return complete;
}

bool Flattener::copy_paint(const db::CellDef& root, const db::Rect& area, const db::Transform& to_target,
                           const tech::TypeMask& types, db::CellDef& target) {
  return copy_cell(root, area, to_target, types, target, 0);
}

namespace {

void collect_child_hints(hints::HintBatch& batch, const db::CellDef& def, const db::Rect& area,
                         const db::Transform& t, int depth) {
  if (depth > kMaxDepth) {
    return;
  }
  (void)search_elements(def, area, [&](const ElementRef& e, const db::Rect& child_area) {
    const db::Transform to_target = db::compose(t, e.to_parent);
    batch.add(*e.use->def, to_target, std::nullopt);
    collect_child_hints(batch, *e.use->def, child_area, to_target, depth + 1);
    return Visit::next;
  });
}

}  // namespace

void Flattener::copy_hints(const db::CellDef& root, const db::Rect& area, const db::Transform& to_target,
                           db::CellDef& target) {
  hints::HintBatch batch;
  batch.add(root, to_target, std::nullopt);
  collect_child_hints(batch, root, area, to_target, 0);
  batch.commit(target);
}

// --- Errors ------------------------------------------------------------------------

void report_error(const OutputStyle& style, const db::CellDef* cell, std::size_t layer, const db::Rect& mask_area,
                  std::string_view code, std::string_view message, GenerationContext& ctx) {
  if (ctx.sink == nullptr || cell == nullptr || style.has_option(style_option::no_errors)) {
    return;
  }
  const std::string name = layer < style.layers.size() ? style.layers[layer].name : std::string{};
  const std::int32_t s = scale_of(style);
  diag::Diagnostic d =
      diag::make(code, "CIF error in cell " + cell->name() + ", layer " + name + ": " + std::string{message},
                 diag::LayoutLocation{cell->name(), mask_area.x0 / s, mask_area.y0 / s});
  d.native_only = true;  // Magic adds feedback; it prints nothing
  ctx.sink->emit(d);
}

}  // namespace hefesto::mask::gen_detail
