// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic canvas's system clipboard link (schematic_clipboard.hpp).
// Owner: SHELL.

#include "hefesto/shell/schematic_clipboard.hpp"

#include <algorithm>
#include <cmath>

#include "hefesto/shell/manager_views.hpp"
#include "hefesto/shell/schematic_library.hpp"

namespace hefesto::shell {

namespace {

constexpr double kPasteSnap = 10.0;  // xschem's default snap, as Place

bool is_copy(const cmd::Invocation& invocation) {
  return invocation.name == "schematic" && invocation.args.size() == 2 && invocation.args[0] == "clipboard" &&
         invocation.args[1] == "copy";
}

}  // namespace

std::string clipboard_echo(const std::string& command, const std::string& text) {
  if (!command.starts_with("schematic clipboard")) return text;
  const std::size_t end = std::min(text.find('\n'), kClipboardEchoBytes);
  if (end >= text.size()) return text;
  return text.substr(0, end) + " … (" + std::to_string(text.size()) + " bytes)";
}

std::string SchematicClipboardLink::copy_command() {
  copy_pending_ = true;
  return command_line({"schematic", "clipboard", "copy"});
}

bool SchematicClipboardLink::copied(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  if (!copy_pending_ || !is_copy(invocation)) return false;
  copy_pending_ = false;
  if (!result.ok()) return false;
  const std::string text = result.value.to_string();
  if (text.empty()) return false;  // nothing selected: the clipboard is unchanged
  host_.set_text(text);
  return true;
}

ClipboardPaste SchematicClipboardLink::paste_commands(schem::Point at) const {
  ClipboardPaste paste;
  const std::string text = host_.text();
  if (text.empty()) {
    paste.message = "Paste: the clipboard holds no text";
    return paste;
  }
  if (text.size() > kMaxClipboardBytes) {
    paste.message = "Paste: the clipboard text is too large (" + std::to_string(text.size()) + " bytes, limit " +
                    std::to_string(kMaxClipboardBytes) + ")";
    return paste;
  }
  const double x = std::round(at.x / kPasteSnap) * kPasteSnap + 0.0;  // + 0.0: never "-0"
  const double y = std::round(at.y / kPasteSnap) * kPasteSnap + 0.0;
  paste.lines.push_back(command_line({"schematic", "clipboard", "set", text}));
  paste.lines.push_back(command_line({"schematic", "paste", coordinate_text(x), coordinate_text(y)}));
  return paste;
}

}  // namespace hefesto::shell
