// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The basic check entry (drc-rule-semantics "Basic check scope", "Error
// rectangle reporting"; openspec/changes/m3-drc design.md D6): the plane
// loop of DRCBasicCheck and the per-tile dispatch of drcTile (subcell-error
// tiles, split tiles, left and bottom edges). The edge walks, rule regions
// and nonstandard rules live in the sibling files listed in kernel.hpp.
//
// The mask-layer (cif) rules of a style are not checked until M5
// (design D13; drcCifCheck is not called).
//
// (Magic ref: drc/DRCbasic.c DRCBasicCheck, drcTile)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/check/basic_check.hpp"

#include "hefesto/db/cell.hpp"
#include "hefesto/db/lock.hpp"
#include "kernel.hpp"

namespace hefesto::drc {

bool CancelToken::requested() const noexcept {
  return (flag != nullptr && flag->load(std::memory_order_relaxed)) || (lock != nullptr && lock->writer_waiting());
}

CheckOutcome basic_check(const CheckInput& input, const db::Rect& check_area, const db::Rect& clip_area,
                         ErrorSink& sink) {
  if (input.cell == nullptr || input.rules == nullptr || check_area.is_null()) {
    return CheckOutcome::completed;
  }
  check_detail::Kernel kernel{input, check_area, clip_area, sink};
  return kernel.run();
}

}  // namespace hefesto::drc

namespace hefesto::drc::check_detail {

namespace {

/// drcOverlapCookie: the rule reported for subcell-error tiles (why 2).
Rule overlap_rule() {
  Rule rule;
  rule.why = why::kOverlap;
  return rule;
}

}  // namespace

Kernel::Kernel(const CheckInput& input, const Rect& check_area, const Rect& clip_area, ErrorSink& sink)
    : input_(input), cell_(*input.cell), rules_(*input.rules), check_(check_area), clip_(clip_area), sink_(sink) {}

CheckOutcome Kernel::run() {
  for (std::size_t p = tech::kFirstUserPlane; p < cell_.plane_count(); ++p) {
    plane_ = static_cast<tech::PlaneId>(p);
    edge_plane_ = &cell_.plane(plane_);
    bool stopped = false;
    edge_plane_->enumerate(check_, tech::TypeMask::all(), [&](const TileView& tile) {
      if (input_.cancel.requested()) {
        stopped = true;
        return false;
      }
      return visit_tile(tile);
    });
    if (stopped) {
      return CheckOutcome::cancelled;
    }
  }
  return CheckOutcome::completed;
}

bool Kernel::visit_tile(const TileView& tile) {
  count(&CheckStatistics::tiles);
  if (body_type(tile) == tech::kErrorS) {
    Rect area = tile.area;
    geo_clip(area, check_);  // clipped to K, not L, and reported even when null
    report(area, overlap_rule());
  }
  if (tile.split && !tile.right_side) {
    split_tile(tile);
  }
  if (tile.area.x0 >= check_.x0) {
    vertical_edges(tile);
  }
  if (tile.area.y0 >= check_.y0) {
    horizontal_edges(tile);
  }
  return true;
}

void Kernel::report(const Rect& area, const Rule& rule) { sink_.report(area, rule); }

void Kernel::count(std::uint64_t CheckStatistics::* counter) const noexcept {
  if (CheckStatistics* stats = sink_.statistics()) {
    ++(stats->*counter);
  }
}

const db::Plane* Kernel::plane_of(tech::PlaneId plane) const noexcept {
  return plane < cell_.plane_count() ? &cell_.plane(plane) : nullptr;
}

}  // namespace hefesto::drc::check_detail
