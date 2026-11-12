// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/gui_script.hpp"

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::shell {

cmd::CommandResult run_gui_script(editor::EditorState& editor, cmd::Dispatcher& dispatcher, std::string_view script,
                                  cmd::Origin origin) {
  if (editor.script.interp != nullptr) {
    return editor.script.interp->evaluate(script, origin);
  }
  const cmd::ParsedScript parsed = cmd::parse_script(script, "gui");
  if (parsed.error) return cmd::CommandResult::failure(*parsed.error);
  cmd::CommandResult result = cmd::CommandResult::success();
  for (const cmd::ParsedCommand& command : parsed.commands) {
    cmd::Invocation invocation{command.words.front(),
                               {command.words.begin() + 1, command.words.end()},
                               origin,
                               std::nullopt};
    result = dispatcher.execute(invocation);
    if (!result.ok()) break;
  }
  return result;
}

std::string gui_result_text(const cmd::CommandResult& result) {
  if (!result.ok() && !result.diagnostics.empty()) return result.diagnostics.front().text;
  return result.value.to_string();
}

}  // namespace hefesto::shell
