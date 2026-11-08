// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The dispatcher: the single entry point through which every front end runs
// commands (capability hefesto-architecture, requirement "Single command API").

#pragma once

#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::cmd {

/// Notified after every executed command: command log, history, replay.
class CommandObserver {
 public:
  CommandObserver() = default;
  CommandObserver(const CommandObserver&) = delete;
  CommandObserver& operator=(const CommandObserver&) = delete;
  virtual ~CommandObserver() = default;

  virtual void on_command(const Invocation& invocation, const CommandResult& result) = 0;
};

class Dispatcher {
 public:
  Dispatcher(const CommandRegistry& registry, platform::PlatformServices services,
             settings::ProjectSettings& project, ScriptSession& session);

  /// Runs one command. An unknown name fails with CMD-DISPATCH-001.
  /// The result carries every diagnostic emitted while the command ran
  /// (emitted through the context first, then those the handler returned).
  /// Each diagnostic is also forwarded to the attached sinks and the logger.
  CommandResult execute(const Invocation& invocation);

  /// Forwards a diagnostic raised outside any command (e.g. a script parse
  /// error) to the attached sinks and the logger.
  void report(const diag::Diagnostic& diagnostic);

  /// Observers and sinks are not owned; each must stay alive until the
  /// dispatcher is destroyed or it is removed again.
  void add_observer(CommandObserver& observer) { observers_.push_back(&observer); }
  void add_sink(diag::DiagnosticSink& sink) { sinks_.push_back(&sink); }
  void remove_sink(const diag::DiagnosticSink& sink) { std::erase(sinks_, &sink); }

  [[nodiscard]] ScriptSession& session() noexcept { return session_; }
  [[nodiscard]] const CommandRegistry& registry() const noexcept { return registry_; }

 private:
  CommandResult run_handler(const Invocation& invocation);
  void publish(const Invocation& invocation, const CommandResult& result);

  const CommandRegistry& registry_;
  platform::PlatformServices services_;
  settings::ProjectSettings& project_;
  ScriptSession& session_;
  std::vector<CommandObserver*> observers_;
  std::vector<diag::DiagnosticSink*> sinks_;
};

}  // namespace hefesto::cmd
