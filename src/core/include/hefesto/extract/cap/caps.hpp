// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capacitance and lumped resistance (capabilities extract-capacitance,
// extract-resistance-lumped; openspec/changes/m7-extraction/design.md D9).
// Substrate (area, perimeter, corner), overlap, sidewall, sideoverlap
// (legacy and fringe-halo) and corner capacitance; coupling accumulates
// into a CouplingTable keyed by node index while extracting and by name
// only at output (Q-EXTRACT-7), so no address order leaks into the file.
// Each compat split is one named function in cap/compat_branches.hpp (D9).
//
// Nodes. The accumulators read the node of a tile through
// CapInputs::node_of, by default the tile's client slot
// (db::Plane::client), which RegionFinder leaves holding the index of the
// tile's NodeRegion (design D6, D8); kClientDefault, or an index past the
// list, is "no node" (Magic's CLIENTDEFAULT).
//
// Order. Magic accumulates in one pass over the planes in plane-number
// order, overlap before sidewall on each plane (ExtCouple.c
// extFindCoupling); find_coupling() keeps that order, so every
// floating-point sum is formed in Magic's order. The other coupling entry
// points run one kind of coupling alone (tests, diagnostics).
//
// Owner: EXTCAP (this header and the implementation; INTEGRATION reviews
// the contract).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {

namespace cap {
class Context;
}

/// Coupling capacitance between pairs of nodes (by index into the cell's
/// NodeRegionList, smaller index first), in attofarads. Address-free: the
/// key is the node index, and output is name-keyed (coupling_records()).
class CouplingTable {
 public:
  void add(std::size_t a, std::size_t b, double cap) {
    if (b < a) {
      std::swap(a, b);
    }
    pairs_[{a, b}] += cap;
  }
  /// Replaces the value of a pair (extSetCapValue).
  void set(std::size_t a, std::size_t b, double cap) {
    if (b < a) {
      std::swap(a, b);
    }
    pairs_[{a, b}] = cap;
  }
  [[nodiscard]] double value(std::size_t a, std::size_t b) const {
    if (b < a) {
      std::swap(a, b);
    }
    const auto it = pairs_.find({a, b});
    return it == pairs_.end() ? 0.0 : it->second;
  }
  [[nodiscard]] const std::map<std::pair<std::size_t, std::size_t>, double>& pairs() const noexcept { return pairs_; }

 private:
  std::map<std::pair<std::size_t, std::size_t>, double> pairs_;
};

/// The node of one side of a tile (`right_side`: Magic's TT_SIDE for a
/// split tile; false for a Manhattan tile), or nullopt for none.
using NodeLookup =
    std::function<std::optional<std::size_t>(tech::PlaneId plane, const db::TileView& tile, bool right_side)>;

/// The default NodeLookup: the tile's client slot as an index below
/// `node_count`; kClientDefault or a larger value is no node.
[[nodiscard]] std::optional<std::size_t> client_node(const db::Plane& plane, const db::TileView& tile,
                                                     std::size_t node_count);

/// The NodeLookup of a production pass (added at the M7 R3 integration):
/// RegionFinder::node_of over the slots `finder` kept (keep_clients) for
/// the pass whose NodeRegionList has `list_size` entries, asked for the
/// side `right_side` names. client_node() reads a plain index and serves
/// fixtures that write the index into the slots themselves; RegionFinder
/// encodes its slots (regions.hpp), so a pass after find_nodes() uses this.
[[nodiscard]] NodeLookup region_node_lookup(const RegionFinder& finder, const db::CellDef& cell,
                                            std::size_t list_size);

/// The inputs every accumulator shares. `cell`, `style` and `technology`
/// are required; the others default to Magic's start-up state.
struct CapInputs {
  const db::CellDef* cell = nullptr;
  const ExtStyle* style = nullptr;
  const compat::CompatSettings* compat = nullptr;  // nullptr: magic-compat ON
  /// Contacts and their residues (DBIsContact, DBPlaneToResidue).
  const tech::Technology* technology = nullptr;
  /// ExtOptions (capacitance, coupling, lumped, fringehalo, corners and
  /// the corner factor, extresist); nullptr: Magic's defaults.
  const ExtractOptions* options = nullptr;
  /// The clip area of a hierarchical pass (extNodeClipArea for substrate
  /// caps, extCoupleSearchArea for coupling); nullopt: the whole plane.
  std::optional<db::Rect> clip;
  /// The `extract halo` override of exts_sideCoupleHalo; nullopt: the style's.
  std::optional<std::int32_t> side_halo;
  /// How a tile's node is read; empty: client_node().
  NodeLookup node_of;
};

/// One tile (side) of a node, as RegionFinder visits it: the plane, the
/// tile and, for a split tile, which side (TT_SIDE).
struct NodeTile {
  tech::PlaneId plane = 0;
  db::TileView tile;
  bool right_side = false;
};

