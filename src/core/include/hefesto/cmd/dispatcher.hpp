// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The dispatcher: the single entry point through which every front end runs
// commands (capability hefesto-architecture, requirement "Single command API";
// ui-command-dispatch "Command dispatch across global and client tables",
// "Command counter and post-command update", "Command logging").
//
// A command word is resolved in the `*window` table and the table of the
// target client (CommandRegistry::resolve). As in Magic's Tcl build, the
// name must be exact (no abbreviation, no case folding; Q-UI-41), except for
// a line typed in the desktop command bar (Origin::gui) while typed
// abbreviations are on: that word may be a unique prefix in any letter case
// (Magic's Lookup()). An unknown name fails with Tcl's message
// `invalid command name "W"`. With an editor session attached,
// every executed command is then written to the `logcommands` log (as
// typed), published to the observers (with the resolved command name), and
// followed by editor::post_command (counter, undo step, client updates,
// highlights, redisplay); an interrupt during the command adds
// `[Interrupted]`. A point forced by `setpoint` is dropped after the next
// interactive (gui, key, touch) command.
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/geometry.hpp"

namespace hefesto::cmd {

/// Tcl's error text for a word that names no command (Q-UI-41).
[[nodiscard]] std::string invalid_command_name(std::string_view word);

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

  /// Runs one command. An unknown name fails with CMD-DISPATCH-001
  /// (`invalid command name "W"`), an ambiguous abbreviation of a typed line
  /// with UI-DISPATCH-001; an empty name is a no-op.
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

  /// Q-UI-43: a script command with no point set by `setpoint` gets the
  /// point at screen (x, y) of the window under it (the topmost, color or
  /// layout), as Magic's Tcl dispatcher gives every command
  /// txCurrentPoint = (100, 100). Only the headless CLI sets it: the
  /// desktop shell's pointer is real. The point lasts for that one command.
  void set_default_point(std::optional<db::Point> pixel) noexcept { default_point_ = pixel; }

  /// Whether a line typed in the command bar (Origin::gui) may abbreviate a
  /// command name (Q-UI-41; on by default). Every other origin always needs
  /// the exact name; the headless CLI turns this off.
  void set_typed_abbreviations(bool on) noexcept { typed_abbreviations_ = on; }
  /// The lookup mode of a command from `origin`.
  [[nodiscard]] NameLookup name_lookup(Origin origin) const noexcept {
    return typed_abbreviations_ && origin == Origin::gui ? NameLookup::abbreviated : NameLookup::exact;
  }

  /// The file system that commands with origin `script` see instead of the
  /// session's (not owned; nullptr: the session's). The project's script
  /// binding installs a sandboxed one (script::SandboxedFileSystem; security
  /// finding S-02), so an engine command run from Tcl reads and writes only
  /// within the interpreter's granted roots.
  void set_script_files(platform::FileSystem* files) noexcept { script_files_ = files; }
  [[nodiscard]] platform::FileSystem* script_files() const noexcept { return script_files_; }
  /// The session's own file system.
  [[nodiscard]] platform::FileSystem& files() const noexcept { return services_.files; }

  [[nodiscard]] ScriptSession& session() noexcept { return session_; }
  [[nodiscard]] const CommandRegistry& registry() const noexcept { return registry_; }

 private:
  /// The client of the next command: `*window` when the default point is on
  /// a window's chrome, `color` when the command point is in a color window,
  /// else client().
  [[nodiscard]] std::string target_client() const;
  CommandResult run_handler(const CommandSpec& spec, const Invocation& invocation);
  void publish(const Invocation& invocation, const CommandResult& result);
  void after_command(const Invocation& typed, const Invocation& resolved, CommandResult& result);
  /// Sets the default point (set_default_point) when no point is set; true
  /// when it did, so that it is dropped after the command.
  bool place_default_point();
  void drop_default_point(const db::Point& saved_pixel);
  void print_command(const Invocation& invocation);
  /// magic-compat: Magic's Lookup() finds the word ambiguous (`id`, `get`).
  [[nodiscard]] bool magic_ambiguous(const std::string& word, const std::string& client) const;

  const CommandRegistry& registry_;
  platform::PlatformServices services_;
  platform::FileSystem* script_files_ = nullptr;
  settings::ProjectSettings& project_;
  ScriptSession& session_;
  editor::EditorState* editor_ = nullptr;
  std::string client_{kLayoutClient};
  bool typed_abbreviations_ = true;
  std::optional<db::Point> default_point_;
  bool chrome_point_ = false;  // the default point is on a window's chrome
  std::vector<CommandObserver*> observers_;
  std::vector<diag::DiagnosticSink*> sinks_;
  std::vector<diag::DiagnosticSink*> live_sinks_;
};

}  // namespace hefesto::cmd
