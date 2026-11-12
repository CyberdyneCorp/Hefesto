// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What every part of the Desktop-Layout screen shares: the Raven runtime
// (fonts, icons), the current forge theme, the core session, and the way a
// UI action runs a command (Tcl in the project's interpreter, origin `gui`,
// so its engine commands go through the core Dispatcher and their tags fire;
// echoed in the Console like a typed command; hefesto-architecture "Single
// command API", m4-scripting tasks.md 13.2). Raven-free.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/screen_models.hpp"
#include "hefesto/shell/theme_tokens.hpp"
#include "raven_ui.hpp"

namespace hefesto::shell {

struct ScreenContext {
  ui::Runtime& runtime;
  CoreSession& core;
  ThemeMode mode = ThemeMode::dark;
  /// Runs one Tcl command line as if typed in the command bar (Console
  /// echo, history, status bar), through CoreCommandSink. Set by the main
  /// window.
  std::function<void(const std::string&)> run;
  /// Appends a command and its result to the Console without running it
  /// (results of background jobs). Set by the main window.
  std::function<void(const std::string&, const std::string&)> echo;
  /// Puts a command in the command bar without running it (an action that
  /// needs an argument, e.g. "label "), and focuses it. Set by the main window.
  std::function<void(const std::string&)> prompt;
  /// Opens a popover menu at window point (x, y): choosing an entry runs
  /// its command (or puts it in the command bar). Set by the main window.
  std::function<void(const std::string& title, const std::vector<MenuEntry>& entries, float x, float y)> menu;
  /// The layer palette's active paint layer (empty: none).
  std::string paint_layer;

  [[nodiscard]] const ThemeTokens& tokens() const noexcept { return theme_tokens(mode); }
  [[nodiscard]] editor::EditorState& editor() noexcept { return core.editor(); }
};

}  // namespace hefesto::shell
