// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What a command handler can reach: platform services, the diagnostic sink
// of the running command, the project settings and the script session.

#pragma once

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cmd {

class CommandRegistry;

/// Per-interpreter state. In M0 there is one session per CLI run or test;
/// the Tcl binding (X-3) creates one per interpreter.
struct ScriptSession {
  explicit ScriptSession(const settings::ProjectSettings& project) : strict(project.script_strict) {}

  bool strict = false;          // script strict mode for this interpreter
  bool quit_requested = false;  // set by `quit`
};

class CommandContext {
 public:
  CommandContext(platform::PlatformServices services, settings::ProjectSettings& project, ScriptSession& session,
                 diag::DiagnosticSink& sink, const CommandRegistry& registry,
                 editor::EditorState* editor = nullptr)
      : services_(services), project_(project), session_(session), sink_(sink), registry_(registry),
        editor_(editor) {}

  [[nodiscard]] platform::PlatformServices& services() noexcept { return services_; }
  [[nodiscard]] settings::ProjectSettings& project() noexcept { return project_; }
  [[nodiscard]] ScriptSession& session() noexcept { return session_; }
  [[nodiscard]] const CommandRegistry& registry() const noexcept { return registry_; }
  /// The M1 session state (technology, cells, windows, ...); nullptr when the
  /// front end runs without one (M0 tests). Handlers that need it fail with
  /// CMD-USAGE-001 "no editor session" when it is absent (cmd::require_editor).
  [[nodiscard]] editor::EditorState* editor() const noexcept { return editor_; }

  /// Emits a diagnostic; it is attached to the running command's result.
  void emit(const diag::Diagnostic& diagnostic) { sink_.emit(diagnostic); }
  [[nodiscard]] diag::DiagnosticSink& sink() noexcept { return sink_; }

 private:
  platform::PlatformServices services_;
  settings::ProjectSettings& project_;
  ScriptSession& session_;
  diag::DiagnosticSink& sink_;
  const CommandRegistry& registry_;
  editor::EditorState* editor_;
};

}  // namespace hefesto::cmd
