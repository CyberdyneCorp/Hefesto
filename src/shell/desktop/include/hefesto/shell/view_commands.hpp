// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shell's own view commands, registered in the core command registry so
// that the command bar reaches them through the core Dispatcher like any
// other command (capability hefesto-architecture, requirement "Single
// command API"):
//   theme dark|light     switch the UI theme
//   pan <dx> <dy>        move the view by framebuffer pixels
//   panel layers|inspector|console|keys|drc ?show|hide|toggle?
//                        show, hide (collapse) or toggle a panel (keys: the
//                        key-binding sheet; drc: the DRC results browser);
//                        with no action it reports "shown" or "hidden"
//   panel layers|inspector dock left|right
//                        dock a side panel on the left or right of the canvas
//   panel reset          the default (M1) arrangement, every side panel shown
//   palette paint ?LAYER|none?
//                        set (or report) the layer palette's paint layer,
//                        which the Paint tool and "Paint" / "Erase" use
//   hefesto::notifications ?on|off?
//                        the job-completion notifications (hefesto-app-ux
//                        "Desktop platform integration through Raven"): the
//                        session layer of the `notifications` setting
//
// `view` and `zoom` are the core's (ui-windows-and-views, registered by
// cmd::register_all_commands; design.md D5): they move the window's
// ViewMapping and the canvas follows it. Keys reach them through the
// macro tables (InputRouter, origin `key`).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

/// The collapsible panels of the main window.
enum class PanelId : std::uint8_t { layers, inspector, console, keys, drc };
enum class DockSide : std::uint8_t { left, right };

struct ViewActions {
  std::function<void(ThemeMode)> theme;
  std::function<void(double, double)> pan;  // framebuffer pixels
  /// Shows (true) or hides a panel; returns whether it is shown afterwards.
  /// With std::nullopt it toggles. Unset: `panel` is not registered.
  std::function<bool(PanelId, std::optional<bool>)> panel;
  /// Whether a panel is shown.
  std::function<bool(PanelId)> panel_shown;
  /// Docks the layers or inspector panel on a side of the canvas; false for
  /// a panel that cannot dock (console, keys). Unset: `dock` is refused.
  std::function<bool(PanelId, DockSide)> dock;
  /// `panel reset`: the default arrangement. Unset: refused.
  std::function<void()> reset;
  /// Sets the paint layer (an empty name clears it) and returns it; with
  /// std::nullopt it only returns it. A layer the technology does not know
  /// is refused with an empty optional. Unset: `palette` is not registered.
  std::function<std::optional<std::string>(std::optional<std::string>)> paint_layer;
};

/// Adds `theme`, `pan`, `hefesto::notifications` and (with their actions) `panel` and `palette` to `registry`.
/// Returns false if a name was taken.
/// The actions are copied into the handlers and must stay callable for as
/// long as the registry is used.
bool register_view_commands(cmd::CommandRegistry& registry, const ViewActions& actions);

}  // namespace hefesto::shell
