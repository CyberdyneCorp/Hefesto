// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchical generation (capability mask-hierarchical-generation
// "Per-cell output composition", "Own-paint generation area", "Non-local
// layers", "Interaction search grid", "Interaction area discovery",
// "Interaction area correction", "Hierarchy-stopping operators",
// "Parent/child disagreement reporting", "Flat CIF output"; design D8):
// the own-paint layers of a cell plus CIFGenSubcells' corrections.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFhier.c (CIFGenSubcells,
// cifGenHierArea, cifHierCellFunc, cifCheckAndErase, cifHierCheckFunc,
// cifHierTempCheckFunc, cifHierErrorFunc, cifHierPaintFunc,
// cifSubtreeHasTypes), cif/CIFwrite.c (cifOut's generation area,
// CIFWriteFlat) and calma/CalmaWrite.c (the FIXED_BBOX enlargement),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// The interaction areas come from the finder shared with hierarchical DRC
// (GenerationSource::find_interaction, drc::interaction_area), which reads
// Q-MASK-400 itself. Squares that no use can reach are skipped without a
// search (the finder returns none there): the grid is walked only where a
// use's bounding box grown by twice the radius lies, from the cell's use
// list, which never bins the use index.
//
// Compat splits: Q-MASK-402 OFF gives a layer that stopped hierarchically no
// residual and no disagreement report; Q-MASK-409 is in sliver.cpp;
// Q-MASK-411 OFF copies labels and hints into the flat CIF cell.
//
// Owner: GEN.

#include <algorithm>
#include <unordered_map>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/gen/classify.hpp"
#include "hefesto/mask/gen/throttle.hpp"
#include "mask/gen/hier_internal.hpp"

