// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The flattened subtrees of hierarchical extraction (Magic's ExtTree):
// their node passes and node names. Ported from Magic 8.3.694 (fc5daa9e)
// extract/ExtHier.c (extHierNewOne, extHierFreeOne, extHierLabFirst),
// extract/ExtBasic.c (extFindNodes, extNodeName, extNodeToTile) and
// extract/ExtSubtree.c; Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include <utility>

#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/hier/yank.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hier_context.hpp"

namespace hefesto::extract::hier {

std::unique_ptr<FlatTree> new_flat(const Env& /*env*/, std::size_t plane_count) {
  auto flat = std::make_unique<FlatTree>();
  flat->owned = new_yank_cell("__EXTTREE__", plane_count);
  flat->def = flat->owned.get();
  return flat;
}

void drop_nodes(FlatTree& flat) {
  if (flat.finder) {
    reset(*flat.def);
    flat.finder.reset();
  }
  flat.nodes.clear();
}

void clear_flat(FlatTree& flat) {
  drop_nodes(flat);
  flat.coupling = CouplingTable{};
  clear_yank_cell(*flat.def);
  flat.look_names = nullptr;
  flat.real_use = nullptr;
}

namespace {

NodePassOptions pass_options(const FlatTree& flat, const Env& env, const db::Rect& clip) {
  NodePassOptions o;
  o.clip = clip;
  o.resistance = env.options->lumped;
  o.magic_compat = env.compat->magic_compat;
  const SubstrateSeeds seeds = substrate_seeds(*flat.def, *env.tech, *env.style, env.options->extresist);
  for (const auto& [plane, tile] : seeds.tiles) {
    o.substrate_seeds.push_back(RegionSeed{plane, {tile.area.x0, tile.area.y0}, tile.right_side});
  }
  if (seeds.tiles.empty() && seeds.tileless) {
    o.substrate_without_tiles = env.style->substrate.plane;
  }
  return o;
}

CapInputs cap_inputs(const FlatTree& flat, const Env& env, const db::Rect& clip) {
  CapInputs in;
  in.cell = flat.def;
  in.style = env.style;
  in.compat = env.compat;
  in.technology = env.tech;
  in.options = env.options;
  in.clip = clip;
  in.side_halo = env.side_halo;
  in.node_of = region_node_lookup(*flat.finder, *flat.def, flat.nodes.size());
  return in;
}

}  // namespace

void extract_flat(FlatTree& flat, const Env& env, const db::Rect& clip, bool labels, bool coupling) {
  drop_nodes(flat);
  flat.coupling = CouplingTable{};
  flat.finder = std::make_unique<RegionFinder>(*env.tech);
  flat.finder->keep_clients = true;
  auto found = flat.finder->find_nodes(*flat.def, *env.style, pass_options(flat, env, clip));
  if (!found.ok()) {
    return;
  }
  flat.nodes = std::move(found).value();
  const CapInputs in = cap_inputs(flat, env, clip);
  (void)substrate_caps(flat.nodes, in);
  if (coupling) {
    (void)find_coupling(flat.nodes, in, flat.coupling);
  }
  if (labels) {
    LabelPassOptions lo;
    lo.clip = clip;
    attach_labels(*flat.def, *flat.finder, flat.nodes, *env.style, *env.tech, lo, *env.compat);
  }
}

void label_regions(FlatTree& flat, const Env& env) {
  drop_nodes(flat);
  flat.finder = std::make_unique<RegionFinder>(*env.tech);
  flat.finder->keep_clients = true;
  RegionCallbacks callbacks;
  const ConnectTable connect{env.style->node_conn};
  auto found = flat.finder->find(*flat.def, db::whole_plane(), env.style->active, connect, callbacks);
  if (!found.ok()) {
    return;
  }
  flat.nodes = std::move(found).value();
  attach_labels(*flat.def, *flat.finder, flat.nodes, *env.style, *env.tech, LabelPassOptions{}, *env.compat);
}

std::optional<std::size_t> node_at(const FlatTree& flat, const TileRef& t) {
  if (!flat.finder || t.plane >= flat.def->plane_count()) {
    return std::nullopt;
  }
  return flat.finder->node_of(flat.def->plane(t.plane), t.tile, flat.nodes.size());
}

TileRef tile_of_node(const FlatTree& flat, const NodeRegion& node) {
  TileRef out;
  out.plane = node.plane;
  if (node.plane >= flat.def->plane_count()) {
    return out;
  }
  out.tile = flat.def->plane(node.plane).tile_at(node.reference);
  if (out.tile.split) {
    out.tile.right_side = out.tile.left_type != node.type;
    out.tile.type = out.tile.right_side ? out.tile.right_type : out.tile.left_type;
  }
  return out;
}

std::string flat_node_name(const NodeRegion& node, const Hier& h) {
  if (const LabelRegion* lab = name_label(node)) {
    return lab->text;
  }
  if (node.reference.x <= kTilelessSubstrateCoord && !h.inputs->substrate_name.empty()) {
    return h.inputs->substrate_name;
  }
  return generated_name(node, *h.env.tech);
}

tech::TileType side_type(const db::TileView& t) {
  if (!t.split) {
    return t.type;
  }
  return t.right_side ? t.right_type : t.left_type;
}

void feedback(Hier& h, const db::Rect& area, std::string text, bool medium) {
  h.result->feedback.push_back(NodeFeedback{area, std::move(text), medium});
}

}  // namespace hefesto::extract::hier
