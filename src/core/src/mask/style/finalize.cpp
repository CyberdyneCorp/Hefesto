// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIFTechFinal ("Automatic stacked-contact inclusion", "CIF reducer",
// "Interaction radius"), CIFTechOutputScale ("Unit normalisation") and the
// contact cut size query ("Contact cut size query").
//
// Ported from Magic VLSI 8.3.694 cif/CIFtech.c (CIFTechFinal,
// cifComputeRadii, cifComputeHalo, CIFTechOutputScale, CIFGetContactSize),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Deviations (docs/decisions/cross-cutting.md X-2 tier 1): the reducer
// skips the interacting family (Q-MASK-12, fix) and does not read the
// slots data a second time as bloat data (undefined behavior in Magic);
// products saturate at the int range instead of overflowing.
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

#include "hefesto/db/geometry.hpp"
#include "mask/style/reader.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask::style_detail {

namespace {

using style_text::clamp_int;
using style_text::gcf;

// --- stacked contacts ---------------------------------------------------------

/// Every stacked contact type with the contact `type` as a component joins
/// `mask` (CIFTechFinal's DBResidueMask loop over the stacked types).
void join_stacked(const tech::Technology& technology, tech::TileType type, tech::TypeMask& mask) {
  for (const tech::ContactInfo& contact : technology.contacts()) {
    if (contact.stacked && (contact.component_a == type || contact.component_b == type)) {
      mask.set(contact.type);
    }
  }
}

void add_stacked_contacts(const tech::Technology& technology, OutputStyle& style) {
  for (OutputLayer& layer : style.layers) {
    for (LayerOp& op : layer.ops) {
      const tech::TypeMask before = op.types;
      for (std::size_t d = tech::kFirstUserType; d < technology.user_type_end(); ++d) {
        const auto type = static_cast<tech::TileType>(d);
        if (before.test(type) && technology.is_contact(type)) {
          join_stacked(technology, type, op.types);
        }
      }
    }
  }
}

// --- the reducer -----------------------------------------------------------------

bool is_halved_squares(std::size_t j) {
  return j == squares_value::size || j == squares_value::sep;
}

bool is_halved_slots(std::size_t j) {
  return j == slots_value::ssize || j == slots_value::ssep || j == slots_value::lsize || j == slots_value::lsep;
}

/// The client values of `op` that the reducer loop considers (zero values
/// are dropped by the caller).
std::vector<std::int64_t> reducer_values(const LayerOp& op) {
  std::vector<std::int64_t> out;
  switch (op.kind) {
    case OpKind::squares:
    case OpKind::slots:
      for (std::size_t j = 0; j < op.values.size(); ++j) {
        const bool halved = op.kind == OpKind::squares ? is_halved_squares(j) : is_halved_slots(j);
        out.push_back(halved ? op.values[j] >> 1 : op.values[j]);
      }
      break;
    case OpKind::squares_grid:
    case OpKind::bridge:
    case OpKind::bridge_lim: out.assign(op.values.begin(), op.values.end()); break;
    case OpKind::bloat_or:
    case OpKind::bloat_min:
    case OpKind::bloat_max:
    case OpKind::bloat_all: out.assign(op.bloat->distance.begin(), op.bloat->distance.end()); break;
    default: break;  // no client data, a flag or a name; interacting: Q-MASK-12
  }
  return out;
}

std::int32_t compute_reducer(const OutputStyle& style) {
  const std::int64_t scale = style.scale.scale_factor;
  std::int64_t min_reduce = scale;
  for (const OutputLayer& layer : style.layers) {
    for (const LayerOp& op : layer.ops) {
      if (op.distance > 0) {
        min_reduce = std::min(min_reduce, gcf(scale, op.distance));
      }
      for (const std::int64_t value : reducer_values(op)) {
        if (value != 0) {
          min_reduce = std::min(min_reduce, gcf(scale, value));
        }
      }
      if (min_reduce == 1) {
        break;  // the rest of this layer's operators
      }
    }
  }
  return clamp_int(min_reduce);
}

// --- CIFTechOutputScale ----------------------------------------------------------

bool has_odd_cut(const OutputStyle& style) {
  using namespace squares_value;
  for (const OutputLayer& layer : style.layers) {
    for (const LayerOp& op : layer.ops) {
      const auto& v = op.values;
      if (op.kind == OpKind::squares && (((v[size] | v[sep]) & 1) != 0)) {
        return true;
      }
      if (op.kind == OpKind::slots && (((v[slots_value::lsize] | v[slots_value::lsep] | v[slots_value::ssize] |
                                         v[slots_value::ssep]) &
                                        1) != 0)) {
        return true;
      }
    }
  }
  return false;
}

/// A rescaled distance, kept within the usable plane as the reader keeps
/// the distances it reads (the operators add it to plane coordinates in
/// 32 bits).
std::int32_t clamp_distance(std::int64_t v) {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(v, -db::kPlaneMax, db::kPlaneMax));
}

