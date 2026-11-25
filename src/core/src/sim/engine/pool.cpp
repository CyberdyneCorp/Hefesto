// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): worker_pool_size fails with
// CORE-NOTIMPL-009; SIMENG implements the Q-SIMADV-13 formula (task 5.5).
//
// Owner: SIMENG.

#include "hefesto/sim/engine/pool.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<std::uint32_t> worker_pool_size(const PoolInputs& /*inputs*/) {
  return not_implemented("sim::worker_pool_size");
}

}  // namespace hefesto::sim
