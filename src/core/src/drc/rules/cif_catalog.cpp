// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::CifCatalog: the cifoutput styles as DRC needs them (see
// cif_catalog.hpp), a view over mask::OutputStyle: the style list and each
// style's scale come from mask::read_output_styles, the contact borders
// from OutputStyle::contact_cut (CIFGetContactSize), and rescale() is
// mask::rescale_step (CIFTechOutputScale).
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (drcGetContactBorder),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2); the re-sourcing onto
// mask::OutputStyle: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/drc/rules/cif_catalog.hpp"

#include <algorithm>
#include <string>

#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::drc {

namespace {

using Kind = CifOpValues::Kind;
using mask::OpKind;

Kind values_kind(OpKind kind) {
  switch (kind) {
    case OpKind::squares: return Kind::squares;
    case OpKind::squares_grid: return Kind::squares_grid;
    case OpKind::slots: return Kind::slots;
    case OpKind::bridge:
    case OpKind::bridge_lim: return Kind::bridge;
    case OpKind::bloat_or:
    case OpKind::bloat_min:
    case OpKind::bloat_max: return Kind::bloat;
    case OpKind::bloat_all: return Kind::bloat_all;
    default: return Kind::plain;
  }
}

/// The distinct non-zero bloat distances (bloat-or, -min, -max), or the
/// bloat-all flag 1 when any type or layer is selected.
std::vector<std::int32_t> bloat_values(const mask::LayerOp& op) {
  std::vector<std::int32_t> values;
  for (const std::int32_t d : op.bloat->distance) {
    if (d != 0 && std::find(values.begin(), values.end(), d) == values.end()) {
      values.push_back(d);
    }
  }
  if (op.kind == OpKind::bloat_all && !values.empty()) {
    values = {values.front()};
  }
  return values;
}

CifOpValues op_view(const mask::LayerOp& op) {
  CifOpValues out;
  out.kind = values_kind(op.kind);
  out.distance = op.distance;
  if (op.bloat) {
    out.values = bloat_values(op);
  } else {
    out.values = op.values;
  }
  if (op.kind == OpKind::op_or && op.cif_layers.empty()) {
    out.or_paint = op.types;
  }
  out.keeps_cuts = op.kind == OpKind::op_or || op.kind == OpKind::and_not;
  return out;
}

/// drcGetContactBorder: the squares (or slots) border of `type`, over the
/// style's scale factor, when positive.
std::optional<ContactBorder> border_of(const mask::OutputStyle& style, tech::TileType type) {
  const auto cut = style.contact_cut(type);
  if (!cut || cut->border <= 0) {
    return std::nullopt;
  }
  return ContactBorder{cut->border, style.scale.scale_factor};
}

CifStyleInfo style_view(const mask::OutputStyle& style, const std::vector<bool>& contacts) {
  CifStyleInfo info;
  info.name = style.name;
  info.scale_factor = style.scale.scale_factor;
  info.expander = style.scale.expander;
  info.reducer = style.scale.reducer;
  for (const mask::OutputLayer& layer : style.layers) {
    info.layers.push_back(layer.name);
    std::vector<CifOpValues> ops;
    ops.reserve(layer.ops.size());
    for (const mask::LayerOp& op : layer.ops) {
      ops.push_back(op_view(op));
    }
    info.ops.push_back(std::move(ops));
  }
  info.contact_borders.assign(contacts.size(), std::nullopt);
  for (std::size_t t = 0; t < contacts.size(); ++t) {
    if (contacts[t]) {
      info.contact_borders[t] = border_of(style, static_cast<tech::TileType>(t));
    }
  }
  return info;
}

}  // namespace

CifCatalog CifCatalog::from_technology(const tech::Technology& technology, diag::DiagnosticSink& /*sink*/) {
  CifCatalog catalog;
  diag::NullSink quiet;
  const mask::OutputStyleSet set = mask::read_output_styles(technology, quiet);
  std::vector<bool> contacts(technology.type_count(), false);
  for (std::size_t t = 0; t < contacts.size(); ++t) {
    contacts[t] = technology.is_contact(static_cast<tech::TileType>(t));
  }
  for (const auto& style : set.base) {
    catalog.styles_.push_back(style_view(*style, contacts));
    catalog.sources_.styles.push_back(style);
  }
  if (!catalog.styles_.empty()) {
    catalog.current_ = 0;
  }
  return catalog;
}

const CifStyleInfo* CifCatalog::current() const noexcept {
  return current_ && *current_ < styles_.size() ? &styles_[*current_] : nullptr;
}

diag::Status CifCatalog::select(std::string_view name) {
  const auto it = std::find_if(styles_.begin(), styles_.end(), [&](const CifStyleInfo& s) { return s.name == name; });
  if (it == styles_.end()) {
    return diag::make("DRC-TECH-003", "\"" + std::string{name} + "\" is not one of the CIF output styles.");
  }
  current_ = static_cast<std::size_t>(it - styles_.begin());
  return diag::Done{};
}

std::optional<ContactBorder> CifCatalog::contact_border(tech::TileType type) const noexcept {
  const CifStyleInfo* style = current();
  if (style == nullptr || type >= style->contact_borders.size()) {
    return std::nullopt;
  }
  return style->contact_borders[type];
}

std::int32_t CifCatalog::default_step_size() const noexcept {
  const CifStyleInfo* style = current();
  if (style == nullptr || style->scale_factor <= 0) {
    return 64;
  }
  // Magic's 6400 / cs_scaleFactor is 0 above 6400, and the checker divides
  // by the step size: never below 1 (Q-DRC-25, fix in both modes).
  return std::max(1, 6400 / style->scale_factor);
}

std::pair<std::int32_t, std::int32_t> CifCatalog::rescale(std::int32_t n, std::int32_t d) {
  std::pair<std::int32_t, std::int32_t> factor{1, 1};
  for (std::size_t i = 0; i < styles_.size() && i < sources_.styles.size(); ++i) {
    const mask::RescaleStep step = mask::rescale_step(*sources_.styles[i], n, d, compat::CompatSettings{});
    sources_.styles[i] = step.style;
    std::vector<bool> contacts(styles_[i].contact_borders.size(), false);
    for (std::size_t t = 0; t < contacts.size(); ++t) {
      contacts[t] = styles_[i].contact_borders[t].has_value();  // a border never appears or vanishes
    }
    styles_[i] = style_view(*step.style, contacts);
    if (current_ && *current_ == i) {
      factor = {step.multiplier, step.reduced};
    }
  }
  return factor;
}

}  // namespace hefesto::drc
