// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Engine output, prompts and terminal input through the interpreter
// (openspec/changes/m4-scripting/design.md D6; capability scripting-api
// "Output, flushing and escaping", "Prompts and dialogs through the script
// layer", "Terminal input queue without console"; Q-SCRIPT-4):
//
//   - engine text is written with the interpreter's
//     `puts -nonewline stdout|stderr "<text>"`, with `"`, `[`, `]`, `\` and
//     `$` always escaped (Q-SCRIPT-4, fix in both magic-compat settings), so
//     a script or console that redefines `puts` captures it; flushing calls
//     `::tcl_flush` (the original Tcl `flush`, renamed by the binding);
//   - a line of input is asked with `magic::dialog "" "<prefix>"` when a
//     console owns the terminal, otherwise the prefix is printed, flushed
//     and `gets stdin` read; a multiple-choice question evaluates
//     `tk_dialog .dialog "Dialog" "<prompt>" {} <default> "<choice>" ...`;
//   - without a console, the bytes a layout window queues (keys typed after
//     the `:` macro) are read before the terminal (TerminalInputQueue).
//
// Ported from Magic 8.3.694 (fc5daa9e) tcltk/tclmagic.c Tcl_printf,
// Tcl_stdflush, TxDialog, TxGetLinePfix, the terminal channel driver
// (Magic ref: tcltk/tclmagic.c:984-1102,1180-1283,1335-1599), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: BINDING (design D2).

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cmd/prompt.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::script {

/// Where the interpreter's engine output goes (editor::EditorState::script
/// .output).
struct OutputRouting {
  bool console = false;    // a console (desktop Tcl console) owns stdout/stderr
  bool suspended = false;  // magic::suspendout / resumeout
};

enum class OutputStream : unsigned char { out, err };

/// `text` with `"`, `[`, `]`, `\` and `$` backslash-escaped, as the binding
/// passes engine text to `puts`.
[[nodiscard]] std::string escape_output(std::string_view text);

/// The script that prints `text`: `puts -nonewline stdout|stderr "<escaped>"`.
[[nodiscard]] std::string puts_script(OutputStream stream, std::string_view text);

/// Prints `text` through the interpreter's `puts` and flushes the stream
/// with `::tcl_flush` (when it exists). Nothing is printed while
/// `routing.suspended`. Returns the evaluation's failure, if any (a `puts`
/// redefined to fail).
cmd::CommandResult write_output(Interpreter& interp, const OutputRouting& routing, OutputStream stream,
                                std::string_view text);

/// A diagnostic sink that prints each diagnostic through the interpreter
/// (Magic's TxPrintf / TxError): errors and warnings on stderr, the rest
/// on stdout, rendered in `format` (native-only diagnostics are dropped in
/// the `magic` format), followed by a newline unless `no_newline`. Front
/// ends attach it to the dispatcher instead of printing diagnostics
/// themselves. Command thread only (never a live sink).
class ScriptOutputSink final : public diag::DiagnosticSink {
 public:
  ScriptOutputSink(Interpreter& interp, const OutputRouting& routing, diag::Format format)
      : interp_(interp), routing_(routing), format_(format) {}

  void emit(const diag::Diagnostic& diagnostic) override;

 private:
  Interpreter& interp_;
  const OutputRouting& routing_;
  diag::Format format_;
};

/// One line of user input through the script layer (TxGetLinePfix): with a
/// console `magic::dialog "" "<prefix>"` (or `magic::dialog` without a
/// prefix); without one the prefix is printed and flushed, then
/// `gets stdin` is read. One trailing newline is removed and the answer is
/// cut to `max_length - 1` characters (0 = no limit). nullopt: no input
/// (an empty answer or a failed evaluation).
[[nodiscard]] std::optional<std::string> read_line(Interpreter& interp, const OutputRouting& routing,
                                                   std::string_view prefix, std::size_t max_length = 0);

/// cmd::PromptHost answered by the script layer (TxDialog): evaluates
/// `tk_dialog .dialog "Dialog" "<escaped prompt>" {} <default> "<choice>" ...`
/// and returns its integer result. try_ask() gives nullopt when the
/// evaluation fails or the result is not a non-negative integer (Magic's -1,
/// Q-SCRIPT-48); ask() then gives the default, as for an integer out of range.
class ScriptPromptHost final : public cmd::PromptHost {
 public:
  explicit ScriptPromptHost(Interpreter& interp) : interp_(interp) {}

  [[nodiscard]] std::size_t ask(std::string_view question, std::span<const std::string> answers,
                                std::size_t default_answer) override;
  [[nodiscard]] std::optional<std::size_t> try_ask(std::string_view question, std::span<const std::string> answers,
                                                   std::size_t default_answer) override;

 private:
  Interpreter& interp_;
};

/// The standard-input queue of a session without a console (Magic's
/// terminal channel driver): bytes queued by a layout window (keys typed
/// after the `:` macro) are served before the terminal; bytes the terminal
/// had already buffered when the queue was installed come first of all.
class TerminalInputQueue {
 public:
  /// Reads from the terminal: returns some bytes, or nullopt at end of
  /// input. An interrupted read (EINTR) is the reader's to retry.
  using Reader = std::function<std::optional<std::string>()>;

  /// `buffered`: what the original channel already held.
  explicit TerminalInputQueue(Reader terminal, std::string buffered = {});

  /// Queues bytes typed in a layout window.
  void queue(std::string_view bytes);

  /// Escape during `:` entry: removes the incomplete last line (complete
  /// lines stay queued) and returns how many characters were removed, so
  /// the caller can erase them with backspaces and restore the `% ` prompt.
  std::size_t cancel_partial_line();

  /// The next line (without its newline): queued bytes first, then the
  /// terminal; nullopt at end of input with nothing pending.
  [[nodiscard]] std::optional<std::string> next_line();

 private:
  Reader terminal_;
  std::string pending_;
};

}  // namespace hefesto::script
