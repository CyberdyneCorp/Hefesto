// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M0 view commands, registered in the core command registry so that the
// command bar reaches them through the core Dispatcher like any other command
// (capability hefesto-architecture, requirement "Single command API"):
//   view                 fit the layout in the canvas
//   zoom <factor>        zoom about the canvas center (factor > 0)
//   theme dark|light     switch the UI theme
//   pan <dx> <dy>        move the view by framebuffer pixels
//
// The canvas key bindings (arrows, + / -, f / Home) issue these commands with
// origin `key` (CanvasController), so they are logged like typed commands.
//
// M0-only placeholders until the view commands of hefesto-app-ux arrive.

#pragma once

#include <functional>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

struct ViewActions {
  std::function<void()> fit;
  std::function<void(double)> zoom;  // factor > 0, centered
  std::function<void(ThemeMode)> theme;
  std::function<void(double, double)> pan;  // framebuffer pixels
};

/// Adds `view`, `zoom`, `theme` and `pan` to `registry`. Returns false if a name was
/// taken. The actions are copied into the handlers and must stay callable for
/// as long as the registry is used.
bool register_view_commands(cmd::CommandRegistry& registry, const ViewActions& actions);

}  // namespace hefesto::shell
