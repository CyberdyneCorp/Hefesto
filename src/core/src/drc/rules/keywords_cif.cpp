// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The mask-layer statements of the drc section (`cifstyle`, `cifwidth`,
// `cifspacing`, `cifarea`, `cifmaxwidth`): parsed, bound to the cifoutput
// style, why texts registered and halo contributions returned in internal
// units, as Magic does; the compiled rules are kept in StyleDetails and
// checked by drc/check/mask_check.cpp (openspec/changes/m5-mask-gds-cif
// design D14; capability drc-cif-rules).
//
// Ported from Magic VLSI 8.3.694 drc/DRCcif.c (drcCifSetStyle,
// drcCifWarning, drcCifWidth, drcCifSpacing, drcCifArea, drcCifMaxwidth),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Deviation: Magic keeps the cifstyle binding (drcCifStyle) across style
// loads; Hefesto starts every load unbound, so a style's mask-layer rules
// depend only on its own `cifstyle` line.
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include <string>
#include <utility>

#include "drc/rules/compiler.hpp"
#include "drc/rules/text_util.hpp"

namespace hefesto::drc::rules_detail {

namespace {

/// (value + scale - 1) / scale in C integer arithmetic.
int cif_halo(std::int32_t value, std::int32_t scale) {
  if (scale == 0) {
    return 0;  // a scale of 0 never survives CIFTechFinal; guard the division
  }
  return text::clamp_int((std::int64_t{value} + scale - 1) / scale);
}

}  // namespace

int StyleCompiler::kw_cif_style(Tokens t) {
  for (const CifStyleInfo& style : cif_.styles()) {
    if (style.name != t[1]) {
      continue;
    }
    details_.cif_style = style.name;
    const CifStyleInfo* current = cif_.current();
    if (current != nullptr && current->name == style.name) {
      drc_cif_style_ = current;
      details_.cif_bound = true;
    } else {
      drc_cif_style_ = nullptr;
      details_.cif_bound = false;
      cif_warned_ = true;  // post no more error messages
    }
    return 0;
  }
  tech_error("Unknown DRC cifstyle " + t[1]);
  return 0;
}

int StyleCompiler::cif_missing_style() {
  if (!cif_warned_) {
    tech_error("Missing cif style for drc\n\tThis message will not be repeated.");
    cif_warned_ = true;
  }
  return 0;
}

std::optional<std::size_t> StyleCompiler::cif_layer(std::string_view name) const {
  const auto& layers = drc_cif_style_->layers;
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (layers[i] == name) {
      return i;
    }
  }
  return std::nullopt;
}

void StyleCompiler::add_mask_rule(MaskLayerRule rule) {
  rule.exception = exception_;
  details_.mask_layer_rules.push_back(std::move(rule));
}

namespace {

MaskLayerRule mask_rule(std::string keyword, std::string layer, std::string layer2, std::int32_t distance,
                        std::int32_t value2, WhyIndex why) {
  MaskLayerRule rule;
  rule.keyword = std::move(keyword);
  rule.layer = std::move(layer);
  rule.layer2 = std::move(layer2);
  rule.distance = distance;
  rule.value2 = value2;
  rule.why = why;
  return rule;
}

}  // namespace

int StyleCompiler::kw_cif_width(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[2]);
  const WhyIndex reason = why(t[3]);
  if (drc_cif_style_ == nullptr) {
    return cif_missing_style();
  }
  if (!cif_layer(t[1])) {
    tech_error("Unknown cif layer: " + t[1]);
    return 0;
  }
  add_mask_rule(mask_rule("cifwidth", t[1], "", distance, 0, reason));
  return cif_halo(distance, drc_cif_style_->scale_factor);
}

int StyleCompiler::kw_cif_spacing(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[3]);
  const WhyIndex reason = why(t[5]);
  if (drc_cif_style_ == nullptr) {
    return cif_missing_style();
  }
  std::size_t layer[2] = {0, 0};
  for (std::size_t k = 0; k < 2; ++k) {
    const auto found = cif_layer(t[1 + k]);
    if (!found) {
      tech_error("Unknown cif layer: " + t[1 + k]);
      return 0;
    }
    layer[k] = *found;
  }
  const std::string& adjacency = t[4];
  const bool touching_ok = adjacency == "touching_ok";
  if (touching_ok && layer[0] != layer[1]) {
    tech_error("Spacing check with touching ok must all be in one plane.");
    return 0;
  }
  if (!touching_ok && adjacency != "touching_illegal") {
    tech_error("Badly formed drc spacing line");
    return 0;
  }
  MaskLayerRule rule = mask_rule("cifspacing", t[1], t[2], distance, 0, reason);
  rule.touching_ok = touching_ok;
  add_mask_rule(std::move(rule));
  return cif_halo(distance, drc_cif_style_->scale_factor);
}

int StyleCompiler::kw_cif_area(Tokens t) {
  const std::int32_t area = text::c_atoi(t[2]);
  const std::int32_t horizon = text::c_atoi(t[3]);
  const WhyIndex reason = why(t[4]);
  if (drc_cif_style_ == nullptr) {
    return cif_missing_style();
  }
  if (!cif_layer(t[1])) {
    tech_error("Unknown cif layer: " + t[1]);
    return 0;
  }
  add_mask_rule(mask_rule("cifarea", t[1], "", area, horizon, reason));
  return cif_halo(horizon, drc_cif_style_->scale_factor);
}

int StyleCompiler::kw_cif_maxwidth(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[2]);
  const WhyIndex reason = why(t[4]);
  if (drc_cif_style_ == nullptr) {
    return cif_missing_style();
  }
  if (!cif_layer(t[1])) {
    tech_error("Unknown cif layer: " + t[1]);
    return 0;
  }
  RuleFlags bend = 0;
  if (t[3] == "bend_ok") {
    bend = flags::kBends;
  } else if (t[3] == "both") {
    bend = flags::kMaxWidthBoth;
  } else if (t[3] != "bend_illegal") {
    tech_error("unknown bend option " + t[3]);
    return 0;
  }
  MaskLayerRule rule = mask_rule("cifmaxwidth", t[1], "", distance, 0, reason);
  rule.flags = bend;
  add_mask_rule(std::move(rule));
  return cif_halo(distance, drc_cif_style_->scale_factor);
}

}  // namespace hefesto::drc::rules_detail
