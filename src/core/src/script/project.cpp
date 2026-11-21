// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/project.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/cmd/commands.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace hefesto::script {

ProjectInterpreter::ProjectInterpreter() = default;

ProjectInterpreter::~ProjectInterpreter() {
  // The interpreter's commands may refer to the other members: delete it
  // first (members are otherwise destroyed in reverse declaration order).
  interp.reset();
}

Interpreter& open_project_interpreter(editor::EditorState& editor, SandboxPolicy policy) {
  close_project_interpreter(editor);
  editor.script.sandbox = std::move(policy);
  editor.script.interp = create_project_interpreter(editor, editor.script.sandbox);
  // M8 (openspec/changes/m8-schematic/design.md D12): the `xschem` Tcl
  // compatibility command over the project's schematic session.
  schem::register_xschem_command(*editor.script.interp, editor.schem);
  return *editor.script.interp;
}

void close_project_interpreter(editor::EditorState& editor) noexcept {
  editor.script.interp.reset();
  editor.script.interceptors.clear();  // their commands lived in the interpreter
}

}  // namespace hefesto::script
