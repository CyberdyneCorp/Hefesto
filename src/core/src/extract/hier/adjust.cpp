// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pass 2 (extract-hierarchy "Merge records and pass-2 adjustments",
// "Hierarchical coupling records"). Ported from Magic 8.3.694 (fc5daa9e)
// extract/ExtSubtree.c (extSubtreeInteraction's second pass,
// extSubtreeAdjustInit, extSubtreeOutputCoupling) and extract/ExtHier.c
// (extHierAdjustments); Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include <string>
#include <utility>
#include <vector>

#include "hier_context.hpp"

namespace hefesto::extract::hier {

namespace {

void add_values(HierNode& node, const NodeRegion& np, double sign) {
  node.cap += sign * np.substrate_cap;
  for (std::size_t c = 0; c < node.classes.size() && c < np.classes.size(); ++c) {
    node.classes[c].area += static_cast<std::int64_t>(sign) * np.classes[c].area;
    node.classes[c].perimeter += static_cast<std::int64_t>(sign) * np.classes[c].perimeter;
  }
}

// extSubtreeAdjustInit.
void adjust_init(Hier& h) {
  for (const NodeRegion& np : h.cum.nodes) {
    if (const auto n = h.table.find(flat_node_name(np, h))) {
      add_values(h.table.node(*n), np, 1.0);
    }
  }
}

// NODETONODE: the node of `look` at the reference point of `np`.
std::optional<std::size_t> node_to_node(const FlatTree& look, const NodeRegion& np) {
  return node_at(look, tile_of_node(look, np));
}

void adjust_coupling(FlatTree& cum, const FlatTree& one) {
  for (const auto& [key, value] : one.coupling.pairs()) {
    const auto c1 = node_to_node(cum, one.nodes[key.first]);
    const auto c2 = node_to_node(cum, one.nodes[key.second]);
    if (!c1 || !c2 || *c1 == *c2) {
      continue;
    }
    cum.coupling.add(*c1, *c2, -value);
  }
}

// extSubtreeOutputCoupling.
void output_coupling(Hier& h) {
  const double cscale = h.env.style->cscale;
  const std::vector<std::pair<std::pair<std::size_t, std::size_t>, double>> pairs(h.cum.coupling.pairs().begin(),
                                                                                  h.cum.coupling.pairs().end());
  for (const auto& [key, value] : pairs) {
    const double cap = value / cscale;
    if (cap == 0.0) {
      continue;
    }
    const std::string a = *tile_to_node(h, tile_of_node(h.cum, h.cum.nodes[key.first]), h.cum, true);
    const std::string b = *tile_to_node(h, tile_of_node(h.cum, h.cum.nodes[key.second]), h.cum, true);
    h.writer->cap(a, b, cap, h.cap_class);
  }
}

}  // namespace

void adjust_tree(Hier& h, FlatTree& cum, FlatTree& one, FlatTree& look) {
  if (h.env.options->coupling) {
    adjust_coupling(cum, one);
  }
  for (const NodeRegion& np : one.nodes) {
    if (np.plane >= look.def->plane_count()) {
      continue;  // an orphaned node
    }
    const TileRef tp = tile_of_node(look, np);
    if (!node_at(look, tp) || side_type(tp.tile) == tech::kSpace) {
      continue;
    }
    const auto name = tile_to_node(h, tp, look, false);
    if (!name) {
      continue;
    }
    if (const auto n = h.table.find(*name)) {
      add_values(h.table.node(*n), np, -1.0);
    }
  }
}

void adjust_interaction(Hier& h) {
  extract_flat(h.cum, h.env, h.clip_area, true, h.env.options->coupling);
  h.sub_use = nullptr;
  adjust_init(h);
  for (const auto& one : h.subs) {
    adjust_tree(h, h.cum, *one, h.cum);
  }
  if (h.env.options->coupling) {
    output_coupling(h);
  }
}

}  // namespace hefesto::extract::hier
