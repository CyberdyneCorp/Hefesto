// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_subtree (extract-hierarchy "Interaction halo and step squares",
// "Interaction processing and connections (pass 1)", "Merge records and
// pass-2 adjustments", "Sticky labels across the hierarchy"). Ported from
// Magic 8.3.694 (fc5daa9e) extract/ExtSubtree.c (extSubtree,
// extSubtreeInteraction, extSubtreeFunc, extSubstrateFunc); Magic is
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include "hefesto/extract/hier/subtree.hpp"

#include <utility>

#include "hefesto/db/paint.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/extract/hier/interact.hpp"
#include "hefesto/extract/hier/yank.hpp"
#include "hier_context.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract {

namespace hier {

namespace {

const tech::TypeMask& paint_types() {
  static const tech::TypeMask mask = ~tech::TypeMask::of(tech::kSpace);
  return mask;
}

// DBCellCopyPaint (one level) of `source` within `area` into `target`.
void copy_level(const db::CellDef& source, const db::Rect& area, db::CellDef& target, const tech::Technology& tech) {
  if (area.x0 > area.x1 || area.y0 > area.y1) {
    return;
  }
  (void)db::copy_paint(source, area, db::kIdentity, target, tech, paint_types());
}

// Whether a parent sticky label lies over material that does not connect
// to it (or over nothing), so it is copied into the cumulative buffer.
bool label_over_other(const Hier& h, const db::Label& lab) {
  const auto home = h.env.tech->home_plane(lab.type);
  if (!home) {
    return false;
  }
  const db::Plane& plane = h.parent->plane(*home);
  db::Rect r = clip(lab.rect, h.inter_area);
  if (r.x0 == r.x1) {
    if (r.y0 == r.y1 && h.env.tech->connects_to(lab.type).test(plane.type_at({r.x0, r.y0}))) {
      return false;
    }
    --r.x0;
    ++r.x1;
  }
  if (r.y0 == r.y1) {
    --r.y0;
    ++r.y1;
  }
  const bool stopped = !plane.enumerate(r, h.env.tech->not_connected(lab.type), [](const db::TileView&) {
    return false;
  });
  return stopped;
}

// extSubtreeFunc's first pass: the parent's sticky labels into the
// cumulative buffer.
void copy_sticky_labels(Hier& h) {
  auto& target = h.cum.def->labels();
  for (const db::Label& lab : h.parent->labels()) {
    if (!lab.sticky || !db::label_in_area(lab.rect, h.inter_area) || !label_over_other(h, lab)) {
      continue;
    }
    target.insert(target.begin(), lab);
  }
}

// extSubtreeFunc.
void subtree_func(Hier& h, const db::CellUse& use, bool& first_pass) {
  auto one = new_flat(h.env, h.parent->plane_count());
  one->real_use = &use;
  h.sub_use = &use;
  h.sub_area = clip(use.bbox, h.inter_area);
  yank_use(use, h.sub_area, *one->def, *h.env.tech, true);
  extract_flat(*one, h.env, h.clip_area, true, h.env.couple_adjust());
  if (first_pass) {
    first_pass = false;
    copy_sticky_labels(h);
  } else {
    label_regions(h.cum, h.env);
  }
  connect_trees(h, h.cum, *one);
  connect_use_substrate(h, use, false);
  drop_nodes(h.cum);
  copy_level(*one->def, h.sub_area, *h.cum.def, *h.env.tech);
  copy_labels_to_front(*one->def, *h.cum.def);
  h.subs.insert(h.subs.begin(), std::move(one));
}

// extSubtreeInteraction.
void interaction(Hier& h) {
  copy_level(*h.parent, h.inter_area, *h.cum.def, *h.env.tech);
  auto first = new_flat(h.env, h.parent->plane_count());
  copy_level(*h.parent, h.inter_area, *first->def, *h.env.tech);
  extract_flat(*first, h.env, h.clip_area, false, h.env.couple_adjust());
  first->look_names = h.parent;
  h.subs.clear();
  h.subs.push_back(std::move(first));
  drop_nodes(h.cum);
  h.cum.look_names = h.parent;
  bool first_pass = true;
  for (const db::CellUse* use : uses_in(*h.parent, h.inter_area)) {
    subtree_func(h, *use, first_pass);
  }
  if (h.env.options->adjust) {
    adjust_interaction(h);
  }
  for (auto& one : h.subs) {
    clear_flat(*one);
  }
  h.subs.clear();
  drop_nodes(h.cum);
  h.cum.coupling = CouplingTable{};
  clear_yank_cell(*h.cum.def);
}

// extSubstrateFunc for the uses near a square without interaction.
void substrate_only(Hier& h, const db::Rect& square) {
  h.clip_area = square;
  for (const db::CellUse* use : uses_in(*h.parent, square)) {
    h.sub_use = use;
    connect_use_substrate(h, *use, true);
  }
}

// The step squares; false when the pass was cancelled (Magic checks
// SigInterruptPending before each square, ExtSubtree.c:251).
bool run_squares(Hier& h, std::int32_t step) {
  return for_each_square(*h.parent, step, h.env.halo, h.env.compat->magic_compat, [&](const InteractionSquare& s) {
    if (h.env.cancelled()) {
      return false;
    }
    if (s.kind == SquareKind::interaction) {
      h.inter_area = s.area;
      h.clip_area = s.clip;
      interaction(h);
    } else {
      substrate_only(h, s.square);
    }
    return true;
  });
}

}  // namespace

}  // namespace hier

diag::Outcome<SubtreeResult> extract_subtree(ExtractContext& ctx, db::CellDef& cell, ExtWriter& writer,
                                             const SubtreeInputs& inputs) {
  if (inputs.technology == nullptr || ctx.style == nullptr) {
    return diag::make(diag::codes::kCmdInternal, "extract_subtree needs a technology and a style.");
  }
  SubtreeResult result;
  hier::Hier h;
  h.env = hier::Env{ctx.style.get(), inputs.technology, &ctx.options, &ctx.compat,
                    hier_halo(*ctx.style, ctx.options, ctx.overrides), ctx.overrides.halo, ctx.progress};
  h.parent = &cell;
  h.inputs = &inputs;
  h.writer = &writer;
  h.result = &result;
  h.table = HierNodeTable(ctx.style->resist_classes.size());
  if (!ctx.yank.cumulative) {
    ctx.yank.cumulative = new_yank_cell("__CUMULATIVE__", cell.plane_count());
  }
  clear_yank_cell(*ctx.yank.cumulative);
  ctx.yank.cumulative->set_plane_count(cell.plane_count());
  h.cum.def = ctx.yank.cumulative.get();

  // A cancelled pass still writes the merges found so far (Magic's done:
  // label) and skips extArray (ExtCell.c:523-524).
  result.cancelled = !hier::run_squares(h, hier_step(*ctx.style, ctx.overrides));
  h.table.write(writer, MergeClass::subcell, ctx.style->cscale);
  h.sub_extracted.clear();
  if (!result.cancelled) {
    hier::extract_arrays(h);
    result.cancelled = h.env.cancelled();
  }
  return result;
}

}  // namespace hefesto::extract
