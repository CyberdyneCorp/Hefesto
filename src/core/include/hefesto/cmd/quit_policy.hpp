// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What an accepted `quit` does (capability app-startup "quit command and
// exit status"; decision D-36; openspec/changes/m4-scripting/design.md D7).
// `quit` parses its arguments and runs the exit checks (unless -noprompt);
// the session's QuitPolicy then decides what quitting means on the host:
//   - the desktop and hefesto-cli end the application with the status
//     (ExitQuitPolicy, the default when ScriptSession::quit_policy is null);
//   - iPadOS cannot end its own process: the policy stops the script,
//     closes the project and returns to the project browser with a
//     diagnostic naming the script and line (app::ProjectBrowserQuitPolicy;
//     the iPad shell installs it in M14).
// Either way ScriptSession::quit_requested is set, so the running script and
// the startup sequence stop (the binding unwinds the Tcl evaluation).
//
// Owner: INTEGRATION.

#pragma once

#include "hefesto/cmd/command.hpp"

namespace hefesto::cmd {

class CommandContext;

/// An accepted `quit`: the exit checks passed or were skipped.
struct QuitRequest {
  int status = 0;         // 0-255
  bool noprompt = false;  // -noprompt: the exit checks were not run
};

class QuitPolicy {
 public:
  QuitPolicy() = default;
  QuitPolicy(const QuitPolicy&) = delete;
  QuitPolicy& operator=(const QuitPolicy&) = delete;
  virtual ~QuitPolicy() = default;

  /// Applies `request` for the command `invocation`; the result is `quit`'s.
  [[nodiscard]] virtual CommandResult quit(CommandContext& ctx, const Invocation& invocation,
                                           const QuitRequest& request) = 0;
};

/// The desktop and CLI policy: the session ends with the status (Magic's
/// windQuitCmd ending the process; the front end exits after the script).
class ExitQuitPolicy final : public QuitPolicy {
 public:
  [[nodiscard]] CommandResult quit(CommandContext& ctx, const Invocation& invocation,
                                   const QuitRequest& request) override;
};

}  // namespace hefesto::cmd
