// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.ext` read-and-flatten subset that `def write` needs (capabilities
// ext-flattening-read, ext-flattening-hierarchy, ext-flattening-names;
// openspec/changes/m6-lef-def-oasis/design.md D4, D9, D10; Open Question 1,
// decided (a)): EFReadFile from the root cell down through its `use`
// records, then EFFlatBuild with EF_FLATNODES | EF_NOFLATSUBCKT, unscaled,
// with resistance unused. The result is the flat node list with each node's
// preferred hierarchical name, its layer and location, and the timestamp of
// every `.ext` file read (Q-LEFDEF-110 OFF). It is built to the M7 specs in
// M7's area layout, so M7 extends it rather than replacing it.
//
// Scaffold stub: read_flat fails with CORE-NOTIMPL-006 until EXTSUB lands
// it (the stub body is inline so that the header needs no area sources).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); EXTSUB
// fills in the implementation in src/core/src/extflat/ (design D2).

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::extflat {

/// The options of one read.
struct ReadOptions {
  /// Record each `.ext` file's `timestamp` (Q-LEFDEF-110 OFF compares them
  /// with the layout's).
  bool timestamps = false;
};

/// One flat node (EFNode after EFFlatBuild with EF_FLATNODES).
struct FlatNode {
  /// The preferred hierarchical name (ext-flattening-names "Name preference
  /// order"), with `/` separating the use path.
  std::string name;
  /// The node's layer as the `.ext` file names it (the `node` record's type).
  std::string layer;
  /// The node's location in root coordinates (the `node` record's x, y).
  db::Point location;
  /// True for the substrate node (the `substrate` record).
  bool substrate = false;
};

/// The timestamp of one `.ext` file read (its `timestamp` record).
struct FileStamp {
  std::string cell;
  std::int64_t timestamp = 0;
};

/// The flattened netlist of one root cell.
class Netlist {
 public:
  Netlist() = default;
  Netlist(std::vector<FlatNode> nodes, std::vector<FileStamp> stamps)
      : nodes_(std::move(nodes)), stamps_(std::move(stamps)) {}

  /// Calls `visitor` for each node in EFVisitNodes order.
  void visit_nodes(const std::function<void(const FlatNode&)>& visitor) const {
    for (const FlatNode& node : nodes_) {
      visitor(node);
    }
  }

  [[nodiscard]] const std::vector<FlatNode>& nodes() const noexcept { return nodes_; }
  [[nodiscard]] const std::vector<FileStamp>& stamps() const noexcept { return stamps_; }

 private:
  std::vector<FlatNode> nodes_;   // EFVisitNodes order
  std::vector<FileStamp> stamps_;  // in read order
};

/// Reads `<root>.ext` and the `.ext` files of every cell it uses, found
/// through `ctx` as EFReadFile finds them, and flattens the nodes.
[[nodiscard]] inline diag::Outcome<Netlist> read_flat(cell::LookupContext& /*ctx*/, std::string_view /*root*/,
                                                      const ReadOptions& /*options*/) {
  return diag::make("CORE-NOTIMPL-006", "not implemented yet: extflat read_flat");
}

}  // namespace hefesto::extflat