/// Multiplies one client value by d; returns the value folded into the
/// expander reduction, or nullopt for a zero value.
std::optional<std::int64_t> scale_value(OpKind kind, std::size_t j, std::int32_t& v, std::int64_t d, bool odd_space) {
  if (v == 0) {
    return std::nullopt;
  }
  const bool bridge = kind == OpKind::bridge || kind == OpKind::bridge_lim;
  v = bridge ? clamp_distance(v * d) : clamp_int(v * d);  // a bridge width is a distance
  std::int64_t folded = std::abs(std::int64_t{v});
  const bool cut = kind == OpKind::squares || kind == OpKind::slots;
  if (kind == OpKind::squares ? is_halved_squares(j) : (kind == OpKind::slots && is_halved_slots(j))) {
    folded >>= 1;  // half-grid
  }
  if (cut && odd_space) {
    folded >>= 1;  // force half-grid
  }
  return folded;
}

/// The client values one operator scales: (values, bloat table).
bool scales_values(OpKind kind) {
  switch (kind) {
    case OpKind::squares:
    case OpKind::squares_grid:
    case OpKind::slots:
    case OpKind::bridge:
    case OpKind::bridge_lim: return true;
    default: return false;
  }
}

/// Multiplies every operand of `op` by d and folds them into `lexpand`.
void scale_op(LayerOp& op, std::int64_t d, bool odd_space, std::int64_t expander, std::int64_t& lexpand) {
  const auto fold = [&](std::int64_t value) { lexpand = gcf(lexpand, gcf(value, expander)); };
  if (op.distance != 0) {
    op.distance = clamp_distance(op.distance * d);
    fold(std::abs(std::int64_t{op.distance}));
  }
  if (scales_values(op.kind)) {
    for (std::size_t j = 0; j < op.values.size(); ++j) {
      if (const auto value = scale_value(op.kind, j, op.values[j], d, odd_space)) {
        fold(*value);
      }
    }
  }
  if (op.bloat) {
    for (std::int32_t& v : op.bloat->distance) {
      if (v != 0) {
        v = clamp_distance(std::int64_t{v} * d);
        fold(std::abs(std::int64_t{v}));
      }
    }
  }
}

void divide_op(LayerOp& op, std::int64_t lexpand) {
  op.distance = static_cast<std::int32_t>(op.distance / lexpand);
  if (scales_values(op.kind)) {
    for (std::int32_t& v : op.values) {
      v = static_cast<std::int32_t>(v / lexpand);
    }
  }
  if (op.bloat && op.kind != OpKind::bloat_all) {  // bloat-all flags are not divided (Q-MASK-13)
    for (std::int32_t& v : op.bloat->distance) {
      v = static_cast<std::int32_t>(v / lexpand);
    }
  }
}

// --- the halo -----------------------------------------------------------------

struct Radii {
  std::int64_t grow = 0;
  std::int64_t shrink = 0;
};

