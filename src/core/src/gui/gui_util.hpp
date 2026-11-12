// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the gui area: nested engine commands (ui/command_util),
// notes, and files named relative to the session's working directory.
// Owner: GUIMGR (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::gui::detail {

/// `[cmd ...]`: runs `words` nested (diagnostics forwarded) and returns the
/// value's string form.
std::string run(cmd::CommandContext& ctx, const std::vector<std::string>& words);

/// The value of a nested command (diagnostics forwarded).
cmd::Value run_value(cmd::CommandContext& ctx, const std::vector<std::string>& words);

/// Emits `text` with `code`.
void note(cmd::CommandContext& ctx, std::string_view code, std::string text);

/// `name` resolved against the session's working directory when relative.
[[nodiscard]] platform::Path session_path(cmd::CommandContext& ctx, std::string_view name);

/// The words of a value (a list's elements, or the words of a string).
[[nodiscard]] std::vector<std::string> words_of(const cmd::Value& value);

/// The elements of a list value (a string value: its words).
[[nodiscard]] std::vector<cmd::Value> elements_of(const cmd::Value& value);

/// A number of a value (int, double or a numeric string).
[[nodiscard]] std::optional<double> number_of(const cmd::Value& value);

/// The failure of a command run without an editor session (CMD-USAGE-001).
[[nodiscard]] cmd::CommandResult no_session(const cmd::CommandContext& ctx);

/// Tcl's error for a procedure called with the wrong number of arguments.
[[nodiscard]] cmd::CommandResult wrong_args(std::string_view usage);

}  // namespace hefesto::gui::detail
