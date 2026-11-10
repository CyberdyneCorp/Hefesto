// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The dispatcher: the single entry point through which every front end runs
// commands (capability hefesto-architecture, requirement "Single command API";
// ui-command-dispatch "Command dispatch across global and client tables",
// "Command counter and post-command update", "Command logging").
//
// A command word is resolved in the `*window` table and the table of the
// target client (CommandRegistry::resolve): unique prefixes and any letter
// case are accepted, an exact name wins. With an editor session attached,
// every executed command is then written to the `logcommands` log (as
// typed), published to the observers (with the resolved command name), and
// followed by editor::post_command (counter, undo step, client updates,
// highlights, redisplay); an interrupt during the command adds
// `[Interrupted]`. A point forced by `setpoint` is dropped after the next
// interactive (gui, key, touch) command.
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <string>
#include <utility>
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

  /// Runs one command. An unknown name fails with CMD-DISPATCH-001, an
  /// ambiguous abbreviation with UI-DISPATCH-001; an empty name is a no-op.
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
  /// Attaches the session state; its undo recorder then reads magic-compat
  /// from this dispatcher's project settings.
  void set_editor(editor::EditorState* editor) noexcept;
  [[nodiscard]] editor::EditorState* editor() const noexcept { return editor_; }

  /// The client commands are sent to (`layout` by default; a front end sets
  /// the client of the window that has the focus, e.g. `color`).
  void set_client(std::string client) { client_ = std::move(client); }
  [[nodiscard]] const std::string& client() const noexcept { return client_; }

  [[nodiscard]] ScriptSession& session() noexcept { return session_; }
  [[nodiscard]] const CommandRegistry& registry() const noexcept { return registry_; }

 private:
  CommandResult run_handler(const CommandSpec& spec, const Invocation& invocation);
  void publish(const Invocation& invocation, const CommandResult& result);
  void after_command(const Invocation& typed, const Invocation& resolved, CommandResult& result);

  const CommandRegistry& registry_;
  platform::PlatformServices services_;
  settings::ProjectSettings& project_;
  ScriptSession& session_;
  editor::EditorState* editor_ = nullptr;
  std::string client_{kLayoutClient};
  std::vector<CommandObserver*> observers_;
  std::vector<diag::DiagnosticSink*> sinks_;
  std::vector<diag::DiagnosticSink*> live_sinks_;
};

}  // namespace hefesto::cmd