/// extNodeAreaFunc's capacitance and resistance-class work for one tile of
/// a node: area and perimeter capacitance to substrate (with the split-tile
/// diagonal (int)sqrt(w*w + h*h), ExtPerim.c:105-117), the corner fringe to
/// substrate (Q-EXTRACT-24) and the per-class area and perimeter. Calling
/// add() for each tile in RegionFinder's visit order reproduces Magic's
/// summation order; substrate_caps() replays that order itself
/// (extract/cap/flood_order.cpp) over the finder's node slots.
class NodeTileAccumulator {
 public:
  /// `node_count` bounds the default client-slot lookup (the corner test
  /// of Q-EXTRACT-24 with magic-compat OFF reads the nodes of neighbours).
  NodeTileAccumulator(const CapInputs& in, std::size_t node_count);
  ~NodeTileAccumulator();
  NodeTileAccumulator(NodeTileAccumulator&&) noexcept;
  NodeTileAccumulator& operator=(NodeTileAccumulator&&) noexcept;
  NodeTileAccumulator(const NodeTileAccumulator&) = delete;
  NodeTileAccumulator& operator=(const NodeTileAccumulator&) = delete;

  /// Adds the tile's contributions to `node` (its substrate_cap and, sized
  /// to the style's classes, its classes); `node_index` is its index.
  void add(const NodeTile& tile, NodeRegion& node, std::size_t node_index) const;

 private:
  std::unique_ptr<const cap::Context> ctx_;
};

/// Area, perimeter and corner capacitance to substrate and the per-class
/// area and perimeter of every node (substrate_cap and classes are
/// recomputed from zero). With option `capacitance` off every node's
/// substrate_cap is 0 ("Capacitance option gating"). Fails with
/// CORE-ARG-style errors only when a required input is missing.
[[nodiscard]] diag::Status substrate_caps(NodeRegionList& nodes, const CapInputs& in);

/// extFindCoupling: overlap, sidewall, sideoverlap (the fringe model of
/// the options) and corner coupling over the planes in Magic's order, then
/// nothing else. Does nothing with option `coupling` off.
[[nodiscard]] diag::Status find_coupling(NodeRegionList& nodes, const CapInputs& in, CouplingTable& out);

/// Overlap coupling with shielding alone (it also removes the shielded
/// area capacitance from the upper node).
[[nodiscard]] diag::Status overlap_caps(NodeRegionList& nodes, const CapInputs& in, CouplingTable& out);
/// Sidewall coupling alone (Q-EXTRACT-20, single-precision denominator).
[[nodiscard]] diag::Status sidewall_caps(const NodeRegionList& nodes, const CapInputs& in, CouplingTable& out);
/// Sideoverlap alone with the given fringe model (Q-EXTRACT-17,
/// Q-EXTRACT-18); the model replaces option `fringehalo`.
[[nodiscard]] diag::Status sideoverlap_caps(NodeRegionList& nodes, const CapInputs& in, FringeModel model,
                                            CouplingTable& out);
/// Corner coupling alone (Q-EXTRACT-24), with corner factor `factor`; it
/// runs only with the fringe-halo model and a side halo > 0.
[[nodiscard]] diag::Status corner_caps(NodeRegionList& nodes, const CapInputs& in, double factor,
                                       CouplingTable& out);

/// extRelocateSubstrateCoupling: every non-zero pair with the substrate
/// node is removed and its value added to the other node's substrate_cap
/// ("Coupling to the substrate node"). Nothing happens without a node.
void relocate_substrate_coupling(NodeRegionList& nodes, std::optional<std::size_t> substrate,
                                 CouplingTable& table);

/// One `cap` record: the two node names, the smaller first (Q-EXTRACT-7),
/// and the value already divided by cscale.
struct CouplingRecord {
  std::string first;
  std::string second;
  double value = 0.0;

  friend bool operator==(const CouplingRecord&, const CouplingRecord&) = default;
};

/// The `cap` records of a table ("Coupling record output"): name-keyed,
/// value / cscale, records whose value is 0 dropped, sorted by (first,
/// second). `names[i]` is the output name of node i. A cell with the
/// `device` property writes none (the caller skips the call).
[[nodiscard]] std::vector<CouplingRecord> coupling_records(const CouplingTable& table,
                                                           std::span<const std::string> names, double cscale);

/// The C field of a `node` record: max(0, cap / cscale).
[[nodiscard]] double node_cap_output(double cap, double cscale) noexcept;

/// The lumped resistance of a node in milliohms (ExtBasic.c:585-645): the
/// sum over its classes of (P + S) / (P - S) * sheet with S = sqrt(P*P -
/// 16A), in Magic's single precision, saturating (a wrapped sum keeps the
/// previous total).
[[nodiscard]] std::int64_t lumped_resistance(const NodeRegion& node, const ExtStyle& style);

/// The R field of a `node` or `substrate` record ("Resistance output
/// rounding"): (r + rscale/2) / rscale in C integer arithmetic.
[[nodiscard]] std::int64_t resistance_output(std::int64_t milliohms, double rscale) noexcept;

/// The values of the `resistclasses` header line, in class order.
[[nodiscard]] std::vector<std::int64_t> resistance_class_header(const ExtStyle& style);

/// 0.6366 * atan(mult * d) with a correctly rounded atan on every
/// platform (Open Question 3 (a), design D22).
[[nodiscard]] double fringe_fraction(double mult, double d);

}  // namespace hefesto::extract
