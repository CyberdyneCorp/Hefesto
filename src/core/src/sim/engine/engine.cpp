// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): make_worker_engine fails with
// CORE-NOTIMPL-009 until SIMENG lands WorkerEngine (task 5.5).
//
// Owner: SIMENG.

#include "hefesto/sim/engine/engine.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<std::unique_ptr<Engine>> make_worker_engine(platform::WorkerHost* /*host*/,
                                                          const WorkerEngineOptions& /*options*/) {
  return not_implemented("sim::make_worker_engine");
}

}  // namespace hefesto::sim
