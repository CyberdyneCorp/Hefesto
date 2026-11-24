// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): submit fails with CORE-NOTIMPL-009.
//
// Owner: SIMENG.

#include "hefesto/sim/engine/job_manager.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<JobId> JobManager::submit(PreparedDeck /*deck*/, JobOptions /*options*/) {
  return not_implemented("sim::JobManager::submit");
}

}  // namespace hefesto::sim
