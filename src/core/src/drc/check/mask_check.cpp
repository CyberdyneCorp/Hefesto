// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DRC on generated mask layers (capability drc-cif-rules; M5 design D14):
// the primitive rules of the mask-layer statements (drcCifWidth,
// drcCifSpacing, drcCifArea, drcCifMaxwidth), the error mapping of Q-DRC-13
// and the drcCifCheck entry: generation of the layers the rules read, then
// the per-tile checks of mask_check_tile.cpp.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) drc/DRCcif.c, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: DRCCIF (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/drc/check/mask_check.hpp"

#include <algorithm>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/mask/gen/context.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "mask_check_internal.hpp"

namespace hefesto::drc {

namespace {

const tech::TypeMask& space_bits() {
  static const tech::TypeMask mask = tech::TypeMask::of(tech::kSpace);
  return mask;
}

const tech::TypeMask& solid_bits() {
  static const tech::TypeMask mask = tech::TypeMask::of(mask::kMaskSolid);
  return mask;
}

/// drcCifAssign: one cookie of distance `dist`, corner distance `cdist`.
MaskRule cookie(const MaskLayerRule& statement, std::int32_t dist, std::int32_t cdist, const tech::TypeMask& ok,
                const tech::TypeMask& corner, RuleFlags rule_flags, std::size_t check_layer) {
  MaskRule out;
  out.rule.dist = dist;
  out.rule.cdist = cdist;
  out.rule.ok = ok;
  out.rule.corner = corner;
  out.rule.flags = rule_flags | flags::kCifRule;
  out.rule.why = statement.why;
  out.rule.exception = statement.exception;
  out.check_layer = check_layer;
  return out;
}

/// Appends in Magic's insertion order; compile_mask_rules() reverses each
/// list at the end, which is Magic's push to the front.
void push(MaskRules& rules, std::size_t layer, MaskList which, MaskRule rule) {
  rules.layers[layer][static_cast<std::size_t>(which)].push_back(std::move(rule));
}

void add_width(MaskRules& rules, const MaskLayerRule& m, std::size_t layer) {
  push(rules, layer, MaskList::solid_tiles,
       cookie(m, m.distance, m.distance, solid_bits(), solid_bits(), flags::kForward, layer));
}

void add_area(MaskRules& rules, const MaskLayerRule& m, std::size_t layer) {
  // dist = the horizon, cdist = the area (mask units squared).
  push(rules, layer, MaskList::solid_tiles,
       cookie(m, m.value2, m.distance, solid_bits(), solid_bits(), flags::kArea | flags::kForward, layer));
}

void add_maxwidth(MaskRules& rules, const MaskLayerRule& m, std::size_t layer) {
  push(rules, layer, MaskList::solid_tiles,
       cookie(m, m.distance, m.distance, solid_bits(), solid_bits(), flags::kMaxWidth | m.flags, layer));
}

// drcCifSpacing (DRC_CIF_SOLID is the space-tile list, DRC_CIF_SPACE the
// solid-tile list).
void add_spacing(MaskRules& rules, const MaskLayerRule& m, std::size_t l0, std::size_t l1, std::int32_t scale) {
  const bool reverse_needed = !m.touching_ok;
  const tech::TypeMask corner = m.touching_ok ? space_bits() : tech::TypeMask::all();
  const RuleFlags both = reverse_needed ? flags::kBothCorners : 0;
  const std::int32_t d = m.distance;
  push(rules, l0, MaskList::space_tiles, cookie(m, d, d, space_bits(), corner, flags::kForward | both, l1));
  push(rules, l0, MaskList::solid_tiles, cookie(m, d, d, space_bits(), corner, flags::kReverse | both, l1));
  if (reverse_needed) {
    push(rules, l1, MaskList::space_tiles,
         cookie(m, d, d, space_bits(), corner, flags::kForward | flags::kBothCorners, l0));
    push(rules, l1, MaskList::solid_tiles,
         cookie(m, d, d, space_bits(), corner, flags::kReverse | flags::kBothCorners, l0));
    if (l0 == l1) {  // two more identical rules, as Magic compiles them
      push(rules, l1, MaskList::solid_tiles,
           cookie(m, d, d, space_bits(), corner, flags::kReverse | flags::kBothCorners, l0));
      push(rules, l0, MaskList::solid_tiles,
           cookie(m, d, d, space_bits(), corner, flags::kReverse | flags::kBothCorners, l1));
    }
  }
  if (l0 != l1) {  // coincident material of the two layers
    push(rules, l1, MaskList::solid_tiles, cookie(m, scale, scale, space_bits(), {}, flags::kForward, l0));
    push(rules, l0, MaskList::solid_tiles, cookie(m, scale, scale, space_bits(), {}, flags::kForward, l1));
  }
}

void add_statement(MaskRules& rules, const MaskLayerRule& m, const mask::OutputStyle& style) {
  const std::optional<std::size_t> layer = style.find_layer(m.layer);
  if (!layer) {
    return;
  }
  if (m.keyword == "cifwidth") {
    add_width(rules, m, *layer);
  } else if (m.keyword == "cifarea") {
    add_area(rules, m, *layer);
  } else if (m.keyword == "cifmaxwidth") {
    add_maxwidth(rules, m, *layer);
  } else if (m.keyword == "cifspacing") {
    if (const std::optional<std::size_t> second = style.find_layer(m.layer2)) {
      add_spacing(rules, m, *layer, *second, style.scale.scale_factor);
    }
  }
}

/// drcCifFinal's drcCifGenLayers.
std::vector<bool> used_layers(const MaskRules& rules) {
  std::vector<bool> used(rules.layers.size(), false);
  for (std::size_t i = 0; i < rules.layers.size(); ++i) {
    for (const std::vector<MaskRule>& list : rules.layers[i]) {
      for (const MaskRule& r : list) {
        used[i] = true;
        if (r.check_layer < used.size()) {
          used[r.check_layer] = true;
        }
      }
    }
  }
  return used;
}

/// C's truncating division, and floor and ceiling for Q-DRC-13 OFF.
db::Coord floor_div(db::Coord v, db::Coord s) noexcept {
  const db::Coord q = v / s;
  return (v % s != 0 && v < 0) ? q - 1 : q;
}

db::Coord ceil_div(db::Coord v, db::Coord s) noexcept {
  const db::Coord q = v / s;
  return (v % s != 0 && v > 0) ? q + 1 : q;
}

void widen(db::Coord& lo, db::Coord& hi) noexcept {
  if (lo != hi) {
    return;
  }
  if (lo < 0) {
    --lo;
  } else {
    ++hi;
  }
}

std::int32_t positive_scale(std::int32_t scale) noexcept { return scale > 0 ? scale : 1; }

/// The layers generation must produce: the ones the rules use and every
/// earlier layer they read (CIFGen with DBAllTypeBits generates them all;
/// the others cannot change a checked layer).
std::vector<bool> layers_to_generate(const MaskRules& rules, const mask::OutputStyle& style) {
  std::string names;
  for (std::size_t i = 0; i < rules.used.size() && i < style.layers.size(); ++i) {
    if (rules.used[i]) {
      names += (names.empty() ? "" : ",") + style.layers[i].name;
    }
  }
  diag::Outcome<mask::LayerSelection> selected = mask::select_layers(style, names);
  return selected.ok() ? selected.value().depend : std::vector<bool>{};  // empty = every layer
}

const MaskRuleCache& compiled_rules(MaskRuleCache& cache, const StyleDetails& details,
                                    const std::shared_ptr<const mask::OutputStyle>& style) {
  if (cache.details != &details || cache.style != style) {
    cache.details = &details;
    cache.style = style;
    cache.rules = compile_mask_rules(details, *style);
    cache.layers = layers_to_generate(cache.rules, *style);
  }
  return cache;
}

void report_collapsed(mask::GenerationContext& ctx, std::size_t collapsed, bool magic_compat) {
  if (collapsed == 0 || ctx.sink == nullptr) {
    return;
  }
  const std::string count = std::to_string(collapsed);
  if (magic_compat) {
    ctx.sink->emit(diag::make_native(
        "DRC-MASK-001", count + " mask-layer area or maxwidth error(s) narrower than one internal unit were dropped"));
  } else {
    ctx.sink->emit(diag::make_native(
        "DRC-MASK-002",
        count + " mask-layer area or maxwidth error(s) narrower than one internal unit were reported rounded outward"));
  }
}

}  // namespace

bool MaskRules::empty() const noexcept {
  return std::all_of(layers.begin(), layers.end(),
                     [](const auto& lists) { return lists[0].empty() && lists[1].empty(); });
}

MaskRules compile_mask_rules(const StyleDetails& details, const mask::OutputStyle& style) {
  MaskRules rules;
  rules.layers.resize(style.layers.size());
  for (const MaskLayerRule& m : details.mask_layer_rules) {
    add_statement(rules, m, style);
  }
  for (auto& lists : rules.layers) {
    for (std::vector<MaskRule>& list : lists) {
      std::reverse(list.begin(), list.end());
    }
  }
  rules.used = used_layers(rules);
  return rules;
}

std::optional<db::Rect> map_mask_error(const db::Rect& mask_rect, std::int32_t scale, MaskErrorKind kind,
                                       const compat::CompatSettings& compat) {
  const db::Coord s = positive_scale(scale);
  if (!compat.magic_compat) {
    const db::Rect out{floor_div(mask_rect.x0, s), floor_div(mask_rect.y0, s), ceil_div(mask_rect.x1, s),
                       ceil_div(mask_rect.y1, s)};
    return out.x0 < out.x1 && out.y0 < out.y1 ? std::optional{out} : std::nullopt;
  }
  db::Rect out{mask_rect.x0 / s, mask_rect.y0 / s, mask_rect.x1 / s, mask_rect.y1 / s};
  if (kind == MaskErrorKind::region) {
    return out.x0 < out.x1 && out.y0 < out.y1 ? std::optional{out} : std::nullopt;
  }
  // areaCifCheck: a zero extent is widened by one unit, downwards below
  // zero and upwards otherwise.
  widen(out.x0, out.x1);
  widen(out.y0, out.y1);
  return out;
}

bool collapses_when_truncated(const db::Rect& mask_rect, std::int32_t scale) noexcept {
  const db::Coord s = positive_scale(scale);
  return mask_rect.x0 / s >= mask_rect.x1 / s || mask_rect.y0 / s >= mask_rect.y1 / s;
}

CheckOutcome mask_layer_check(const CheckInput& input, const db::Rect& check_area, const db::Rect& clip_area,
                              ErrorSink& sink) {
  const StyleDetails* details = input.rules != nullptr ? input.rules->details() : nullptr;
  if (details == nullptr || !details->cif_bound || details->mask_layer_rules.empty() || input.cif == nullptr ||
      input.cell == nullptr || input.technology == nullptr || check_area.is_null()) {
    return CheckOutcome::completed;  // drcCifValid FALSE, or nothing to check
  }
  const std::shared_ptr<const mask::OutputStyle> style = input.cif->output_style(details->cif_style);
  if (!style) {
    return CheckOutcome::completed;
  }
  MaskRuleCache local_rules;
  const MaskRuleCache& compiled =
      compiled_rules(input.mask_rules != nullptr ? *input.mask_rules : local_rules, *details, style);
  const MaskRules& rules = compiled.rules;
  if (rules.empty()) {
    return CheckOutcome::completed;
  }
  mask::GenerationContext local;
  mask::GenerationContext& ctx = input.generation != nullptr ? *input.generation : local;
  mask::GenerateOptions options;
  options.layers = compiled.layers;
  options.report_errors = false;  // drcCifCheck leaves CIFErrorDef as it is; Hefesto adds no feedback
  const mask::GenerationSource source{input.technology, input.compat, {}};
  const mask::LayerPlanes planes = mask::generate_area(source, *input.cell, input.cell, check_area, *style, ctx, options);

  const std::int32_t scale = positive_scale(style->scale.scale_factor);
  mask_detail::MaskChecker checker(input, planes, scale, mask_detail::scaled(check_area, scale), clip_area, sink);
  const CheckOutcome outcome = checker.run(rules);
  report_collapsed(ctx, checker.collapsed(), input.compat.magic_compat);
  return outcome;
}

}  // namespace hefesto::drc
