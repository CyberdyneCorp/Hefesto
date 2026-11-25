// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_subtree: the hierarchical part of extracting one cell (capability
// extract-hierarchy; openspec/changes/m7-extraction design D3, D11): the
// interactions between the cell's paint and its subcells and between
// subcells, in step squares ("Interaction halo and step squares"), pass 1
// (connections and hierarchical names), pass 2 (adjustments and the
// hierarchical `cap` records), the subcell substrate connections
// (extract-substrate "Subcell substrate connections"), and then the array
// interactions ("Array interactions"). Writes, through `writer`, the
// subcell `cap` records as it goes, the subcell `merge` records in Magic's
// hash order (Q-EXTRACT-7), then the array `cap` and `merge` records.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtSubtree.c (extSubtree,
// extSubtreeInteraction, extSubtreeFunc, extSubtreeTileToNode),
// extract/ExtHier.c, extract/ExtHard.c, extract/ExtArray.c and
// extract/ExtYank.c; Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Contract with extract_cell (EXTCMD's driver, extCellFile's order): call
// extract_subtree() after the cell's flat records, while the client slots
// of the cell's node pass are still kept (RegionFinder::keep_clients), so
// the names of the cell's own nodes are found at their tiles (Magic's
// et_lookNames); reset the slots afterwards. The yank buffers are scratch
// cells owned by the context (ctx.yank; never registered in a cell table).
//
// Owner: EXTHIER.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/regions.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::extract {

class ExtWriter;

/// The extracted cell's own nodes, as its flat pass left them.
struct ParentNodes {
  /// The node pass of the cell, its slots kept; nullptr: no node of the
  /// cell is found by location (every parent tile is unnamed).
  const RegionFinder* finder = nullptr;
  /// The size of that pass's NodeRegionList.
  std::size_t list_size = 0;
  /// The output name of each list entry (as the `node` records wrote it).
  std::span<const std::string> names;
  /// The list index of the cell's substrate node (Magic's glob_subsnode);
  /// nullopt: no substrate node, so no subcell substrate is connected.
  std::optional<std::size_t> substrate;
};

/// The inputs of extract_subtree() besides the context.
struct SubtreeInputs {
  const tech::Technology* technology = nullptr;  // required
  ParentNodes parent;
  /// extSubsName's text for a tile-less substrate node (the substrate
  /// declaration's name with its `$` variable substituted); empty when
  /// the style declares none.
  std::string substrate_name;
  /// Cells marked "don't use" (CDDONTUSE): their substrate is never
  /// connected. Empty: none is.
  std::function<bool(const db::CellDef&)> dont_use;
};

/// What the hierarchical part reports.
struct SubtreeResult {
  std::int32_t errors = 0;
  std::int32_t warnings = 0;
  /// The feedback entries (Illegal overlap, Cannot find the name of this
  /// node, the labels warning), in the order Magic adds them.
  std::vector<NodeFeedback> feedback;
};

/// extSubtree then extArray for `cell`. Fails only when a required input
/// is missing.
[[nodiscard]] diag::Outcome<SubtreeResult> extract_subtree(ExtractContext& ctx, db::CellDef& cell, ExtWriter& writer,
                                                           const SubtreeInputs& inputs);

}  // namespace hefesto::extract