namespace hefesto::mask {

using gen_detail::clip;
using gen_detail::grow;

namespace gen_detail {

namespace {

void add_hier_tile(GenerationContext& ctx) {
  ++ctx.statistics.flat_tiles;  // CIFTileOps
}

// cifHierErrorFunc for the space tiles of `total` under a component tile.
void report_space(HierPass& pass, std::size_t layer, const MaskPlane& total, const db::TileView& comp) {
  const tech::TypeMask space = tech::TypeMask::of(tech::kSpace);
  std::vector<db::Rect> errors;
  const auto visit = [&](const db::TileView& t) {
    const bool exempt = t.split && ((t.area.x0 == comp.area.x0 && !t.right_side) ||
                                    (t.area.x1 == comp.area.x1 && t.right_side));
    if (!exempt) {
      errors.push_back(clip(t.area, comp.area));
    }
    return true;
  };
  if (comp.split) {
    total.plane().enumerate_triangle(comp.area, comp.diagonal, comp.right_side, space, visit);
  } else {
    total.plane().enumerate(comp.area, space, visit);
  }
  for (const db::Rect& e : errors) {
    report_error(pass.style, pass.error_cell, layer, e, kDisagree, "parent and child disagree on CIF", pass.ctx);
  }
}

void erase_tile(MaskPlane& total, const db::TileView& t) {
  if (t.split) {
    (void)total.erase_triangle(t.area, t.diagonal, t.right_side);
  } else {
    (void)total.erase(t.area);
  }
}

}  // namespace

bool keeps_residual(const HierPass& pass, std::size_t layer) {
  return pass.source.compat.magic_compat || !stops_hierarchically(pass.style, layer);  // Q-MASK-402
}

void check_and_erase(HierPass& pass, LayerPlanes& total, const LayerPlanes& comp) {
  for (std::size_t i = 0; i < pass.style.layers.size(); ++i) {
    MaskPlane* comp_plane = comp[i];
    MaskPlane* total_plane = total[i];
    if (comp_plane == nullptr || total_plane == nullptr) {
      continue;
    }
    const bool check = !pass.style.layers[i].temporary && keeps_residual(pass, i);
    for (const db::TileView& t : comp_plane->solid_tiles()) {
      if (check) {
        report_space(pass, i, *total_plane, t);
      }
      erase_tile(*total_plane, t);
      add_hier_tile(pass.ctx);
    }
  }
}

void paint_residual(HierPass& pass, const LayerPlanes& total, LayerPlanes& out) {
  const bool slivers = pass.style.has_option(style_option::grow_slivers);
  for (std::size_t i = 0; i < pass.style.layers.size() && i < total.size(); ++i) {
    const MaskPlane* plane = total[i];
    if (plane == nullptr || !keeps_residual(pass, i)) {
      continue;
    }
    if (out.planes[i] == nullptr) {
      out.planes[i] = &pass.ctx.pool.acquire();
    }
    const std::int32_t min_width = pass.style.layers[i].min_width;
    for (const db::TileView& t : plane->solid_tiles()) {
      const db::Rect area =
          slivers ? grow_sliver(plane->plane(), t, min_width, pass.source.compat.magic_compat) : t.area;
      paint_piece(*out.planes[i], t, area);
      add_hier_tile(pass.ctx);
      ++pass.ctx.statistics.hier_rects;  // CIFHierRects
    }
  }
}

void paint_piece(MaskPlane& plane, const db::TileView& t, const db::Rect& area) {
  if (t.split) {
    (void)plane.paint_triangle(area, t.diagonal, t.right_side);
  } else {
    (void)plane.paint(area);
  }
}

// cifGenHierArea.
void gen_hier_area(HierPass& pass, const db::Rect& area, const tech::TypeMask& yank, const std::vector<bool>& gen,
                   LayerPlanes& out) {
  const db::CellDef& cell = pass.cell;
  std::unique_ptr<db::CellDef> total_def = make_scratch("__CIF__", *pass.source.technology);
  (void)pass.flattener.copy(cell, grow(area, pass.radius), db::kIdentity, yank, *total_def);
  GenerateOptions options;
  options.layers = gen;
  options.hierarchical = true;
  options.unfracture = pass.unfracture;
  pass.error_cell = &cell;
  LayerPlanes total = generate_area(pass.source, *total_def, &cell, area, pass.style, pass.ctx, options);

  options.report_errors = false;  // reported when the cells are generated on their own
  LayerPlanes comp(pass.ctx.pool, pass.style.layers.size());
  std::unique_ptr<db::CellDef> comp_def = make_scratch("__CIF2__", *pass.source.technology);
  (void)search_elements(cell, area, [&](const ElementRef& e, const db::Rect& child_area) {
    clear_scratch(*comp_def);
    (void)pass.flattener.copy(*e.use->def, grow(child_area, pass.radius), e.to_parent, yank, *comp_def);
    generate_area_into(pass.source, *comp_def, e.use->def, db::apply(e.to_parent, child_area), pass.style, pass.ctx,
                       options, comp);
    return pass.ctx.cancelled() ? Visit::stop : Visit::next;
  });
  generate_area_into(pass.source, cell, &cell, area, pass.style, pass.ctx, options, comp);
  check_and_erase(pass, total, comp);
  paint_residual(pass, total, out);
  ++pass.ctx.statistics.interaction_areas;
}

namespace {

// cifSubtreeHasTypes: paint of `types` in `def` or a descendant, each
// definition searched once.
bool subtree_has_types(const db::CellDef& def, const tech::TypeMask& types,
                       std::unordered_map<const db::CellDef*, bool>& visited, int depth) {
  const auto [it, fresh] = visited.try_emplace(&def, false);
  if (!fresh || depth > kMaxDepth) {
    return it->second;
  }
  bool found = false;
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count() && !found; ++p) {
    found = !def.plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), types, [](const db::TileView&) {
      return false;
    });
  }
  for (const auto& use : def.uses()) {
    if (found) {
      break;
    }
    found = use->def != nullptr && subtree_has_types(*use->def, types, visited, depth + 1);
  }
  visited[&def] = found;
  return found;
}

// Whether the non-local correction runs for `cell` ("Non-local layers").
bool runs_nonlocal(const db::CellDef& cell, const LayerClasses& classes) {
  if (std::ranges::none_of(classes.nonlocal, [](bool b) { return b; }) || cell.uses().empty()) {
    return false;
  }
  std::unordered_map<const db::CellDef*, bool> visited;
  return subtree_has_types(cell, classes.nonlocal_seed, visited, 0);
}

}  // namespace

// --- Square skipping -------------------------------------------------------------

