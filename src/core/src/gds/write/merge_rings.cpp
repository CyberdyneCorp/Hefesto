// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Boundary rings of merged polygon output (capability gds-write "Merged
// polygon output", Q-MASK-217 replicate). Ported from Magic 8.3.694
// calma/CalmaWrite.c:1954-2257 (calmaAddSegment, calmaRemoveDegenerate,
// calmaRemoveColinear, calmaMergeSegments) and utils/fraction.c:54-62
// (FindGCF); Magic is (c) the Regents of the University of California,
// distributed under its permissive license (THIRD_PARTY_NOTICES.md).
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/write/merge_rings.hpp"

#include <optional>

namespace hefesto::gds::merge {

namespace {

constexpr std::int64_t kMaxBoundPoints = 201;  // calmaMergeSegments' GDS limit check

bool same_point(db::Point a, db::Point b) noexcept {
  return a.x == b.x && a.y == b.y;
}

/// The colinearity test of calmaRemoveColinear for the points p, c, n.
bool colinear(db::Point p, db::Point c, db::Point n) noexcept {
  if ((p.x == c.x && p.x == n.x) || (p.y == c.y && p.y == n.y)) {
    return true;
  }
  if (p.x == c.x || p.y == c.y || c.x == n.x || c.y == n.y) {
    return false;
  }
  // Non-Manhattan edges: compare the direction vectors, each reduced by its
  // greatest common factor only while the two still differ.
  std::int64_t dx1 = std::int64_t{c.x} - p.x;
  std::int64_t dy1 = std::int64_t{c.y} - p.y;
  std::int64_t dx2 = std::int64_t{n.x} - c.x;
  std::int64_t dy2 = std::int64_t{n.y} - c.y;
  if (dx1 != dx2 || dy1 != dy2) {
    const std::int64_t gcf = find_gcf(dx1, dy1);
    if (gcf > 1) {
      dx1 /= gcf;
      dy1 /= gcf;
    }
  }
  if (dx1 != dx2 || dy1 != dy2) {
    const std::int64_t gcf = find_gcf(dx2, dy2);
    if (gcf > 1) {
      dx2 /= gcf;
      dy2 /= gcf;
    }
  }
  return dx1 == dx2 && dy1 == dy2;
}

/// The inner loop of calmaMergeSegments: the antiparallel internal segment
/// of `bound` matching `cur` (whose predecessor is `last`), spliced.
bool splice_into(RingArena& arena, std::uint32_t last, std::uint32_t cur, Bound& bound, std::int64_t num_points,
                 std::int64_t edge_size) {
  const db::Point cur_start = arena[cur].start;
  const db::Point cur_end = arena[arena[cur].next].start;
  std::uint32_t comp_last = bound.first;
  for (std::size_t steps = 0; steps < arena.node_count(); ++steps) {
    const std::uint32_t comp = arena[comp_last].next;
    if (arena[comp].kind != SegmentKind::external && same_point(arena[comp].start, cur_end) &&
        same_point(arena[arena[comp].next].start, cur_start)) {
      arena[last].next = arena[comp].next;
      arena[comp_last].next = arena[cur].next;
      if (comp == bound.first) {
        bound.first = comp_last;
      }
      bound.points += num_points - 2;
      bound.size += edge_size - 2;
      return true;
    }
    comp_last = comp;
    if (comp_last == bound.first) {
      break;
    }
  }
  return false;
}

/// calmaAddSegment's test of one node `cur`: the result when it is a kInit
/// node matching the segment (after marking or splitting it), else nullopt.
std::optional<int> try_node(RingArena& arena, std::uint32_t cur, SegmentKind kind, db::Point p1, db::Point p2,
                            std::uint32_t& cursor) {
  if (arena[cur].kind != SegmentKind::init) {
    return std::nullopt;
  }
  const bool start_match = same_point(p1, arena[cur].start);
  const bool end_match = same_point(p2, arena[arena[cur].next].start);
  if (start_match && end_match) {
    arena[cur].kind = kind;  // the segment completes this edge
    cursor = cur;
    return static_cast<int>(kind);
  }
  if (start_match) {
    // The new node keeps the old kind (kInit), which is what Magic returns.
    const SegmentKind old_kind = arena[cur].kind;
    arena[cur].kind = kind;
    cursor = arena.insert_after(cur, p2, old_kind);
    return static_cast<int>(old_kind);
  }
  if (end_match) {
    cursor = arena.insert_after(cur, p1, kind);
    return static_cast<int>(kind);
  }
  return std::nullopt;
}

/// calmaMergeSegments' walk of the bounds (newest first) for the segment
/// `cur`; true when it was spliced.
bool splice_any(RingArena& arena, std::uint32_t last, std::uint32_t cur, BoundList& bounds, std::int64_t num_points,
                std::int64_t edge_size) {
  for (auto bound = bounds.rbegin(); bound != bounds.rend(); ++bound) {
    if (bound->points + num_points <= kMaxBoundPoints &&
        splice_into(arena, last, cur, *bound, num_points, edge_size)) {
      return true;
    }
  }
  return false;
}

/// One pass of calmaRemoveDegenerate's inner loop; true when it removed a
/// backtracking pair (and restarted the bound at its predecessor).
bool remove_one_degenerate(RingArena& arena, Bound& bound) {
  std::uint32_t last = bound.first;
  for (std::size_t steps = 0; steps < arena.node_count(); ++steps) {
    const std::uint32_t cur = arena[last].next;
    const std::uint32_t after = arena[arena[cur].next].next;
    if (same_point(arena[cur].start, arena[after].start)) {
      arena[last].next = after;
      bound.first = last;
      bound.points -= 2;
      bound.size -= 2;
      return true;
    }
    last = arena[last].next;
    if (last == bound.first) {
      break;
    }
  }
  return false;
}

void remove_colinear_in(RingArena& arena, Bound& bound) {
  // Magic's loop: `for (last = first; last != stop;) { stop = first; ... }`,
  // so the stop node is the bound's first node as it was when the step began.
  std::uint32_t last = bound.first;
  std::uint32_t stop = 0;
  do {
    if (bound.size <= 1) {
      return;  // Magic would read a freed node here
    }
    stop = bound.first;
    const std::uint32_t cur = arena[last].next;
    const std::uint32_t next = arena[cur].next;
    if (colinear(arena[last].start, arena[cur].start, arena[next].start)) {
      arena[last].next = next;
      if (cur == bound.first) {
        bound.first = last;
      }
      --bound.points;
      --bound.size;
    } else {
      last = arena[last].next;
    }
  } while (last != stop);
}

}  // namespace

std::uint32_t RingArena::make_ring(std::initializer_list<db::Point> corners) {
  const auto first = static_cast<std::uint32_t>(nodes_.size());
  std::uint32_t index = first;
  for (const db::Point p : corners) {
    nodes_.push_back(RingNode{p, SegmentKind::init, ++index});
  }
  nodes_.back().next = first;
  return first;
}

std::uint32_t RingArena::insert_after(std::uint32_t after, db::Point start, SegmentKind kind) {
  const auto index = static_cast<std::uint32_t>(nodes_.size());
  nodes_.push_back(RingNode{start, kind, nodes_[after].next});
  nodes_[after].next = index;
  return index;
}

std::int64_t RingArena::ring_size(std::uint32_t first) const {
  std::int64_t size = 0;
  std::uint32_t node = first;
  for (std::size_t steps = 0; steps < nodes_.size(); ++steps) {
    ++size;
    node = nodes_[node].next;
    if (node == first) {
      break;
    }
  }
  return size;
}

mask::Polygon RingArena::points(std::uint32_t first) const {
  mask::Polygon out;
  std::uint32_t node = first;
  for (std::size_t steps = 0; steps < nodes_.size(); ++steps) {
    out.push_back(nodes_[node].start);
    node = nodes_[node].next;
    if (node == first) {
      break;
    }
  }
  return out;
}

int add_segment(RingArena& arena, std::uint32_t& cursor, bool external, db::Point p1, db::Point p2) {
  const SegmentKind kind = external ? SegmentKind::external : SegmentKind::internal;
  std::uint32_t cur = cursor;
  for (std::size_t steps = 0; steps < arena.node_count(); ++steps) {
    if (const std::optional<int> result = try_node(arena, cur, kind, p1, p2, cursor)) {
      return *result;
    }
    cur = arena[cur].next;
    if (cur == cursor) {
      break;
    }
  }
  return -1;  // no connecting point (Magic: "shouldn't happen, but isn't fatal")
}

void merge_segments(RingArena& arena, std::uint32_t edge, BoundList& bounds, std::int64_t num_points) {
  const std::int64_t edge_size = arena.ring_size(edge);
  if (!bounds.empty()) {
    std::uint32_t last = edge;
    for (std::int64_t steps = 0; steps < edge_size; ++steps) {
      const std::uint32_t cur = arena[last].next;
      if (arena[cur].kind != SegmentKind::external && splice_any(arena, last, cur, bounds, num_points, edge_size)) {
        return;
      }
      last = arena[last].next;
      if (last == edge) {
        break;
      }
    }
  }
  bounds.push_back(Bound{edge, num_points, edge_size});
}

void remove_degenerate(RingArena& arena, BoundList& bounds) {
  for (auto bound = bounds.rbegin(); bound != bounds.rend(); ++bound) {
    // A ring of two nodes would make Magic free the node it keeps.
    while (bound->size > 2 && remove_one_degenerate(arena, *bound)) {
    }
  }
}

void remove_colinear(RingArena& arena, BoundList& bounds) {
  for (auto bound = bounds.rbegin(); bound != bounds.rend(); ++bound) {
    remove_colinear_in(arena, *bound);
  }
}

std::int64_t find_gcf(std::int64_t a, std::int64_t b) noexcept {
  std::int64_t x = a < 0 ? -a : a;
  std::int64_t y = b < 0 ? -b : b;
  if (y == 0) {
    return x;  // Magic divides by zero; never reached from remove_colinear
  }
  for (std::int64_t r = x % y; r != 0; r = x % y) {
    x = y;
    y = r;
  }
  return y;
}

}  // namespace hefesto::gds::merge