/// cifComputeRadii for one layer, given the radii of the earlier layers.
Radii layer_radii(const OutputLayer& layer, const std::vector<Radii>& earlier) {
  Radii r;
  for (const LayerOp& op : layer.ops) {
    if (op.kind == OpKind::bbox || op.kind == OpKind::net || op.kind == OpKind::tagged ||
        op.kind == OpKind::mask_hints) {
      break;  // never used hierarchically
    }
    for (const std::size_t i : op.cif_layers) {
      if (i < earlier.size()) {
        r.grow = std::max(r.grow, earlier[i].grow);
        r.shrink = std::max(r.shrink, earlier[i].shrink);
      }
    }
    if (op.kind == OpKind::grow || op.kind == OpKind::grow_min || op.kind == OpKind::grow_grid) {
      r.grow += op.distance;
    } else if (op.kind == OpKind::shrink) {
      r.shrink += op.distance;
    } else if (op.kind == OpKind::bloat_or) {
      std::int64_t cur_grow = 0;
      std::int64_t cur_shrink = 0;
      for (const std::int32_t d : op.bloat->distance) {
        if (d > cur_grow) {
          cur_grow = d;
        } else if (-std::int64_t{d} > cur_shrink) {
          cur_shrink = -std::int64_t{d};
        }
      }
      r.grow += cur_grow;
      r.shrink += cur_shrink;
    }
  }
  return r;
}

}  // namespace

ScaleApplied output_scale(OutputStyle& style, std::int64_t n, std::int64_t d, bool recompute_halo_always) {
  bool odd_space = false;
  if ((style.scale.scale_factor & 1) != 0) {
    n *= 2;
    d *= 2;
    odd_space = true;
  } else if ((d & 1) != 0 && has_odd_cut(style)) {
    n *= 2;
    d *= 2;
  }
  style.scale.scale_factor = clamp_int(style.scale.scale_factor * n);
  style.scale.expander = clamp_int(style.scale.expander * d);
  const std::int64_t expander = style.scale.expander;
  std::int64_t lexpand = expander;
  for (OutputLayer& layer : style.layers) {
    for (LayerOp& op : layer.ops) {
      scale_op(op, d, odd_space, expander, lexpand);
    }
  }
  // Magic takes the smaller of the operand GCD and gcd(scale, expander),
  // not their GCD, so the scale can be truncated (Q-MASK-29).
  lexpand = std::min(lexpand, gcf(style.scale.scale_factor, expander));
  ScaleApplied applied{clamp_int(d), lexpand > 1 ? clamp_int(lexpand) : 1};
  if (lexpand <= 1) {
    if (recompute_halo_always) {
      style.radius = style.compute_halo();  // Q-MASK-18, magic-compat OFF
    }
    return applied;
  }
  if (style.scale.scale_factor % lexpand != 0) {
    applied.rounded_from = style.scale.scale_factor;
  }
  style.scale.scale_factor = static_cast<std::int32_t>(style.scale.scale_factor / lexpand);
  style.scale.expander = static_cast<std::int32_t>(style.scale.expander / lexpand);
  for (OutputLayer& layer : style.layers) {
    for (LayerOp& op : layer.ops) {
      divide_op(op, lexpand);
    }
  }
  style.radius = style.compute_halo();
  return applied;
}

namespace {

bool uses_bloat_all(const OutputStyle& style) {
  return std::ranges::any_of(style.layers, [](const OutputLayer& layer) {
    return std::ranges::any_of(layer.ops, [](const LayerOp& op) { return op.kind == OpKind::bloat_all; });
  });
}

}  // namespace

std::optional<std::string> grid_expander_warning(const OutputStyle& style) {
  // Only bloat-all keeps a style from reducing, so only such a style can
  // pair a PDK gridlimit with an expander that is not 10 on purpose; the
  // reducing styles (sky130 and IHP density, gf180 metfill) stay quiet
  // (Q-MASK-8, amended 2026-11-14).
  if (style.grid_limit == 0 || style.scale.expander == 10 || !uses_bloat_all(style)) {
    return std::nullopt;
  }
  return "gridlimit " + std::to_string(style.grid_limit) + " of output style \"" + style.name +
         "\" is applied in mask units of expander " + std::to_string(style.scale.expander) + ", not 10 (Q-MASK-8).";
}

