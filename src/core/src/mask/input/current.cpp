// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIFPaintCurrent: the per-cell processing of the read planes ("Per-cell
// layer processing order", "Painting rule results", "Input operators or,
// and, and-not", "grow and shrink", "grow-grid", "not-square", "copyup",
// "boundary", "mask-hints", "tagged"). Each rule's operators run through
// the shared operator kernels (evaluate_layer, capability
// mask-output-operators) over an evaluation style built from the input
// rules; the compat decisions of the input operators are made there:
//   - Q-MASK-306: ON grows Euclidean when the session's output style says
//     grow-euclidean (Magic reads CIFCurStyle while reading); OFF is square;
//   - Q-MASK-307: ON evaluates boundary and mask-hints with their output
//     meaning (FIXED_BBOX and MASKHINTS_* scaled by the output style's scale
//     factor); OFF drops them from the evaluation;
//   - Q-MASK-310 / Q-MASK-6: the tagged label rectangle is used unscaled
//     (ON) or multiplied by S (OFF), through the evaluation style's scale;
//   - Q-MASK-330 / Q-MASK-7 and Q-MASK-334 / Q-MASK-1: the kernels' own
//     branch points.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFrdcl.c
// (CIFPaintCurrent, cifCopyPaintFunc, cifMaskHintFunc, cifMakeBoundaryFunc,
// cifPaintCurrentFunc), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Deviations: a repeated `mask-hints NAME` unions into MASKHINTS_NAME in
// both modes (Q-MASK-308, fix; Magic looks up the bare name and replaces
// the property). The `gds maskhints` comparison at the end of
// CIFPaintCurrent belongs to the GDS reader (hints::gds_maskhints_region).
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <utility>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/hints/hints.hpp"
#include "hefesto/mask/ops/evaluate.hpp"
#include "mask/input/engine_state.hpp"
#include "mask/input/reader.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask {

namespace input_detail {

namespace {

std::string rule_name(const InputStyle& style, const InputRule& rule, const tech::Technology& technology) {
  if (rule.temporary) {
    return rule.cif_layer < style.cif_layers.size() ? style.cif_layers[rule.cif_layer] : std::string{};
  }
  return std::string{technology.type_name(rule.type)};
}

std::vector<LayerOp> evaluation_ops(const InputRule& rule, const compat::CompatSettings& compat) {
  std::vector<LayerOp> ops;
  ops.reserve(rule.ops.size());
  for (const LayerOp& op : rule.ops) {
    const bool output_meaning = op.kind == OpKind::boundary || op.kind == OpKind::mask_hints;
    if (output_meaning && !compat.magic_compat) {
      continue;  // Q-MASK-307 OFF: no output meaning in the evaluation
    }
    ops.push_back(op);
  }
  return ops;
}

}  // namespace

OutputStyle evaluation_style(const InputStyle& style, const OutputStyle* output, const tech::Technology& technology,
                             const compat::CompatSettings& compat) {
  OutputStyle eval;
  eval.name = style.name;
  const std::size_t n = style.cif_layers.size();
  eval.layers.resize(n + style.rules.size());
  for (std::size_t i = 0; i < n; ++i) {
    eval.layers[i].name = style.cif_layers[i];
    eval.layers[i].temporary = true;
  }
  for (std::size_t r = 0; r < style.rules.size(); ++r) {
    OutputLayer& layer = eval.layers[n + r];
    layer.name = rule_name(style, style.rules[r], technology);
    layer.temporary = style.rules[r].temporary;
    layer.ops = evaluation_ops(style.rules[r], compat);
  }
  if (compat.magic_compat) {
    // Magic's CIFGenLayer reads CIFCurStyle during a read (Q-MASK-306, 307 ON).
    eval.scale.scale_factor = output != nullptr ? output->scale.scale_factor : 1;
    eval.grid_limit = output != nullptr ? output->grid_limit : 0;
    eval.options = output != nullptr && output->has_option(style_option::grow_euclidean) ? style_option::grow_euclidean : 0;
  } else {
    eval.scale.scale_factor = style_text::clamp_int(style.scale_factor);  // Q-MASK-310 OFF: label area times S
  }
  eval.options |= style_option::no_errors;  // the input pass has no error cell (CIFErrorDef)
  return eval;
}

}  // namespace input_detail

const OutputStyle& ReadEngine::evaluation() {
  if (!state_->evaluation_valid) {
    state_->evaluation = input_detail::evaluation_style(state_->style, editor_.mask.output_styles.current_style(),
                                                        *editor_.technology, compat_);
    state_->evaluation_valid = true;
  }
  return state_->evaluation;
}

