// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Commands that reach the host beyond files (run a process, download, change
// which PDK scripts are trusted or where PDKs live) refuse invocations that
// a script made (decision X-3; hefesto-security "Script sandbox
// enforcement"; docs/contributing/security.md findings S-01 and S-03).
//
// The origin of a command run from Tcl is `script` when the evaluation is a
// script's or a script file is on the Tcl stack, and the evaluation's own
// origin otherwise (a line typed in the console, a GUI action); see
// script/binding/dispatch.cpp. Key macros can be defined by scripts, so they
// count as scripts here.
//
// Owner: INTEGRATION (design D2, cmd/ other files).

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "hefesto/cmd/command.hpp"

namespace hefesto::cmd {

/// True when a command from `origin` may run processes or change trust:
/// typed lines, GUI actions and the command line, not scripts, key macros
/// or plugins.
[[nodiscard]] bool may_reach_host(Origin origin) noexcept;

/// What a refused command would have done.
enum class HostAccess : std::uint8_t {
  process,  // run a process or download (SCRIPT-SANDBOX-001)
  trust,    // change PDK trust or the PDK store (SCRIPT-SANDBOX-008)
};

/// The refusal of `what` (e.g. "shell: process execution") for a script
/// invocation, raised as a Tcl error whatever the strict mode; nullopt when
/// `invocation` may reach the host.
[[nodiscard]] std::optional<CommandResult> refuse_host_access(const Invocation& invocation, std::string_view what,
                                                              HostAccess access = HostAccess::process);

}  // namespace hefesto::cmd
