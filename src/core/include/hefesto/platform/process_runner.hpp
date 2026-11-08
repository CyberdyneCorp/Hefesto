// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Optional external-process execution (capability hefesto-architecture,
// requirement "Platform services abstraction", scenario "No process
// execution"). The core declares the interface only; implementations that
// spawn processes live in the hosts (src/cli, desktop shell), never in core.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform {

struct ProcessRequest {
  std::string program;  // looked up on PATH when it has no '/'
  std::vector<std::string> args;
  std::optional<Path> working_directory;
  std::string stdin_data;
};

struct ProcessOutput {
  int exit_code = 0;  // 128 + signal number when the process was killed by a signal
  std::string stdout_data;
  std::string stderr_data;
};

class ProcessRunner {
 public:
  ProcessRunner() = default;
  ProcessRunner(const ProcessRunner&) = delete;
  ProcessRunner& operator=(const ProcessRunner&) = delete;
  virtual ~ProcessRunner() = default;

  /// False on platforms without process execution (iPadOS).
  [[nodiscard]] virtual bool supported() const noexcept = 0;
  /// Runs to completion. PLATFORM-UNSUPPORTED-001 when !supported(),
  /// PLATFORM-PROCESS-001 when the process cannot be started.
  [[nodiscard]] virtual diag::Outcome<ProcessOutput> run(const ProcessRequest& request) = 0;
};

/// The core default: every request reports "unsupported".
class UnsupportedProcessRunner final : public ProcessRunner {
 public:
  [[nodiscard]] bool supported() const noexcept override { return false; }
  [[nodiscard]] diag::Outcome<ProcessOutput> run(const ProcessRequest& request) override;
};

}  // namespace hefesto::platform
