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

  /// Live sinks see each diagnostic a handler emits through its context at
  /// the moment it is emitted, while the command still runs (for progress
  /// displays such as `pdk install -progress`), in addition to the normal
  /// publication to the sinks above after the command returns. They are
  /// called on the emitting thread (a handler's worker thread included),
  /// before the command's script location is attached, and never for the
  /// diagnostics a handler returns in its result. Do not add or remove live
  /// sinks while a command runs.
  void add_live_sink(diag::DiagnosticSink& sink) { live_sinks_.push_back(&sink); }
  void remove_live_sink(const diag::DiagnosticSink& sink) { std::erase(live_sinks_, &sink); }

  /// Attaches the M1 session state passed to every handler (not owned; must
  /// outlive the dispatcher or be detached with nullptr).
  void set_editor(editor::EditorState* editor) noexcept { editor_ = editor; }
  [[nodiscard]] editor::EditorState* editor() const noexcept { return editor_; }

  [[nodiscard]] ScriptSession& session() noexcept { return session_; }
  [[nodiscard]] const CommandRegistry& registry() const noexcept { return registry_; }

 private:
  CommandResult run_handler(const Invocation& invocation);
  void publish(const Invocation& invocation, const CommandResult& result);

  const CommandRegistry& registry_;
  platform::PlatformServices services_;
  settings::ProjectSettings& project_;
  ScriptSession& session_;
  editor::EditorState* editor_ = nullptr;
  std::vector<CommandObserver*> observers_;
  std::vector<diag::DiagnosticSink*> sinks_;
  std::vector<diag::DiagnosticSink*> live_sinks_;
};

}  // namespace hefesto::cmd
