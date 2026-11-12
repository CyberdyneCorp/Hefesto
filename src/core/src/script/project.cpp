// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/project.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"
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
  return *editor.script.interp;
}

void close_project_interpreter(editor::EditorState& editor) noexcept {
  editor.script.interp.reset();
}

}  // namespace hefesto::script