std::optional<std::string> scale_rounding_warning(const OutputStyle& style, const ScaleApplied& applied) {
  if (applied.rounded_from == 0) {
    return std::nullopt;
  }
  return "the scale factor of output style \"" + style.name + "\" was rounded: " +
         std::to_string(applied.rounded_from) + " is not a multiple of the common factor " +
         std::to_string(applied.reduced) + ", so it became " + std::to_string(style.scale.scale_factor) +
         " (Q-MASK-29).";
}

void check_grid_expander(const OutputStyle& style, Messages& messages) {
  if (auto text = grid_expander_warning(style)) {
    messages.native(kGridExpander, std::move(*text));
  }
}

void finalize(const tech::Technology* technology, OutputStyle& style, Messages& messages) {
  const ScaleApplied applied = output_scale(style, 1, 1, false);
  if (auto text = scale_rounding_warning(style, applied)) {
    messages.native(kScaleRounded, std::move(*text));
  }
  if (style.scale.scale_factor <= 0) {
    messages.error("No valid scale factor was given for " + style.name + " CIF.");
    style.scale.scale_factor = 1;
    return;
  }
  if (technology != nullptr) {
    add_stacked_contacts(*technology, style);
  }
  style.scale.reducer = compute_reducer(style);
  style.radius = style.compute_halo();
  check_grid_expander(style, messages);
}

}  // namespace hefesto::mask::style_detail

namespace hefesto::mask {

std::int64_t OutputStyle::compute_halo() const noexcept {
  std::vector<style_detail::Radii> radii;
  radii.reserve(layers.size());
  std::int64_t max_grow = 0;
  std::int64_t max_shrink = 0;
  for (const OutputLayer& layer : layers) {
    radii.push_back(style_detail::layer_radii(layer, radii));
    max_grow = std::max(max_grow, radii.back().grow);
    max_shrink = std::max(max_shrink, radii.back().shrink);
  }
  std::int64_t r = max_grow > max_shrink ? 2 * max_grow : 2 * max_shrink;
  if (scale.scale_factor > 0) {
    r /= scale.scale_factor;
  }
  return r + 1;
}

std::int64_t OutputStyle::halo() const noexcept {
  return radius;
}

std::optional<OutputStyle::ContactCut> OutputStyle::contact_cut(tech::TileType type) const noexcept {
  for (const OutputLayer& layer : layers) {
    // The leading run of `or` operators without mask layers.
    for (std::size_t i = 0; i < layer.ops.size(); ++i) {
      const LayerOp& op = layer.ops[i];
      if (op.kind != OpKind::op_or || !op.cif_layers.empty()) {
        break;
      }
      if (!op.types.test(type)) {
        continue;
      }
      for (std::size_t k = i + 1; k < layer.ops.size(); ++k) {
        const LayerOp& sop = layer.ops[k];
        if (sop.kind == OpKind::squares || sop.kind == OpKind::squares_grid || sop.kind == OpKind::slots) {
          // squares: border, size, sep; slots: sborder, ssize, ssep (same indices)
          return ContactCut{sop.values[squares_value::size], sop.values[squares_value::border],
                            sop.values[squares_value::sep]};
        }
        if (sop.kind != OpKind::op_or && sop.kind != OpKind::and_not) {
          break;  // the relation between the layer and its cuts is broken
        }
      }
    }
  }
  return std::nullopt;
}

std::optional<std::int64_t> OutputStyle::contact_size(tech::TileType type) const noexcept {
  const auto cut = contact_cut(type);
  return cut ? std::optional{cut->size()} : std::nullopt;
}

}  // namespace hefesto::mask
