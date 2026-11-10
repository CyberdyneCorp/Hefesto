// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `logcommands` log lines (ui-command-dispatch "Command logging").
// Owner: DISPATCH.
// (Magic ref: textio/txCommands.c:866-992)

#include "hefesto/ui/command_log.hpp"

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/state.hpp"

namespace hefesto::ui {

namespace {

// The command name after any namespace qualifier (`magic::logc` -> `logc`).
std::string_view unqualified(std::string_view name) {
  const std::size_t at = name.find("::");
  return at == std::string_view::npos ? name : name.substr(at + 2);
}

bool never_logged(const cmd::Invocation& invocation) {
  const std::string_view name = unqualified(invocation.name);
  return name.starts_with("logc") || name == "*bypass" || name == "setpoint" ||
         (name == "wire" && !invocation.args.empty() && invocation.args.front() == "show");
}

void append(editor::EditorState& editor, platform::FileSystem& files, std::string line) {
  CommandLogState& log = state(editor).log;
  if (!log.path || log.suspended) {
    return;
  }
  line += '\n';
  if (log.update) {
    line += "updatedisplay\n";
  }
  (void)files.append(*log.path, line);
}

}  // namespace

std::string log_line(const cmd::Invocation& invocation) {
  if (invocation.name.empty() || never_logged(invocation)) {
    return {};
  }
  std::string line = invocation.name;
  for (const std::string& arg : invocation.args) {
    line += ' ';
    const bool quote = arg.find(' ') != std::string::npos;
    if (quote) {
      line += '"';
    }
    line += arg;
    if (quote) {
      line += '"';
    }
  }
  return line;
}

void log_command(editor::EditorState& editor, platform::FileSystem& files, const cmd::Invocation& invocation) {
  std::string line = log_line(invocation);
  if (!line.empty()) {
    append(editor, files, std::move(line));
  }
}

void log_button(editor::EditorState& editor, platform::FileSystem& files, std::string_view button,
                std::string_view action) {
  append(editor, files, "pushbutton " + std::string{button} + " " + std::string{action});
}

}  // namespace hefesto::ui
