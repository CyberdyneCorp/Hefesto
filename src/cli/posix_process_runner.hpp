// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// POSIX implementation of the ProcessRunner platform service. It lives in the
// CLI host because the core must not spawn processes (capability
// hefesto-architecture, requirement "Platform-independent core").

#pragma once

#include "hefesto/platform/process_runner.hpp"

namespace hefesto::cli {

class PosixProcessRunner final : public platform::ProcessRunner {
 public:
  /// Ignores SIGPIPE for the process so a child that closes its stdin early
  /// cannot kill the host.
  PosixProcessRunner();

  [[nodiscard]] bool supported() const noexcept override { return true; }
  [[nodiscard]] diag::Outcome<platform::ProcessOutput> run(const platform::ProcessRequest& request) override;
};

}  // namespace hefesto::cli
