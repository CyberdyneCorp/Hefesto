// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PosixWorkerHost: platform::WorkerHost for Linux and macOS, with
// posix_spawn and pipes and one reader thread per channel (capability
// hefesto-architecture "Long-lived worker processes"; openspec/changes/
// m9-lvs-simulation design D9 "Platform service", D10 "Worker IO").
//
// hefesto-cli and the desktop shell use it. The channel writes frames to the
// worker's standard input and reads frames from its standard output; the
// worker's standard error is inherited. SIGPIPE is ignored for the process,
// so a write to a worker that has exited fails instead of killing the host.
//
// Owner: PLATFORM.

#pragma once

#include <memory>

#include "hefesto/platform/worker_host.hpp"

namespace hefesto::cli {

class PosixWorkerHost final : public platform::WorkerHost {
 public:
  /// Ignores SIGPIPE for the process.
  PosixWorkerHost();

  [[nodiscard]] bool supported() const noexcept override { return true; }
  [[nodiscard]] diag::Outcome<std::unique_ptr<platform::WorkerChannel>> start(
      const platform::WorkerSpec& spec) override;
};

}  // namespace hefesto::cli
