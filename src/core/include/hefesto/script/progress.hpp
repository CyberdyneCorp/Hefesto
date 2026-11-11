// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// script::ScriptProgress: the progress of a long-running script evaluation
// (openspec/changes/m4-scripting/design.md D5 "Resources" and "Long-running
// scripts"; capability hefesto-scripting "Long-running script progress and
// interrupt"; boards M4-Script-Consent tile 7 and M4-Console tiles 6-8).
//
// Threading model: a script runs on the thread that owns its interpreter,
// which on the desktop is the UI thread (Tcl interpreters are bound to the
// thread that created them, and the engine commands a script runs edit the
// session the UI draws). While it runs, the interpreter's time-limit handler
// calls ProgressReporting::handler every `interval` from inside the
// evaluation, on that same thread, with the file and line being executed.
// The front end pumps its events and draws a frame there (the Interrupt
// card, the console's running header), so the Interrupt button and Ctrl-C
// reach Interpreter::request_interrupt; the running evaluation then stops at
// its next command with SCRIPT-TCL-002. No thread is created.
//
// The handler must not evaluate a script in the same interpreter: input
// that would run a command is queued by the front end until the evaluation
// ends (the board's "takes no input until it ends"). A handler that does
// evaluate anyway gets no progress for that nested evaluation.
//
// Owner: TCL (design D2; ProjectInterpreter::progress is INTEGRATION's
// member, this type is TCL's).

#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::script {

/// One progress report of a running top-level evaluation.
struct ScriptProgress {
  /// The file the top-level evaluation sources (Interpreter::source), or
  /// empty for evaluated text (a console line, a menu callback).
  std::string script;
  /// The command executing now: its script file (or the sourced file when
  /// the frame has none) and line; empty file and line 0 when unknown.
  ScriptLocation location;
  /// Time since the top-level evaluation started.
  std::chrono::steady_clock::duration elapsed{};
  /// The origin the evaluation was started with (gui: console and command
  /// bar; script: PDK files and menu callbacks; cli).
  cmd::Origin origin = cmd::Origin::script;
  /// True for the last report, sent once when an evaluation that reported
  /// progress ends (interrupted or not); the front end hides its card.
  bool done = false;
};

using ProgressHandler = std::function<void(const ScriptProgress&)>;

/// The progress hook of one project (ProjectInterpreter::progress). It
/// outlives the interpreter, so a reopened project keeps its handler.
struct ProgressReporting {
  /// Called every `interval` while a top-level evaluation runs; empty: no
  /// reports (the CLI), and evaluation is not slowed at all.
  ProgressHandler handler;
  /// The period of the reports (and so of the frames a front end draws).
  std::chrono::milliseconds interval{50};
};

}  // namespace hefesto::script
