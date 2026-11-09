// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What every part of the Desktop-Layout screen shares: the Raven runtime
// (fonts, icons), the current forge theme, the core session, and the way a
// UI action runs a command (through the core Dispatcher, origin `gui`, echoed
// in the Console like a typed command; hefesto-architecture "Single command
// API"). Raven-free.

#pragma once

#include <functional>
#include <string>

#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/theme_tokens.hpp"
#include "raven_ui.hpp"

namespace hefesto::shell {

struct ScreenContext {
  ui::Runtime& runtime;
  CoreSession& core;
  ThemeMode mode = ThemeMode::dark;
  /// Runs one command line as if typed in the command bar (Console echo,
  /// history, status bar). Set by the main window.
  std::function<void(const std::string&)> run;
  /// Appends a command and its result to the Console without running it
  /// (results of background jobs). Set by the main window.
  std::function<void(const std::string&, const std::string&)> echo;

  [[nodiscard]] const ThemeTokens& tokens() const noexcept { return theme_tokens(mode); }
  [[nodiscard]] editor::EditorState& editor() noexcept { return core.editor(); }
};

}  // namespace hefesto::shell
