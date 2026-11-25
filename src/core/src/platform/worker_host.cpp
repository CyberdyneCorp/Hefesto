// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The core default WorkerHost: no process creation (hefesto-architecture
// "Long-lived worker processes", scenario "No process creation").
//
// Owner: PLATFORM.

#include "hefesto/platform/worker_host.hpp"

#include "hefesto/diag/codes.hpp"

namespace hefesto::platform {

diag::Outcome<std::unique_ptr<WorkerChannel>> UnsupportedWorkerHost::start(const WorkerSpec& spec) {
  return diag::make(diag::codes::kPlatformUnsupported,
                    "worker processes are not available on this platform (requested '" + spec.program + "')");
}

}  // namespace hefesto::platform
