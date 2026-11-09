// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shell's own view commands, registered in the core command registry so
// that the command bar reaches them through the core Dispatcher like any
// other command (capability hefesto-architecture, requirement "Single
// command API"):
//   theme dark|light     switch the UI theme
//   pan <dx> <dy>        move the view by framebuffer pixels
//
// `view` and `zoom` are the core's (ui-windows-and-views, registered by
// cmd::register_all_commands; design.md D5): they move the window's
// ViewMapping and the canvas follows it. The canvas keys issue them with
// origin `key` (CanvasController).

#pragma once

#include <functional>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

struct ViewActions {
  std::function<void(ThemeMode)> theme;
  std::function<void(double, double)> pan;  // framebuffer pixels
};

/// Adds `theme` and `pan` to `registry`. Returns false if a name was taken.
/// The actions are copied into the handlers and must stay callable for as
/// long as the registry is used.
bool register_view_commands(cmd::CommandRegistry& registry, const ViewActions& actions);

}  // namespace hefesto::shell
