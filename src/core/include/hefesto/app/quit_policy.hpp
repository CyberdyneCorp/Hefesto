// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The iPadOS quit policy (capability app-startup "quit command and exit
// status", scenario "PDK guard quit on iPadOS"; decision D-36;
// openspec/changes/m4-scripting/design.md D7). An iPad app cannot end its
// own process, so an accepted `quit` (the exit checks passed, or
// -noprompt) closes the current project instead:
//   - the running script stops: ScriptSession::quit_requested is set, so
//     the binding unwinds the Tcl evaluation and the startup sequence ends;
//     the exit status is not applied;
//   - the command reports APP-QUIT-001 naming the script and line that
//     called `quit` (the invocation's location, else the project
//     interpreter's current script location);
//   - the host is told to discard the project's session and show the
//     project browser with that diagnostic. It must do so after the command
//     returns (the session is in use while the script unwinds), never from
//     inside ProjectBrowserHost::close_project.
// The desktop and the CLI keep cmd::ExitQuitPolicy. The iPad shell installs
// this policy in M14 (ScriptSession::quit_policy); M4 tests it with a test
// host.
//
// Owner: INTEGRATION.

#pragma once

#include "hefesto/cmd/quit_policy.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::app {

/// The front end that owns the open project (the iPad shell; a test host).
class ProjectBrowserHost {
 public:
  ProjectBrowserHost() = default;
  ProjectBrowserHost(const ProjectBrowserHost&) = delete;
  ProjectBrowserHost& operator=(const ProjectBrowserHost&) = delete;
  virtual ~ProjectBrowserHost() = default;

  /// Schedules closing the project and showing the project browser with
  /// `diagnostic` (APP-QUIT-001), once the running command has returned.
  virtual void close_project(const diag::Diagnostic& diagnostic) = 0;
};

class ProjectBrowserQuitPolicy final : public cmd::QuitPolicy {
 public:
  explicit ProjectBrowserQuitPolicy(ProjectBrowserHost& host) : host_(host) {}

  [[nodiscard]] cmd::CommandResult quit(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                                        const cmd::QuitRequest& request) override;

 private:
  ProjectBrowserHost& host_;
};

}  // namespace hefesto::app
