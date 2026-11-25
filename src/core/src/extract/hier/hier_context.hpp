// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The state of one cell's hierarchical extraction (Magic's HierExtractArg
// and ExtTree; extract/extractInt.h), shared by the extract/hier sources.
//
// Owner: EXTHIER.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/hier/node_table.hpp"
#include "hefesto/extract/hier/subtree.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract::hier {

/// What every step reads.
struct Env {
  const ExtStyle* style = nullptr;
  const tech::Technology* tech = nullptr;
  const ExtractOptions* options = nullptr;
  const compat::CompatSettings* compat = nullptr;
  std::int32_t halo = 1;
  std::optional<std::int32_t> side_halo;  // the `extract halo` override

  [[nodiscard]] bool couple_adjust() const { return options->coupling && options->adjust; }
};

/// A tile side in a plane (Magic's Tile * with its dinfo).
struct TileRef {
  tech::PlaneId plane = 0;
  db::TileView tile;
};

/// ExtTree: a flattened subtree in a scratch cell, its nodes (slots kept)
/// and its coupling.
struct FlatTree {
  db::CellDef* def = nullptr;           // the scratch cell (owned elsewhere or by `owned`)
  std::unique_ptr<db::CellDef> owned;
  std::unique_ptr<RegionFinder> finder;  // the last node pass, slots kept
  NodeRegionList nodes;
  CouplingTable coupling;
  const db::CellDef* look_names = nullptr;  // et_lookNames: the extracted cell
  const db::CellUse* real_use = nullptr;    // et_realuse
};

/// The naming mode of a pass: extSubtreeTileToNode or extArrayTileToNode.
enum class Naming : std::uint8_t { subtree, array };

/// The array being processed (ExtArray.c's globals).
struct ArrayState {
  const db::CellUse* use = nullptr;
  std::int32_t px = 0, py = 0;  // the primary element
  std::int32_t ix = 0, iy = 0;  // the element interacting with it
  db::Transform ptrans, itrans;
  FlatTree* primary = nullptr;
};

/// HierExtractArg.
struct Hier {
  Env env;
  db::CellDef* parent = nullptr;
  const SubtreeInputs* inputs = nullptr;
  ExtWriter* writer = nullptr;
  SubtreeResult* result = nullptr;
  HierNodeTable table{0};
  FlatTree cum;
  std::vector<std::unique_ptr<FlatTree>> subs;  // extSubList, the most recent first
  const db::CellUse* sub_use = nullptr;
  db::Rect sub_area, inter_area, clip_area;
  Naming naming = Naming::subtree;
  ArrayState array;
  std::unordered_set<const db::CellUse*> sub_extracted;  // CU_SUB_EXTRACTED
  CapClass cap_class = CapClass::subcell;
};

// --- flat.cpp ---------------------------------------------------------------

/// A FlatTree with its own new scratch cell.
[[nodiscard]] std::unique_ptr<FlatTree> new_flat(const Env& env, std::size_t plane_count);
/// extHierFreeOne / ExtResetTiles: slots reset, nodes, coupling, paint and
/// labels dropped.
void clear_flat(FlatTree& flat);
/// Drops the node list and resets the slots (ExtFreeLabRegions +
/// ExtResetTiles), keeping paint and labels.
void drop_nodes(FlatTree& flat);
/// extFindNodes(def, clip) with the capacitance, then (when `labels`)
/// ExtLabelRegions(def, clip) and (when `coupling`) extFindCoupling.
void extract_flat(FlatTree& flat, const Env& env, const db::Rect& clip, bool labels, bool coupling);
/// ExtFindRegions over the active types (extHierLabFirst) and
/// ExtLabelRegions over the whole plane: the labelled regions of the
/// cumulative buffer in pass 1.
void label_regions(FlatTree& flat, const Env& env);
/// The list index of the node holding `t`, if any (extHasRegion).
[[nodiscard]] std::optional<std::size_t> node_at(const FlatTree& flat, const TileRef& t);
/// extNodeToTile: the tile (side) at a node's reference point in `flat`.
[[nodiscard]] TileRef tile_of_node(const FlatTree& flat, const NodeRegion& node);
/// extNodeName of a flat node: its first name label, the substrate name
/// for a tile-less substrate node, else the generated name.
[[nodiscard]] std::string flat_node_name(const NodeRegion& node, const Hier& h);
/// The type of the side `t` names.
[[nodiscard]] tech::TileType side_type(const db::TileView& t);
/// Adds a feedback entry.
void feedback(Hier& h, const db::Rect& area, std::string text, bool medium);

// --- naming.cpp ---------------------------------------------------------------

/// ha_nodename: extSubtreeTileToNode or extArrayTileToNode by h.naming.
/// nullopt only when `hard` is false and no name was found.
[[nodiscard]] std::optional<std::string> tile_to_node(Hier& h, const TileRef& t, FlatTree& et, bool hard);
/// The parent's node at `t`'s area on its plane (et_lookNames), if any.
[[nodiscard]] std::optional<std::string> parent_name_at(const Hier& h, const TileRef& t);

// --- hard.cpp -------------------------------------------------------------------

/// The hard-way label of a tile of `et` (extSubtreeHardNode /
/// extArrayHardNode): attaches the found label to the tile's node and to
/// et's cell; false when none was found.
bool hard_node(Hier& h, const TileRef& t, FlatTree& et);

// --- connect.cpp ---------------------------------------------------------------

/// extHierConnections between `cum` and `one` over h.sub_area.
void connect_trees(Hier& h, FlatTree& cum, FlatTree& one);
/// Unifies the nodes of two names (the cumulative side first).
void unify_names(Hier& h, const std::string& cumulative, const std::string& added);

// --- adjust.cpp ----------------------------------------------------------------

/// extSubtreeAdjustInit, extHierAdjustments for every subtree and
/// extSubtreeOutputCoupling.
void adjust_interaction(Hier& h);
/// extHierAdjustments(ha, cum, one, look).
void adjust_tree(Hier& h, FlatTree& cum, FlatTree& one, FlatTree& look);

// --- substrate_hier.cpp ---------------------------------------------------------

/// extHierSubstrate(ha, use, x, y) (-1 for an absent index): true when the
/// substrate was extracted or needs nothing (Magic's 0).
bool connect_substrate(Hier& h, const db::CellUse& use, std::int32_t x, std::int32_t y);
/// The substrate of every element of `use` (extSubstrateFunc and the end
/// of extSubtreeFunc); `mark_always`: set the use's flag even when every
/// element failed.
void connect_use_substrate(Hier& h, const db::CellUse& use, bool mark_always);

// --- array.cpp -------------------------------------------------------------------

/// extArray.
void extract_arrays(Hier& h);

}  // namespace hefesto::extract::hier
