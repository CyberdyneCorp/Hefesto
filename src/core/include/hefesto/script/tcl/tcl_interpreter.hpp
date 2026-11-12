// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The only way to obtain a script::Interpreter
// (openspec/changes/m4-scripting/design.md D4/D5): a full Tcl 8.6
// interpreter per open project (capability hefesto-scripting "One
// interpreter per open project"), initialized from the embedded library
// (init.tcl evaluated from memory, `tcl_library` = kVirtualLibraryRoot,
// `auto_path` and the Tcl-module path limited to it, no host Tcl file read)
// and restricted by the SandboxPolicy before the caller gets it (sandbox.hpp):
// removed and restricted commands, the filesystem layer limited to the
// granted roots, the synthesized `env`, and the evaluation limits.
//
// Results: Interpreter::evaluate and ::source fail with SCRIPT-TCL-001 (a
// Tcl error, located at Tcl's -errorline and the sourced file),
// SCRIPT-TCL-002 (interrupted through request_interrupt) or SCRIPT-TCL-003
// (a SandboxLimits limit was exceeded); `catch` cannot absorb the last two.
// The SCRIPT-SANDBOX-* diagnostics of the denials the evaluation caused
// follow, also in an ok result (a denial the script caught). Native commands
// receive the Origin of the evaluate call that runs them.
//
// Owner: TCL (design D2).

#pragma once

#include <memory>

#include "hefesto/script/interp.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script {

/// Creates the interpreter of the project whose session is `editor`.
/// Never null; Tcl's process-wide initialization (Tcl_FindExecutable) runs
/// once on first use.
/// Denials are published to `editor.script.events`, which must outlive the
/// interpreter (ProjectInterpreter deletes `interp` first).
[[nodiscard]] std::unique_ptr<Interpreter> create_project_interpreter(editor::EditorState& editor,
                                                                      const SandboxPolicy& policy);

/// The script file and line of the command `interpreter` is executing (Tcl's
/// `info frame 0`, then `info script` for the file); empty file and line 0
/// outside an evaluation or for an interpreter not made here. For native
/// commands that report a diagnostic at the calling script line.
[[nodiscard]] ScriptLocation current_script_location(const Interpreter& interpreter);

}  // namespace hefesto::script