void ReadEngine::note_rule(const InputRule& rule) {
  const OutputStyle* output = editor_.mask.output_styles.current_style();
  const bool grows = rule.find(OpKind::grow) != nullptr || rule.find(OpKind::shrink) != nullptr;
  if (grows && !state_->euclidean_noted && output != nullptr && output->has_option(style_option::grow_euclidean)) {
    state_->euclidean_noted = true;
    state_->emit(diag::make_native(input_detail::kEuclideanNote,
                                   std::string{"input grow/shrink while the output style has grow-euclidean: "} +
                                       (compat_.magic_compat ? "Euclidean, as Magic (Q-MASK-306)"
                                                             : "square (Q-MASK-306)")));
  }
  if (rule.find(OpKind::tagged) != nullptr && !state_->tagged_noted && state_->style.scale_factor != 1) {
    state_->tagged_noted = true;
    state_->emit(diag::make_native(input_detail::kTaggedScaleNote,
                                   "input tagged rule with scale factor " +
                                       std::to_string(state_->style.scale_factor) +
                                       (compat_.magic_compat ? ": label area unscaled, as Magic (Q-MASK-310)"
                                                             : ": label area scaled (Q-MASK-310)")));
  }
}

void ReadEngine::paint_layer_rule(const MaskPlane& result, db::CellDef& cell, tech::TileType type) {
  const tech::Technology& technology = *editor_.technology;
  const bool contact = technology.is_contact(type);
  for (const db::TileView& tile : result.solid_tiles()) {
    const db::Rect area = scale_rect(tile.area, Snap::exact, contact);
    if (area.x0 == area.x1 || area.y0 == area.y1) {
      continue;  // degenerate after a limited rescale
    }
    std::optional<db::TriangleSpec> triangle;
    if (tile.split) {
      triangle = db::TriangleSpec{tile.diagonal, tile.right_side};
    }
    (void)db::paint_type(cell, technology, area, type, triangle);
  }
}

void ReadEngine::make_boundary(const db::Rect& tile_area, db::CellDef& cell) {
  const db::Rect area = scale_rect(tile_area, Snap::exact);
  if (cell.flags().fixed_bbox) {
    const db::PropertyValue* value = cell.properties().find("FIXED_BBOX");
    const auto* dims = value != nullptr ? std::get_if<db::DimensionList>(value) : nullptr;
    if (dims != nullptr && dims->values.size() == 4 &&
        db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]} != area) {
      state_->emit(diag::make(input_detail::kBoundaryRedefined, "Warning:  Cell " + cell.name() + " boundary was redefined."));
    }
  }
  db::set_property(cell, "FIXED_BBOX", db::DimensionList{{area.x0, area.y0, area.x1, area.y1}});
  cell.flags().fixed_bbox = true;
}

void ReadEngine::store_hints(const MaskPlane& result, db::CellDef& cell, const std::string& name) {
  hints::HintRegion region;
  for (const db::TileView& tile : result.solid_tiles()) {
    region.paint(scale_rect(tile.area, Snap::exact));
  }
  hints::add_input_region(cell, name, region);
}

void ReadEngine::save_copyup(const MaskPlane& result, const db::CellDef& cell, const LayerOp& copyup) {
  std::vector<MaskPlane*>& saved = state_->saved[&cell];
  saved.resize(state_->style.cif_layers.size(), nullptr);
  for (const std::size_t layer : copyup.cif_layers) {
    if (layer >= saved.size()) {
      continue;
    }
    if (saved[layer] == nullptr) {
      saved[layer] = &state_->pool.acquire();
    }
    (void)saved[layer]->unite(result);
  }
}

void ReadEngine::templayer_effects(const InputRule& rule, const MaskPlane& result, db::CellDef& cell) {
  if (result.empty()) {
    return;
  }
  if (const LayerOp* copyup = rule.find(OpKind::copyup)) {
    save_copyup(result, cell, *copyup);
    return;  // boundary and mask-hints are ignored in a copyup templayer
  }
  if (rule.find(OpKind::boundary) != nullptr) {
    for (const db::TileView& tile : result.solid_tiles()) {
      make_boundary(tile.area, cell);  // the last tile wins
    }
  }
  if (const LayerOp* hints = rule.find(OpKind::mask_hints)) {
    store_hints(result, cell, hints->text);
  }
}

void ReadEngine::paint_current(std::vector<MaskPlane*>& planes, db::CellDef& cell) {
  const OutputStyle& eval = evaluation();
  const std::size_t n = state_->style.cif_layers.size();
  OperatorContext ctx;
  ctx.compat = compat_;
  ctx.cell = &cell;
  ctx.pool = &state_->pool;
  const LayerInputs inputs{&cell, std::span<MaskPlane* const>{planes}, editor_.technology.get()};
  for (std::size_t r = 0; r < state_->style.rules.size(); ++r) {
    const InputRule& rule = state_->style.rules[r];
    note_rule(rule);
    MaskPlane& result = evaluate_layer(eval, n + r, inputs, db::whole_plane(), ctx);
    ctx.stale_marks.clear();
    if (!rule.temporary) {
      paint_layer_rule(result, cell, rule.type);
      state_->pool.release(result);
      continue;
    }
    templayer_effects(rule, result, cell);
    if (rule.cif_layer < planes.size() && planes[rule.cif_layer] != nullptr) {
      state_->pool.release(*planes[rule.cif_layer]);  // the templayer replaces the read plane
      planes[rule.cif_layer] = &result;
    } else {
      state_->pool.release(result);
    }
  }
  for (MaskPlane* plane : planes) {
    if (plane != nullptr) {
      plane->clear();
    }
  }
}

}  // namespace hefesto::mask
