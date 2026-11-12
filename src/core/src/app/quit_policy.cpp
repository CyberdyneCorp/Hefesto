// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/app/quit_policy.hpp"

#include <string>
#include <utility>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace hefesto::app {

namespace {

// Where `quit` was called: the invocation's own location (a script read
// line by line), else the line of the file the interpreter is evaluating.
diag::SourceLocation quit_location(const cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  if (invocation.source) {
    return *invocation.source;
  }
  const editor::EditorState* editor = ctx.editor();
  if (editor != nullptr && editor->script.interp != nullptr) {
    return script::current_script_location(*editor->script.interp);
  }
  return {};
}

std::string closed_text(const diag::SourceLocation& where) {
  std::string text = "The project was closed by \"quit\"";
  if (!where.file.empty()) {
    text += " in \"" + where.file + "\"";
    if (where.line != 0) {
      text += " line " + std::to_string(where.line);
    }
  }
  return text + "; iPadOS apps cannot quit, so the project browser is shown.";
}

}  // namespace

cmd::CommandResult ProjectBrowserQuitPolicy::quit(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                                                  const cmd::QuitRequest& /*request*/) {
  const diag::SourceLocation where = quit_location(ctx, invocation);
  diag::Location location;
  if (!where.file.empty()) {
    location = where;
  }
  diag::Diagnostic closed = diag::make_native(codes::kQuitClosedProject, closed_text(where), std::move(location));
  ctx.session().quit_requested = true;  // stops the script and the startup sequence; no exit status
  host_.close_project(closed);
  cmd::CommandResult result = cmd::CommandResult::success();
  result.diagnostics.push_back(std::move(closed));
  return result;
}

}  // namespace hefesto::app
