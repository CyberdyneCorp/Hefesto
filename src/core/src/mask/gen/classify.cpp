// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer classes of hierarchical generation (capability
// mask-hierarchical-generation "Classification of hierarchically processed
// layers", "Non-local layers", "Interaction search grid"; design D8).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFtech.c (CIFTechFinal,
// :2051-2238) and cif/CIFhier.c (CIFGenSubcells' step size), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: GEN.

#include "hefesto/mask/gen/classify.hpp"

#include <algorithm>

#include "hefesto/db/geometry.hpp"
#include "hefesto/mask/ops/evaluate.hpp"

namespace hefesto::mask {

namespace {

bool is_bloat_typed(OpKind kind) noexcept {
  return kind == OpKind::bloat_or || kind == OpKind::bloat_max || kind == OpKind::bloat_min;
}

// The operators that force a layer into HierLayers.
bool forces_hierarchy(OpKind kind, bool magic_compat) noexcept {
  switch (kind) {
    case OpKind::op_and:
    case OpKind::and_not:
    case OpKind::interacting:
    case OpKind::shrink:
    case OpKind::close:
    case OpKind::bridge:
    case OpKind::bridge_lim:
    case OpKind::bloat_or:
    case OpKind::bloat_max:
    case OpKind::bloat_min:
    case OpKind::bloat_all:
      return true;
    case OpKind::grow_min:  // Q-MASK-405 OFF
    case OpKind::not_square:
    case OpKind::orthogonal:
    case OpKind::maxrect:
      return !magic_compat;
    default:
      return false;
  }
}

void set_layers(std::vector<bool>& set, const std::vector<std::size_t>& layers) {
  for (const std::size_t l : layers) {
    if (l < set.size()) {
      set[l] = true;
    }
  }
}

// The extra yank types and dependencies of a bloat operator.
void add_bloat(const LayerOp& op, tech::TypeMask& yank, std::vector<bool>& depend) {
  if (!op.bloat) {
    return;
  }
  const BloatTable& b = *op.bloat;
  for (std::size_t j = 0; j < b.distance.size(); ++j) {
    const auto t = static_cast<tech::TileType>(j);
    if (is_bloat_typed(op.kind) && b.distance[j] != b.distance[tech::kSpace]) {
      yank.set(t);
    } else if (op.kind == OpKind::bloat_all && b.distance[j] != 0) {
      if (b.plane < 0) {
        if (j < depend.size()) {
          depend[j] = true;
        }
      } else {
        yank.set(t);
      }
    }
  }
}

struct LayerReads {
  std::vector<bool> depend;
  tech::TypeMask yank;
  bool forced = false;
};

LayerReads layer_reads(const OutputLayer& layer, std::size_t count, bool magic_compat) {
  LayerReads r;
  r.depend.assign(count, false);
  for (const LayerOp& op : layer.ops) {
    set_layers(r.depend, op.cif_layers);
    r.yank |= op.types;
    add_bloat(op, r.yank, r.depend);
    r.forced = r.forced || forces_hierarchy(op.kind, magic_compat);
  }
  return r;
}

bool reads_any(const OutputLayer& layer, const std::vector<bool>& set) {
  return std::ranges::any_of(layer.ops, [&](const LayerOp& op) {
    return std::ranges::any_of(op.cif_layers, [&](std::size_t l) { return l < set.size() && set[l]; });
  });
}

bool has_op(const OutputLayer& layer, OpKind a, OpKind b) {
  return std::ranges::any_of(layer.ops, [&](const LayerOp& op) { return op.kind == a || op.kind == b; });
}

// The backward pass; returns the per-layer yank masks (layerYank).
std::vector<tech::TypeMask> backward_pass(const OutputStyle& style, bool magic_compat, LayerClasses& out) {
  const std::size_t n = style.layers.size();
  std::vector<tech::TypeMask> layer_yank(n);
  for (std::size_t i = n; i-- > 0;) {
    LayerReads r = layer_reads(style.layers[i], n, magic_compat);
    if (out.hier[i] || r.forced) {
      out.yank |= r.yank;
      out.hier[i] = true;
      for (std::size_t j = 0; j < n; ++j) {
        out.hier[j] = out.hier[j] || r.depend[j];
      }
    }
    layer_yank[i] = r.yank;
  }
  return layer_yank;
}

void nonlocal_sets(const OutputStyle& style, const std::vector<tech::TypeMask>& layer_yank, LayerClasses& out) {
  const std::size_t n = style.layers.size();
  for (std::size_t i = 0; i < n; ++i) {
    out.nonlocal[i] = has_op(style.layers[i], OpKind::maxrect, OpKind::maxrect) || reads_any(style.layers[i], out.nonlocal);
  }
  out.nonlocal_gen = out.nonlocal;
  for (std::size_t i = n; i-- > 0;) {
    if (!out.nonlocal_gen[i]) {
      continue;
    }
    for (const LayerOp& op : style.layers[i].ops) {
      set_layers(out.nonlocal_gen, op.cif_layers);
    }
    out.nonlocal_yank |= layer_yank[i];
  }
}

bool seeds(OpKind kind) noexcept {
  return kind == OpKind::op_or || kind == OpKind::bloat_or || kind == OpKind::bloat_max ||
         kind == OpKind::bloat_min || kind == OpKind::bloat_all;
}

void seed_types(const OutputStyle& style, LayerClasses& out) {
  const std::size_t n = style.layers.size();
  std::vector<bool> seed_layers(n, false);
  for (std::size_t i = 0; i < n; ++i) {
    seed_layers[i] = has_op(style.layers[i], OpKind::maxrect, OpKind::maxrect);
  }
  for (std::size_t i = n; i-- > 0;) {
    if (!seed_layers[i]) {
      continue;
    }
    for (const LayerOp& op : style.layers[i].ops) {
      if (seeds(op.kind)) {
        out.nonlocal_seed |= op.types;
        set_layers(seed_layers, op.cif_layers);
      }
    }
  }
}

// Q-MASK-403: ON removes the bbox/boundary layers only; OFF also every
// layer reading a removed layer, transitively (one forward pass suffices,
// layers read only earlier layers).
void removal_pass(const OutputStyle& style, bool magic_compat, LayerClasses& out) {
  std::vector<bool> removed(style.layers.size(), false);
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    const OutputLayer& layer = style.layers[i];
    removed[i] = has_op(layer, OpKind::boundary, OpKind::bbox) || (!magic_compat && reads_any(layer, removed));
    if (removed[i]) {
      out.hier[i] = false;
    }
  }
}

}  // namespace

LayerClasses classify_layers(const OutputStyle& style, const compat::CompatSettings& compat) {
  const std::size_t n = style.layers.size();
  LayerClasses out;
  out.hier.assign(n, false);
  out.nonlocal.assign(n, false);
  const std::vector<tech::TypeMask> layer_yank = backward_pass(style, compat.magic_compat, out);
  nonlocal_sets(style, layer_yank, out);
  seed_types(style, out);
  for (std::size_t i = 0; i < n; ++i) {  // forward pass
    out.hier[i] = out.hier[i] || reads_any(style.layers[i], out.hier);
  }
  removal_pass(style, compat.magic_compat, out);
  return out;
}

std::int64_t interaction_step(const OutputStyle& style, std::int64_t radius) noexcept {
  if (style.step_size > 0) {
    return style.step_size;
  }
  return std::max<std::int64_t>(20 * std::min<std::int64_t>(radius, db::kInfinity), 50);
}

bool stops_hierarchically(const OutputStyle& style, std::size_t layer) noexcept {
  return layer < style.layers.size() && hierarchical_stop(style.layers[layer].ops).has_value();
}

}  // namespace hefesto::mask
