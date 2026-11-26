// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label attachment and the per-node records (capability
// extract-nodes-and-labels "Label attachment", "Port records", "Node and
// attribute records", "Alias output", "Duplicate label warning";
// openspec/changes/m7-extraction/design.md D8). Added in M7 R3 (EXTNODE).
//
// attach_labels() is ExtLabelRegions: it reads the region slots of a pass
// run with RegionFinder::keep_clients. write_node_records() is
// extOutputNodes: `port` records, then per node its `node` (or
// `substrate`) record with its `attr` and `equiv` records.
//
// Owner: EXTNODE.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/extract/node/regions.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::extract {

class ExtStyle;
class ExtWriter;

/// The labels extOutputNodes marked as non-exported aliases (Magic's
/// `lab_port = INFINITY`), by index in the cell's label list. Cleared at
/// the start of every cell extraction (ExtCell.c:520-523); later label
/// attachment in the same extraction skips them.
using AliasMarks = std::vector<bool>;

struct LabelPassOptions {
  /// The sticky-label area (ExtLabelRegions' clipArea); the whole plane
  /// when unset.
  std::optional<db::Rect> clip;
  /// Create sticky-label nodes and attach substrate labels (a node list is
  /// given in Magic's call).
  bool sticky = true;
  const AliasMarks* skip = nullptr;
};

/// ExtLabelRegions: attaches the labels of `cell` to the nodes of `nodes`
/// (a list of `finder`'s last pass, its slots kept), in label-list order,
/// with the insertion of label_insert_position(); prepends a sticky-label
/// node for an active-type label over no node.
void attach_labels(const db::CellDef& cell, const RegionFinder& finder, NodeRegionList& nodes,
                   const ExtStyle& style, const tech::Technology& technology, const LabelPassOptions& options,
                   const compat::CompatSettings& compat);

/// A feedback entry left by the duplicate-label and unique passes
/// (DBWFeedbackAdd): area, text, and the medium-highlight style of the
/// unique pass (pale highlights otherwise).
struct NodeFeedback {
  db::Rect area;
  std::string text;
  bool medium = false;

  friend bool operator==(const NodeFeedback&, const NodeFeedback&) = default;
};

/// The substrate node's name, when the list holds one (extSubsName).
struct SubstrateName {
  std::optional<std::size_t> node;  // list index
  std::string name;
  /// Added in M7 R5: whether that node is also the cell's substrate node
  /// (glob_subsnode), written as the `substrate` record. False for an
  /// abstract view's node at -infinity, which extMakeNodeNumPrint names
  /// with extSubsName but extOutputNodes writes as a plain `node`.
  bool record = true;
};

struct NodeRecordOptions {
  /// Final values per list index (empty: R 0; C = substrate_cap / cscale,
  /// negative written as 0, as extOutputNodes does).
  std::vector<std::int64_t> resistance;
  std::vector<double> capacitance;
  double cscale = 1.0;
  SubstrateName substrate;
  /// EXT_DOALIASES (`extract do aliases`).
  bool aliases = false;
  /// EXT_DORESISTANCE (lumped resistance) and full R-C extraction in the
  /// same pass: the shorted-ports warning's suppression (Q-EXTRACT-22).
  bool resistance_on = true;
  bool extresist = false;
};

struct NodeRecordResult {
  AliasMarks non_exported;  // by label index
  std::size_t warnings = 0;
};

/// extOutputNodes: writes the `port`, `node`/`substrate`, `attr` and
/// `equiv` records of `nodes` (a port named like its node moves the node's
/// reference to the label) and reports the shorted-ports warning
/// (EXTRACT-NODE-001) to `sink`.
NodeRecordResult write_node_records(ExtWriter& writer, NodeRegionList& nodes, const db::CellDef& cell,
                                    const tech::Technology& technology, const NodeRecordOptions& options,
                                    const compat::CompatSettings& compat, diag::DiagnosticSink& sink);

/// The name extMakeNodeNumPrint gives list entry `i`: the substrate name
/// or generated_name().
[[nodiscard]] std::string number_name(const NodeRegionList& nodes, std::size_t i,
                                      const tech::Technology& technology, const SubstrateName& substrate);

/// extFindDuplicateLabels: one feedback entry per occurrence of a name
/// label text attached to more than one node (none for an abstract view,
/// a cell with `LEFview`); each counts as one warning.
[[nodiscard]] std::vector<NodeFeedback> duplicate_label_feedback(const db::CellDef& cell, const NodeRegionList& nodes,
                                                                 const tech::Technology& technology,
                                                                 const SubstrateName& substrate);

}  // namespace hefesto::extract
