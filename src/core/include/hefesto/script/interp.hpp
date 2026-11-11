// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// script::Interpreter: the handle of one project's Tcl interpreter
// (openspec/changes/m4-scripting/design.md D4; capability hefesto-scripting
// "One interpreter per open project", "Host-independent scripting binding").
// The interface is Tcl-free: no file outside src/core/src/script/tcl/
// includes tcl.h. Values cross as cmd::Value (lists stay lists).
//
// Obtained only from script::tcl::create_project_interpreter (sandboxed,
// design D5) and kept in editor::EditorState::script.interp. Not thread-safe:
// every call except request_interrupt() comes from the command thread.
//
// Owner: INTEGRATION (this interface); TCL implements it
// (src/core/src/script/tcl/).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/value.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::script {

/// A native command defined in the interpreter. It receives the words as
/// called: `name` is the command word as written (e.g. "::magic::box" or
/// "box"), `args` the remaining words after substitution, origin `script`
/// and `source` the calling script location when known. An ok result's value
/// becomes the Tcl result; a failed result raises a Tcl error whose message
/// is the first diagnostic's text. Magic's quiet, non-strict failures
/// (design D6) are therefore decided by the command itself: BINDING's
/// wrappers return ok with an empty value unless `raises` or strict mode.
using NativeCommand = std::function<cmd::CommandResult(const cmd::Invocation&)>;

class Interpreter {
 public:
  Interpreter() = default;
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;
  virtual ~Interpreter() = default;

  /// Evaluates `script` at global level. ok with the interpreter's result
  /// as the value, or failed with one SCRIPT-TCL-* error diagnostic holding
  /// the Tcl error message (and the error location when known). `origin` is
  /// recorded for the commands the script runs.
  [[nodiscard]] virtual cmd::CommandResult evaluate(std::string_view script,
                                                    cmd::Origin origin = cmd::Origin::script) = 0;

  /// Tcl's `source` of `path` (through the sandbox's filesystem layer), so
  /// that `info script` names the file while it runs.
  [[nodiscard]] virtual cmd::CommandResult source(const platform::Path& path) = 0;

  /// Sets a global (or namespace-qualified) variable; false with the Tcl
  /// error left in `error` when the variable cannot be set (a trace refused).
  virtual bool set_var(std::string_view name, const cmd::Value& value, std::string* error = nullptr) = 0;

  /// The variable's value, or nullopt when it does not exist.
  [[nodiscard]] virtual std::optional<cmd::Value> get_var(std::string_view name) const = 0;

  /// Defines (or replaces) the command `name` (a fully qualified name such
  /// as "::magic::box" creates the namespace when needed).
  virtual void define_command(std::string_view name, NativeCommand command) = 0;

  /// True when a command (native, built-in or proc) named `name` exists.
  [[nodiscard]] virtual bool has_command(std::string_view name) const = 0;

  /// `info script`: the file being sourced right now, or empty.
  [[nodiscard]] virtual std::string info_script() const = 0;

  /// Asks the running evaluation to stop at its next command (the resource
  /// limit of design D5); the evaluation fails with a SCRIPT-TCL-* error.
  /// The only call allowed from another thread.
  virtual void request_interrupt() noexcept = 0;
};

}  // namespace hefesto::script
