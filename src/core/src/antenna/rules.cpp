// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RuleView (antenna/antenna.hpp): the antenna rules, model flags, tie-down
// and gate sets, plane order and thickness of one extraction style
// (capability antenna-check "Antenna tech-file keywords grammar", "Antenna
// coefficient unit conversion", "Shared antenna definitions for LEF
// output"). EXTTECH parses and converts them (extract/tech/
// antenna_rules.cpp, conversion.cpp); this is the read side.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c (exts_antennaRatio
// defaults, ExtGetGateTypesMask, ExtGetDiffTypesMask), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: ANTENNA.

#include <cstddef>

#include "hefesto/antenna/antenna.hpp"
#include "hefesto/extract/tech/style_view.hpp"

namespace hefesto::antenna {

RuleView RuleView::from_style(const extract::ExtStyle& style) {
  RuleView view;
  view.rules_ = style.antenna;
  view.tiedown_ = style.tiedown;
  view.gates_ = style.gate_types();
  view.model_ = style.antenna_model;
  view.plane_ordering_ = style.plane_ordering;
  view.plane_order_ = style.plane_order;
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    view.thickness_[t] = style.heights[t].thickness;
  }
  return view;
}

RuleView RuleView::from_view(const extract::StyleView& style) {
  RuleView view;
  view.gates_ = style.gate_types().value_or(tech::TypeMask{});
  view.tiedown_ = style.diff_types().value_or(tech::TypeMask{});
  return view;
}

const extract::AntennaRule* RuleView::find(tech::TileType type) const noexcept {
  for (const extract::AntennaRule& rule : rules_) {
    if (rule.type == type) {
      return &rule;
    }
  }
  return nullptr;
}

std::optional<extract::AntennaRule> RuleView::rules(tech::TileType type) const {
  const extract::AntennaRule* rule = find(type);
  return rule != nullptr ? std::optional{*rule} : std::nullopt;
}

extract::AntennaModel RuleView::calc_type(tech::TileType type) const noexcept {
  const extract::AntennaRule* rule = find(type);
  return rule != nullptr ? rule->model : extract::AntennaModel::none;
}

float RuleView::ratio_gate(tech::TileType type) const noexcept {
  const extract::AntennaRule* rule = find(type);
  return rule != nullptr ? static_cast<float>(rule->ratio_gate) : 0.0F;
}

// ratio_diff[0] is ratioDiffB (+inf for `none`), [1] ratioDiffA; a line
// that failed before storing them leaves Magic's zero defaults.
float RuleView::ratio_diff_const(tech::TileType type) const noexcept {
  const extract::AntennaRule* rule = find(type);
  return rule != nullptr && !rule->ratio_diff.empty() ? static_cast<float>(rule->ratio_diff[0]) : 0.0F;
}

float RuleView::ratio_diff_prop(tech::TileType type) const noexcept {
  const extract::AntennaRule* rule = find(type);
  return rule != nullptr && rule->ratio_diff.size() > 1 ? static_cast<float>(rule->ratio_diff[1]) : 0.0F;
}

std::int32_t RuleView::plane_order(tech::PlaneId plane) const noexcept {
  return plane < plane_order_.size() ? plane_order_[plane] : -1;
}

float RuleView::thickness(tech::TileType type) const noexcept {
  return type < tech::kMaxTileTypes ? thickness_[type] : 0.0F;
}

}  // namespace hefesto::antenna