SquareGrid::SquareGrid(const db::CellDef& cell, const db::Rect& area, std::int64_t radius, std::int64_t step)
    : total_(clip(area, cell.bbox())), radius_(radius), step_(std::max<std::int64_t>(step, 1)) {
  // A use whose bounding box is farther than 2r (plus a unit for touching)
  // from a square is not found by the finder's search of the square grown
  // by 2r, so that square has no interaction.
  const std::int64_t reach = 2 * radius_ + 1;
  for (const auto& use : cell.uses()) {
    reach_.push_back(grow(use->bbox, reach));
  }
}

std::uint64_t SquareGrid::count() const noexcept {
  if (total_.is_null()) {
    return 0;
  }
  const auto cuts = [this](std::int64_t len) { return static_cast<std::uint64_t>((len + step_ - 1) / step_); };
  return cuts(total_.height()) * cuts(total_.width());
}

void SquareGrid::for_each(const std::function<bool(const db::Rect& square)>& visit) const {
  for (std::int64_t y = total_.y0; y < total_.y1; y += step_) {
    const std::int64_t y1 = std::min<std::int64_t>(y + step_, total_.y1);
    std::vector<db::Rect> row;
    for (const db::Rect& r : reach_) {
      if (r.y0 <= y1 && r.y1 >= y) {
        row.push_back(r);
      }
    }
    if (!row.empty() && !walk_row(row, y, y1, visit)) {
      return;
    }
  }
}

bool SquareGrid::walk_row(const std::vector<db::Rect>& row, std::int64_t y, std::int64_t y1,
                          const std::function<bool(const db::Rect&)>& visit) const {
  std::int64_t x = total_.x0;
  while (x < total_.x1) {
    // The first use reaching at or right of x: jump to its square.
    std::int64_t next = total_.x1;
    for (const db::Rect& r : row) {
      if (r.x1 >= x) {
        next = std::min<std::int64_t>(next, std::max<std::int64_t>(x, r.x0));
      }
    }
    if (next >= total_.x1) {
      return true;
    }
    x += (next - x) / step_ * step_;  // stay on the grid
    const db::Rect square{static_cast<db::Coord>(x), static_cast<db::Coord>(y),
                          static_cast<db::Coord>(std::min<std::int64_t>(x + step_, total_.x1)),
                          static_cast<db::Coord>(y1)};
    if (!visit(grow(square, radius_))) {
      return false;
    }
    x += step_;
  }
  return true;
}

}  // namespace gen_detail

using gen_detail::HierPass;

// --- Generation area -----------------------------------------------------------

db::Rect generation_area(const db::CellDef& cell, std::int64_t radius, bool fixed_bbox) {
  db::Rect area = grow(cell.bbox(), radius);
  if (!fixed_bbox) {
    return area;
  }
  const db::PropertyValue* value = cell.properties().find("FIXED_BBOX");
  const auto* dims = value != nullptr ? std::get_if<db::DimensionList>(value) : nullptr;
  if (dims != nullptr && dims->values.size() == 4) {
    const std::vector<std::int32_t>& v = dims->values;
    area = db::include(area, db::Rect{v[0], v[1], v[2], v[3]});
  }
  return area;
}

// --- The output switches ----------------------------------------------------------

HierarchyOptions hierarchy_options(const db::CellDef& cell, const HierarchyOptions& inherited, bool gds,
                                   const compat::CompatSettings& compat) {
  HierarchyOptions out = inherited;
  if ((gds || !compat.magic_compat) && cell.properties().find("CIFhier") != nullptr) {
    out.subcells = true;
    out.arrays = true;
  }
  return out;
}

// --- CIFGenSubcells --------------------------------------------------------------

