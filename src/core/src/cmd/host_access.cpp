// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/host_access.hpp"

#include <string>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::cmd {

namespace {

// The script area's denials (docs/diagnostics/codes/script.md).
constexpr std::string_view kProcessDenied = "SCRIPT-SANDBOX-001";
constexpr std::string_view kTrustDenied = "SCRIPT-SANDBOX-008";

}  // namespace

bool may_reach_host(Origin origin) noexcept {
  return origin != Origin::script && origin != Origin::key && origin != Origin::plugin;
}

std::optional<CommandResult> refuse_host_access(const Invocation& invocation, std::string_view what,
                                                HostAccess access) {
  if (may_reach_host(invocation.origin)) {
    return std::nullopt;
  }
  const std::string_view code = access == HostAccess::trust ? kTrustDenied : kProcessDenied;
  return CommandResult::raised(diag::make(code, std::string{what} + " is not permitted in the script sandbox"));
}

CommandResult refuse_process_execution(std::string_view what) {
  return CommandResult::raised(diag::make(
      kProcessDenied, std::string{what} + ": process execution is not permitted (Hefesto runs no external programs)"));
}

}  // namespace hefesto::cmd
