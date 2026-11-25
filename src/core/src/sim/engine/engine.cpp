// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// make_worker_engine: the WorkerEngine of worker_engine.hpp with the default
// channel limits (simulation-engine-embedding "Worker channel limits").
//
// Owner: SIMENG (the interface); the WorkerEngine body is SIMD's (task 5.4).

#include "hefesto/sim/engine/engine.hpp"

#include "sim/engine/worker_engine.hpp"

namespace hefesto::sim {

diag::Outcome<std::unique_ptr<Engine>> make_worker_engine(platform::WorkerHost* host,
                                                          const WorkerEngineOptions& options) {
  return worker::make(host, options, worker::Limits{});
}

}  // namespace hefesto::sim