void generate_subcells(const GenerationSource& source, const db::CellDef& cell, const db::Rect& area,
                       const OutputStyle& style, GenerationContext& ctx, LayerPlanes& into) {
  if (source.technology == nullptr) {
    return;
  }
  HierPass pass(source, style, ctx, cell);
  const LayerClasses classes = classify_layers(style, source.compat);
  std::vector<bool> local = classes.hier;
  const bool nonlocal = gen_detail::runs_nonlocal(cell, classes);
  if (nonlocal) {
    for (std::size_t i = 0; i < local.size(); ++i) {
      local[i] = local[i] && !classes.nonlocal[i];
    }
  }
  const std::uint64_t old_tiles = ctx.statistics.flat_tiles;
  const gen_detail::SquareGrid grid(cell, area, pass.radius, interaction_step(style, pass.radius));
  ProgressThrottle throttle(ctx.progress, ctx.clock);
  std::uint64_t cuts = 0;
  if (source.find_interaction) {
    grid.for_each([&](const db::Rect& square) {
      if (ctx.cancelled()) {
        return false;
      }
      const std::optional<db::Rect> interaction = source.find_interaction(cell, square, pass.radius);
      if (interaction) {
        gen_detail::gen_hier_area(pass, *interaction, classes.yank, local, into);
        throttle.step(++cuts, grid.count());
      }
      return true;
    });
  }
  if (nonlocal && !ctx.cancelled()) {
    gen_detail::gen_hier_area(pass, area, classes.nonlocal_yank, classes.nonlocal_gen, into);
  }
  ctx.statistics.hier_tiles += ctx.statistics.flat_tiles - old_tiles;  // CIFHierTileOps
  throttle.finish();
}

// --- Per-cell output ----------------------------------------------------------------

LayerPlanes generate_hierarchical(const GenerationSource& source, const db::CellDef& cell, const OutputStyle& style,
                                  GenerationContext& ctx, const HierarchyOptions& options) {
  if (source.technology == nullptr) {
    return {};
  }
  const db::Rect area = generation_area(cell, generation_radius(style, source.compat), options.fixed_bbox);
  GenerateOptions own;
  own.unfracture = options.unfracture;
  LayerPlanes out = generate_area(source, cell, &cell, area, style, ctx, own);
  if (options.subcells && !ctx.cancelled()) {
    generate_subcells(source, cell, area, style, ctx, out);
  }
  if (options.arrays && !ctx.cancelled()) {
    generate_arrays(source, cell, area, style, ctx, out);
  }
  return out;
}

LayerPlanes generate_hierarchical(editor::EditorState& editor, const db::CellDef& cell, const OutputStyle& style,
                                  GenerationContext& ctx) {
  HierarchyOptions options;
  options.subcells = editor.mask.cif.hier;
  options.arrays = editor.mask.cif.array;
  options.unfracture = editor.mask.cif.unfracture;
  return generate_hierarchical(generation_source(editor), cell, style, ctx, options);
}

diag::Outcome<std::vector<db::Rect>> interaction_areas(const editor::EditorState& editor, const db::CellDef& cell,
                                                       const db::Rect& area, std::int64_t halo, std::int32_t step) {
  const GenerationSource source = generation_source(editor);
  const std::int64_t side = step > 0 ? step : std::max<std::int64_t>(20 * std::min<std::int64_t>(halo, db::kInfinity), 50);
  std::vector<db::Rect> out;
  const gen_detail::SquareGrid grid(cell, area, halo, side);
  grid.for_each([&](const db::Rect& square) {
    if (std::optional<db::Rect> found = source.find_interaction(cell, square, halo)) {
      out.push_back(*found);
    }
    return true;
  });
  return out;
}

// --- Flat CIF output -----------------------------------------------------------------

namespace {

void copy_labels(const db::CellDef& cell, const db::Rect& area, db::CellDef& scratch) {
  db::search_labels(cell, area, [&](const db::LabelHit& hit) {
    db::Label label = *hit.label;
    label.rect = db::apply(hit.to_root, label.rect);
    label.justification = db::transform_position(hit.to_root, label.justification);
    label.rotation = db::transform_angle(hit.to_root, label.rotation, false);
    label.text = hit.path + label.text;
    label.text_bbox.reset();
    scratch.labels().push_back(std::move(label));
    return true;
  });
}

}  // namespace

void flatten_for_output(const GenerationSource& source, const db::CellDef& cell, const OutputStyle& style,
                        db::CellDef& scratch) {
  if (source.technology == nullptr) {
    return;
  }
  const db::Rect area = grow(cell.bbox(), generation_radius(style, source.compat));
  gen_detail::Flattener flattener(*source.technology, source.compat.magic_compat);
  (void)flattener.copy_paint(cell, area, db::kIdentity, gen_detail::all_but_space_and_drc(), scratch);
  if (!source.compat.magic_compat) {  // Q-MASK-411 OFF
    flattener.copy_hints(cell, area, db::kIdentity, scratch);
    copy_labels(cell, area, scratch);
  }
  db::recompute_bbox(scratch, source.compat.magic_compat);
}

}  // namespace hefesto::mask
