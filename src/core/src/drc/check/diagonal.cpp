// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Split-tile processing of the basic check (drc-rule-semantics "Angle
// rules", "Diagonal edge checks"; openspec/changes/m3-drc design.md D6).
//
// A split tile is processed once, from its left-side report: the first
// ANGLES_90 rule of the pair (right type, left type) checks its angle, and
// every standard or SPLITTILE rule of that pair is applied to the diagonal
// through the offset-triangle search. Magic accepts a candidate only if it
// also lies in the initiating tile's rectangle, which another tile never
// does, so Magic 8.3.694 reports nothing from a diagonal edge: with
// magic-compat ON Hefesto reproduces that (the statistics still count the
// searches); with it OFF it performs the intended check and reports every
// candidate of the translated triangle as a standard candidate (Q-DRC-10).
//
// (Magic ref: drc/DRCbasic.c drcTile (lines 727-843), areaNMCheck,
// areaNMReject; drc/DRCextend.c drcCheckAngles)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/check/euclidean.hpp"
#include "kernel.hpp"

namespace hefesto::drc::check_detail {

namespace {

constexpr RuleFlags kSkippedOnDiagonals =
    flags::kAngles90 | flags::kArea | flags::kMaxWidth | flags::kRectSize | flags::kOffGrid;

}  // namespace

void Kernel::split_tile(const TileView& tile) {
  const std::span<const Rule> rules = rules_.rules(tile.right_type, tile.left_type);
  for (const Rule& rule : rules) {
    if (rule.has(flags::kAngles90)) {
      check_angles(tile, rule);
      break;
    }
  }
  for (std::size_t i = 0; i < rules.size(); ++i) {
    const Rule& rule = rules[i];
    if (rule.has(flags::kTrigger)) {
      ++i;  // triggers are not evaluated on diagonals: skip both rules
      continue;
    }
    if ((rule.flags & kSkippedOnDiagonals) != 0 || plane_ != rule.edge_plane) {
      continue;
    }
    diagonal_rule(tile, rule);
  }
  count(&CheckStatistics::edges);
}

void Kernel::diagonal_rule(const TileView& tile, const Rule& rule) {
  count(&CheckStatistics::rules);
  count(&CheckStatistics::slow);
  if (input_.compat.magic_compat) {
    return;  // Q-DRC-10: Magic's acceptance test never succeeds
  }
  const db::Plane* plane = plane_of(rule.check_plane);
  if (plane == nullptr) {
    return;
  }
  const auto width = static_cast<std::int32_t>(tile.area.width());
  const auto height = static_cast<std::int32_t>(tile.area.height());
  const DiagonalOffset offset = diagonal_offset(rule.dist, width, height);
  std::int32_t dx = offset.dx;
  std::int32_t dy = tile.diagonal == db::Diagonal::ne_sw ? -offset.dy : offset.dy;
  const bool forward = !rule.reverse();
  if (forward) {  // the forward search lies behind the triangle
    dx = -dx;
    dy = -dy;
  }
  Search search;
  search.rule = &rule;
  search.region = Rect{tile.area.x0 + dx, tile.area.y0 + dy, tile.area.x1 + dx, tile.area.y1 + dy};
  search.initial = &tile;
  tech::TypeMask wrong = ~rule.ok;
  wrong.clear_type(tech::kErrorS);
  const bool same_plane = rule.check_plane == plane_;
  plane->enumerate_triangle(search.region, tile.diagonal, forward, wrong, [&](const TileView& found) {
    if (!(same_plane && found.area == tile.area)) {
      candidate(found, search);
    }
    return true;
  });
}

}  // namespace hefesto::drc::check_detail
