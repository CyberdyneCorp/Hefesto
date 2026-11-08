// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The narrow interface through which the desktop shell sends command text to
// the core (capability hefesto-architecture, requirement "Single command
// API"; design.md D5/D6). The shell never calls algorithms directly: the
// command bar, menus and shortcuts all end up here.
//
// M0 binding: the core command API (hefesto/cmd/, design.md D6) is being
// written in parallel, so the shell codes against this interface. Binding it
// is one adapter class that splits the line into an Invocation with origin
// `gui`, calls Dispatcher::execute and maps CommandResult to CommandOutcome.

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
