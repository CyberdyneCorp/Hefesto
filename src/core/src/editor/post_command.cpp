// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// editor::post_command (design.md D4). Owner: INTEGRATION; DISPATCH adds
// the client update handlers (task 7.2).

#include "hefesto/editor/post_command.hpp"

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/state.hpp"

namespace hefesto::editor {

void post_command(EditorState& editor, const cmd::Invocation& invocation) {
  if (invocation.name != "*bypass" && invocation.origin != cmd::Origin::internal) {
    ++editor.command_count;
  }
  (void)editor.undo.close_step();
  ui::run_client_updates(editor);
  editor.highlights.redraw(editor);
  editor.windows.update_display();
}

}  // namespace hefesto::editor
