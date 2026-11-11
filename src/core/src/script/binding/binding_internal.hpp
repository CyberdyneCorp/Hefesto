// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internals of the command binding shared by bind.cpp, dispatch.cpp and
// procedures.cpp (openspec/changes/m4-scripting/design.md D6).
//
// Owner: BINDING (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script::detail {

/// What the binding's native commands refer to. The interpreter, registry
/// and dispatcher must outlive the commands (the interpreter is closed
/// before the dispatcher goes away).
struct Binding {
  Interpreter& interp;
  const cmd::CommandRegistry& registry;
  cmd::Dispatcher& dispatcher;

  [[nodiscard]] editor::EditorState* editor() const noexcept { return dispatcher.editor(); }
  [[nodiscard]] bool magic_compat() const;
};

/// A Tcl error with `message` (empty: Magic's error with no message).
/// The origin of an engine command a Tcl evaluation runs (security finding
/// S-01): `script` when the evaluation is a script's or a script file is on
/// the Tcl stack (a sourced file, a procedure a file defined), else the
/// evaluation's own origin (a line typed in the console, a GUI action).
[[nodiscard]] cmd::Origin command_origin(const Binding& b, const cmd::Invocation& call);

[[nodiscard]] cmd::CommandResult tcl_error(std::string message);

/// The native command `::magic::<name>` (and its global import): quiet
/// dispatch, the ok-status convention unless strict or raised, then the
/// tag callback (scripting-api "Command status and error signalling",
/// "Tag callback execution and substitution").
[[nodiscard]] cmd::CommandResult dispatch_command(const Binding& binding, const std::string& name,
                                                  const cmd::Invocation& call);

/// A command Magic implements in Tcl (cmd::kScriptClient): run through the
/// dispatcher; any failure is a Tcl error carrying its message, and no tag
/// callback runs (Magic's Tcl procedures are not dispatched by
/// _tcl_dispatch).
[[nodiscard]] cmd::CommandResult dispatch_procedure(const Binding& binding, const std::string& name,
                                                    const cmd::Invocation& call);

/// `<window path> <command> ?args?` (_tk_dispatch).
[[nodiscard]] cmd::CommandResult dispatch_window(const Binding& binding, const std::string& path,
                                                 const cmd::Invocation& call);

/// Runs the tag of `invoked` (if any) after an ok command whose result is
/// `result` (TagCallback). `window`: the path of a window-directed command.
[[nodiscard]] cmd::CommandResult run_tag(const Binding& binding, std::string_view invoked,
                                         const std::vector<std::string>& args, const std::optional<std::string>& window,
                                         cmd::CommandResult result);

/// `::hefesto::_rebase_cell_paths <old dir> <new dir>` behind the binding's
/// `cd` (DBRebaseCellPaths).
void rebase_cell_paths(editor::EditorState& editor, std::string_view old_dir, std::string_view new_dir);

}  // namespace hefesto::script::detail
