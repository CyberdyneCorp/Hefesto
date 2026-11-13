// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/binding/commands.hpp"

#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/binding/tags.hpp"

namespace hefesto::script {

namespace {

// `tag` reached through the command API (the CLI's typed lines, a non-Tcl
// host): the same table and rules as the interpreter's `magic::tag`.
cmd::CommandResult tag_handler(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  editor::EditorState* editor = ctx.editor();
  if (editor == nullptr) {
    return cmd::CommandResult::raised(diag::make("SCRIPT-TCL-001", ""));
  }
  return tag_command(editor->script.tags, invocation.args);
}

}  // namespace

bool register_binding_commands(cmd::CommandRegistry& registry) {
  // `tag` is a Tcl command of tclmagic.c (AddCommandTag), outside Magic's
  // window command tables: exact-name table cmd::kScriptClient.
  return !registry
                 .add(cmd::CommandSpec{"tag", "tag [add|replace] <command> [<script>]",
                                       "Attach a Tcl script to run after <command> succeeds (%-substitutions).",
                                       tag_handler},
                      cmd::kScriptClient)
                 .has_value();
}

}  // namespace hefesto::script
