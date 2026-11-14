// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The boundary rings of merged polygon output (capability gds-write
// "Merged polygon output"; openspec/changes/m5-mask-gds-cif/design.md D12,
// Q-MASK-217 replicate). A line-for-line port of Magic 8.3.694
// calma/CalmaWrite.c calmaAddSegment, calmaMergeSegments,
// calmaRemoveDegenerate and calmaRemoveColinear: point order and polygon
// split points are observable in the output, so every pass keeps Magic's
// scan order, start node and stop rule.
//
// Magic links malloc'd LinkedBoundary nodes and frees the ones it unlinks.
// Here the nodes of one connected component live in a RingArena and are
// addressed by index; an unlinked node simply stays in the arena. Each
// bound also counts the nodes actually in its ring (`size`), besides
// Magic's bt_points, so the passes can stop before the cases where Magic
// would read a freed node (a ring of two nodes in calmaRemoveDegenerate,
// of one node in calmaRemoveColinear). Tile planes never produce such
// rings; the guard only keeps malformed input from looping or crashing.
//
// Differences from the spec text (the code is the reference): add_segment
// returns the kind of the node it leaves the cursor on, so a match of the
// segment's start only returns kInit (2), not 0 or 1; the caller adds that
// to its internal-edge count exactly as Magic does (an isolated tile whose
// side meets several neighbours is therefore merged, not written as a
// plain rectangle). A segment that matches no node returns -1.
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/mask/plane/plane.hpp"

namespace hefesto::gds::merge {

/// LB_EXTERNAL, LB_INTERNAL, LB_INIT (calma/calmaInt.h:248-250); the
/// values are observable through add_segment's result.
enum class SegmentKind : std::uint8_t { external = 0, internal = 1, init = 2 };

/// One LinkedBoundary: the segment from `start` to the next node's start.
struct RingNode {
  db::Point start;
  SegmentKind kind = SegmentKind::init;
  std::uint32_t next = 0;
};

/// A BoundaryTop: a ring entry node, Magic's point count and the nodes
/// really linked in the ring.
struct Bound {
  std::uint32_t first = 0;
  std::int64_t points = 0;  // bt_points
  std::int64_t size = 0;    // nodes in the ring (equal to points for tile input)
};

/// The nodes of one component. Indices stay valid until clear().
class RingArena {
 public:
  /// A closed ring through `corners` in order, every node kInit; returns
  /// the index of the first (Magic's `edge`).
  std::uint32_t make_ring(std::initializer_list<db::Point> corners);
  /// Inserts a node after `after` and returns its index.
  std::uint32_t insert_after(std::uint32_t after, db::Point start, SegmentKind kind);

  [[nodiscard]] RingNode& operator[](std::uint32_t i) { return nodes_[i]; }
  [[nodiscard]] const RingNode& operator[](std::uint32_t i) const { return nodes_[i]; }
  [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }
  /// The nodes reachable from `first` before it comes round again, at most
  /// node_count() of them.
  [[nodiscard]] std::int64_t ring_size(std::uint32_t first) const;
  /// The start points from `first` around the ring (no closing point).
  [[nodiscard]] mask::Polygon points(std::uint32_t first) const;
  void clear() noexcept { nodes_.clear(); }

 private:
  std::vector<RingNode> nodes_;
};

/// calmaAddSegment: from `cursor` (updated to the node it finishes on),
/// scans the ring once for the first kInit node whose start is `p1` and/or
/// whose successor's start is `p2`, and marks or splits it. Returns the
/// kind of the node left under the cursor (0, 1 or 2) or -1 when no node
/// matches.
int add_segment(RingArena& arena, std::uint32_t& cursor, bool external, db::Point p1, db::Point p2);

/// The bounds of one component in creation order: Magic's list is the
/// reverse (newest first), so every walk below goes from back() to front().
using BoundList = std::vector<Bound>;

/// calmaMergeSegments: splices the tile ring at `edge` into the first bound,
/// newest first, holding an antiparallel internal segment and room for
/// `num_points`, or adds a new (newest) bound.
void merge_segments(RingArena& arena, std::uint32_t edge, BoundList& bounds, std::int64_t num_points);

/// calmaRemoveDegenerate over every bound.
void remove_degenerate(RingArena& arena, BoundList& bounds);
/// calmaRemoveColinear over every bound.
void remove_colinear(RingArena& arena, BoundList& bounds);

/// FindGCF (utils/fraction.c) for non-zero operands, in 64-bit.
[[nodiscard]] std::int64_t find_gcf(std::int64_t a, std::int64_t b) noexcept;

}  // namespace hefesto::gds::merge
