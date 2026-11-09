// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// POSIX implementation of the ProcessRunner platform service (posix_spawn).
// It lives outside the core because the core must not spawn processes
// (capability hefesto-architecture, requirement "Platform-independent
// core"); the CLI and the desktop both link it.

#pragma once

#include "hefesto/platform/process_runner.hpp"

namespace hefesto::platform_posix {

class PosixProcessRunner final : public platform::ProcessRunner {
 public:
  /// Ignores SIGPIPE for the process so a child that closes its stdin early
  /// cannot kill the host.
  PosixProcessRunner();

  [[nodiscard]] bool supported() const noexcept override { return true; }
  [[nodiscard]] diag::Outcome<platform::ProcessOutput> run(const platform::ProcessRequest& request) override;
};

}  // namespace hefesto::platform_posix
