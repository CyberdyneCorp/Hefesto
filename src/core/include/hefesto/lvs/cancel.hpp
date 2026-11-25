// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs::CancelToken: how an LVS run is cancelled (openspec/changes/
// m9-lvs-simulation design.md D8; capability lvs-comparison-algorithm
// "Long-running comparisons", lvs-setup-file "Setup evaluation is bounded and
// cancellable"). Setup evaluation, reduction, refinement and the compare
// queue poll it; the flag belongs to the run (lvs::Session).
//
// Owner: SCAFFOLD (shared by the LVS workstreams).

#pragma once

#include <atomic>

namespace hefesto::lvs {

struct CancelToken {
  /// The run's cancel flag; nullptr = the run cannot be cancelled.
  const std::atomic<bool>* flag = nullptr;

  /// True when the run should stop now.
  [[nodiscard]] bool requested() const noexcept { return flag != nullptr && flag->load(std::memory_order_relaxed); }
};

}  // namespace hefesto::lvs
