// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Progress and cancellation of a long generation, read or write
// (Q-MASK-414; openspec/changes/m5-mask-gds-cif/design.md D4, D8, D16),
// and the generation statistics (`cif statistics`, DrcStatistics::cif_tiles).
// The commands, the CLI and the shell pass a Progress to gds::read/write,
// cif::read/write and the generators; nullptr means no reporting and no
// cancellation.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GEN reports
// through it, SHELL hosts it (design D2).

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

namespace hefesto::mask {

struct Progress {
  /// Called with the fraction done in [0, 1]; the reporter rate-limits
  /// (about every 5 s). May be empty.
  std::function<void(double fraction)> report;
  /// Set by the host to request cancellation; nullptr = never cancelled.
  const std::atomic<bool>* cancel = nullptr;

  /// True when the host asked to stop.
  [[nodiscard]] bool cancelled() const noexcept { return cancel != nullptr && cancel->load(std::memory_order_relaxed); }
  /// Reports `fraction` when a reporter is set.
  void update(double fraction) const {
    if (report) {
      report(fraction);
    }
  }
};

/// Counters of one or more generations (CIFTileOps, CIFHierTileOps, ...).
struct GenerationStatistics {
  std::uint64_t flat_tiles = 0;          // tiles painted by flat generation
  std::uint64_t hier_tiles = 0;          // tiles painted by hierarchical processing
  std::uint64_t flat_rects = 0;          // rectangles output by flat generation
  std::uint64_t hier_rects = 0;          // rectangles output by hierarchical processing
  std::uint64_t interaction_areas = 0;   // interaction areas processed

  friend bool operator==(const GenerationStatistics&, const GenerationStatistics&) = default;
};

}  // namespace hefesto::mask
