// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The narrow interface through which the desktop shell sends command text to
// the core (capability hefesto-architecture, requirement "Single command
// API"; design.md D5/D6). The shell never calls algorithms directly: the
// command bar, menus and shortcuts all end up here.
//
// The adapter to the core is CoreCommandSink (core_command_sink.hpp): it
// evaluates the line as Tcl in the project's interpreter with origin `gui`
// and maps the result and the diagnostics to a CommandOutcome.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hefesto::shell {

enum class OutcomeStatus { ok, failed };

/// What the shell shows after a command: status, one line for the status bar,
/// and any diagnostics already rendered as text ("error: CODE: text").
struct CommandOutcome {
  OutcomeStatus status = OutcomeStatus::ok;
  std::string message;
  std::vector<std::string> diagnostics;
  /// The non-empty results of the line's commands that succeeded, in order,
  /// one per line (`help; paint x` keeps the help text when paint fails).
  std::string output;
  /// What the line's commands printed (their info diagnostics' text: `help`,
  /// `what`), one per line; the status bar summarizes it when there is no
  /// result value.
  std::string printed;

  [[nodiscard]] bool ok() const noexcept { return status == OutcomeStatus::ok; }
};

class CommandSink {
 public:
  virtual ~CommandSink() = default;
  /// Executes one command line (already trimmed, never empty).
  virtual CommandOutcome submit(std::string_view line) = 0;

 protected:
  CommandSink() = default;
  CommandSink(const CommandSink&) = default;
  CommandSink& operator=(const CommandSink&) = default;
};

}  // namespace hefesto::shell
