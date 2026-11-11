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
//   - between the commands of a long run it calls the pump, so the window
//     keeps drawing and the Interrupt card and Ctrl-C reach
//     request_interrupt().
//
// The command bar sends its lines to the same TclConsole (one interpreter,
// one history).
//
// Owner: SHELL (task 10.5).

#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
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

  /// Called between commands while an evaluation runs (at most every
  /// `interval`): the shell draws a frame there.
  void set_pump(std::function<void()> pump, std::chrono::milliseconds interval = std::chrono::milliseconds{50});
  /// The Dispatcher ran a command: pumps when an evaluation runs and the
  /// interval has passed.
  void on_command();
  [[nodiscard]] bool evaluating() const noexcept { return depth_ > 0; }

  // ConsoleEvaluator
  [[nodiscard]] bool complete(std::string_view text) override;
  ConsoleEvaluation evaluate(std::string_view command) override;
  void interrupt() noexcept override;
  [[nodiscard]] std::size_t undo_steps() const override;

  /// The command bar's outcome for a line the console ran (status bar).
  [[nodiscard]] static CommandOutcome outcome_of(const ConsoleEvaluation& evaluation);

 private:
  class Capture;
  cmd::CommandResult console_puts(const cmd::Invocation& inv);
  void write(ConsoleLineKind kind, std::string_view text, bool newline);

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
};

}  // namespace hefesto::shell
