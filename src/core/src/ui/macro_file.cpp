// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The key-binding file reader (design.md D7). Owner: DISPATCH.

#include "hefesto/ui/macro_file.hpp"

#include "hefesto/cmd/script.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

MacroFileResult read_macro_file(editor::EditorState& editor, std::string_view text, std::string_view source_name) {
  MacroFileResult result;
  const cmd::ParsedScript parsed = cmd::parse_script(text, source_name);
  for (const cmd::ParsedCommand& command : parsed.commands) {
    const std::string& name = command.words.front();
    const diag::SourceLocation where{std::string{source_name}, command.line};
    if (name != "macro" && name != "imacro") {
      result.diagnostics.push_back(diag::make(
          codes::kUsage, "a key-binding file holds only macro and imacro lines, not \"" + name + "\"", where));
      continue;
    }
    const std::span<const std::string> args{command.words.data() + 1, command.words.size() - 1};
    cmd::CommandResult r = macro_command(editor, args, name == "imacro", "layout");
    for (diag::Diagnostic& d : r.diagnostics) {
      if (std::holds_alternative<std::monostate>(d.location)) {
        d.location = where;
      }
      result.diagnostics.push_back(std::move(d));
    }
    ++result.applied;
  }
  if (parsed.error) {
    result.diagnostics.push_back(*parsed.error);
  }
  return result;
}

}  // namespace hefesto::ui
