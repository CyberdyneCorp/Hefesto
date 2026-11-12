// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl console bound to the project's interpreter
// (docs/design/mockups/M4-Console.dc.html; design.md D4: the console uses
// script::Interpreter::evaluate / request_interrupt; capability
// scripting-api "Output, flushing and escaping"). TclConsoleSession is the
// ConsoleEvaluator of the desktop:
//
//   - attach() replaces `::puts` (Tcl's own is kept as
//     `::hefesto::tcl_puts`) so that `puts` to stdout and stderr, and the
//     engine text the binding prints through `puts` (literal `$`,
//     Q-SCRIPT-4), land in the console transcript in order; other channels
//     go to Tcl's `puts`;
//   - while it evaluates, the engine's diagnostics (a magic:: command's
//     warnings and errors, sandbox denials) are also transcript lines;
//   - while any evaluation of the project runs (the console's, a menu
//     callback, the PDK startup) it calls the pump: from the interpreter's
//     progress handler every interval, also inside a pure Tcl loop
//     (script::ProjectInterpreter::progress, which the session installs),
//     and between engine commands. The window keeps drawing, and the
//     Interrupt card and Ctrl-C reach request_interrupt(). Everything runs
//     on the UI thread, which owns the interpreter (no thread is created);
//     the pump must only draw and take the Interrupt input, and a command
//     typed meanwhile is queued (TclConsole, ConsoleEvaluator::busy);
//   - the edits of an interrupted console command become one undo step
//     named after the command (undo::UndoLog::merge_since).
//
// The command bar sends its lines to the same TclConsole (one interpreter,
// one history).
//
// Owner: SHELL (task 10.5).

#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/progress.hpp"
#include "hefesto/shell/command_sink.hpp"
#include "hefesto/shell/tcl_console.hpp"

namespace hefesto::shell {

class TclConsoleSession final : public ConsoleEvaluator {
 public:
  TclConsoleSession(editor::EditorState& editor, cmd::Dispatcher& dispatcher);
  ~TclConsoleSession() override;
  TclConsoleSession(const TclConsoleSession&) = delete;
  TclConsoleSession& operator=(const TclConsoleSession&) = delete;

  /// Where `puts` and engine messages go (the console's transcript).
  void set_transcript(ConsoleTranscript* transcript) noexcept { transcript_ = transcript; }
  /// Installs the console's `puts` in the project's interpreter (call once
  /// the interpreter exists: app::StartupEnvironment::configure_interpreter).
  /// False when the interpreter cannot rename `puts` (Tcl's stays).
  bool attach(script::Interpreter& interp);
  [[nodiscard]] bool attached() const noexcept { return attached_ != nullptr; }
  /// The interpreter of the open project, or null.
  [[nodiscard]] script::Interpreter* interpreter() const noexcept;

  /// Called while an evaluation runs, about every `interval` (the
  /// project's progress interval is set to it): the shell draws a frame
  /// there.
  void set_pump(std::function<void()> pump, std::chrono::milliseconds interval = std::chrono::milliseconds{50});
  /// The Dispatcher ran a command: pumps when an evaluation runs and the
  /// interval has passed.
  void on_command();
  /// The console's own evaluation runs.
  [[nodiscard]] bool evaluating() const noexcept { return depth_ > 0; }
  /// The last progress report of the evaluation running now (any origin),
  /// or nullopt when none runs or it has not reported yet (the first
  /// report comes one interval after it starts). The canvas Interrupt card
  /// (shell::running_script, interrupt_card) is built from it.
  [[nodiscard]] const std::optional<script::ScriptProgress>& progress() const noexcept { return progress_; }
  /// The interpreter's progress handler (installed by the constructor).
  void on_progress(const script::ScriptProgress& progress);

  // ConsoleEvaluator
  [[nodiscard]] bool complete(std::string_view text) override;
  ConsoleEvaluation evaluate(std::string_view command) override;
  void interrupt() noexcept override;
  [[nodiscard]] std::size_t undo_steps() const override;
  /// Another evaluation (not the console's) is running and reporting.
  [[nodiscard]] bool busy() const override { return depth_ == 0 && progress_.has_value(); }
  /// `info commands` / `info vars` of `glob` in the project's interpreter.
  [[nodiscard]] std::vector<std::string> names(TclWordKind kind, std::string_view glob) override;

  /// The command bar's outcome for a line the console ran (status bar).
  [[nodiscard]] static CommandOutcome outcome_of(const ConsoleEvaluation& evaluation);

 private:
  class Capture;
  cmd::CommandResult console_puts(const cmd::Invocation& inv);
  void write(ConsoleLineKind kind, std::string_view text, bool newline);
  void pump_now();

  editor::EditorState& editor_;
  cmd::Dispatcher& dispatcher_;
  ConsoleTranscript* transcript_ = nullptr;
  script::Interpreter* attached_ = nullptr;
  std::string pending_out_;  // `puts -nonewline` text not yet ended by a newline
  ConsoleLineKind pending_kind_ = ConsoleLineKind::out;
  std::function<void()> pump_;
  std::chrono::milliseconds pump_interval_{50};
  std::chrono::steady_clock::time_point last_pump_{};
  int depth_ = 0;
  bool pumping_ = false;
  std::optional<script::ScriptProgress> progress_;
  std::string last_where_;  // "file:line" of the last report of the ended run
};

/// "rc.tcl:12" (the file name only), "line 12" without a file, empty when
/// the line is unknown.
[[nodiscard]] std::string script_where(const script::ScriptLocation& location);

}  // namespace hefesto::shell
