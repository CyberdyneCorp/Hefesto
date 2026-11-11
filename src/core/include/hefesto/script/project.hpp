// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// script::ProjectInterpreter: the per-project script state,
// editor::EditorState::script (openspec/changes/m4-scripting/design.md D4;
// capability hefesto-scripting "One interpreter per open project",
// Q-SCRIPT-41). One EditorState is one open project, so two open projects
// have two interpreters with separate variables, procedures and
// `magic::` helper definitions.
//
// The struct is shared (owned by INTEGRATION); each member is owned by the
// workstream named beside it, which may change the member's own type in its
// own header but asks INTEGRATION (open item) to add or remove members here.
// Every member is touched by the command thread only, except
// Interpreter::request_interrupt.
//
// Lifetime: `interp` is created when the project opens
// (open_project_interpreter) and deleted when it closes
// (close_project_interpreter, or the EditorState destructor). It is deleted
// before the other members, because its commands may refer to them.

#pragma once

#include <memory>
#include <vector>

#include "hefesto/pcell/framework/state.hpp"
#include "hefesto/script/binding/output.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/binding/variables.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/pdk/helper_origins.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/script/pdk/widgets.hpp"
#include "hefesto/script/progress.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script {

struct ProjectInterpreter {
  ProjectInterpreter();
  ~ProjectInterpreter();
  ProjectInterpreter(const ProjectInterpreter&) = delete;
  ProjectInterpreter& operator=(const ProjectInterpreter&) = delete;

  // ---- TCL -----------------------------------------------------------------
  /// The project's Tcl interpreter; null while no project is open.
  std::unique_ptr<Interpreter> interp;
  /// The policy `interp` was created with (granted roots, removed commands,
  /// limits, synthesized env).
  SandboxPolicy sandbox;

  // ---- BINDING -------------------------------------------------------------
  TagTable tags;                // `tag` scripts per command
  PathVariableCache path_cache; // Q-SCRIPT-5 cache and warn-once set
  OutputRouting output;         // stdout/stderr routing, console flag

  // ---- PDKINT --------------------------------------------------------------
  TrustStore pdk_trust;                 // trusted PDK versions (session view)
  HelperOriginTracker helper_origins;   // Q-SCRIPT-41 origins of magic:: procs
  MenuModel menu_model;                 // menu edits of intercepted widget calls

  // ---- PCELL ---------------------------------------------------------------
  pcell::FrameworkState gencell;  // dialog model, toolkit menus

  // ---- INTEGRATION ---------------------------------------------------------
  ScriptEvents events;  // denied operations, skipped statements, consent, ...
  /// The progress hook of long-running evaluations (script/progress.hpp):
  /// set by the front end (the desktop pumps a frame for the Interrupt
  /// card), read by `interp` at the start of each top-level evaluation.
  ProgressReporting progress;
  /// The file system of the engine commands a script runs (security finding
  /// S-02; script::SandboxedFileSystem over `sandbox`), created by
  /// bind_registry and kept while the editor lives.
  std::unique_ptr<platform::FileSystem> script_files;
  /// The widget interceptors of the wrapper frames opened in this project
  /// (one per frame; gui::open_wrapper, gui::adopt_wrapper) and the file
  /// picker they ask, which forwards to the front end's picker.
  std::vector<std::unique_ptr<WidgetInterceptor>> interceptors;
  std::unique_ptr<platform::FilePicker> session_picker;
};

/// Creates the interpreter of `editor`'s project with `policy` (the project
/// opens), replacing (and first deleting) one that is already open.
Interpreter& open_project_interpreter(editor::EditorState& editor, SandboxPolicy policy);

/// Deletes the project's interpreter (the project closes); no-op when none.
void close_project_interpreter(editor::EditorState& editor) noexcept;

}  // namespace hefesto::script
