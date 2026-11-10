// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Core view commands (capabilities ui-windows-and-views,
// ui-feedback-and-highlights, ui-wizard-and-debug), registered by
// cmd::register_all_commands():
//   xview, scroll, center, findbox, expand, unexpand, see, grid, redraw,
//   reset, updatedisplay, openwindow, specialopen, closewindow,
//   windowpositions, windownames, windowcaption, windowborder,
//   windowscrollbars, over, underneath, grow, crosshair, feedback, and the
//   wizard commands *watch, *coord, *winddebug, *files;
// and, through register_view_and_zoom(), `view` and `zoom`, which are core
// commands like the others (the desktop shell has no `view`/`zoom` of its
// own; its camera gestures stay direct and unlogged). The `color` window
// client's commands (view::color_client_commands(), view/color_window.hpp)
// go to that client's table.
// Handlers mutate view::WindowManager and the VIEW members of
// editor::EditorState and notify observers; the desktop shell redraws.
//
// Owner: VIEW (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <string_view>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::view {

/// Adds the view commands to `registry`. Returns false if a name was taken.
bool register_view_commands(cmd::CommandRegistry& registry);

/// Adds the core `view` and `zoom` commands.
bool register_view_and_zoom(cmd::CommandRegistry& registry);

/// "Change-driven redisplay": true for the commands that report every
/// display change they make themselves, through WindowManager::area_changed
/// (paint, erase, splitpaint, label) or a window notification (the view and
/// window commands), or that change nothing drawn (box, snap). Any other
/// command may change what a window shows in ways not reported yet.
[[nodiscard]] bool reports_own_redisplay(std::string_view command);

/// Observes the dispatcher for a front end that redraws incrementally: after
/// a command that does not report its own redisplay, or an editing command
/// that failed part-way, every window is marked for a whole redisplay. The
/// dispatcher's post-command update (editor::post_command) then performs the
/// pending redisplay (WindowManager::update_display).
class RedisplayTracker final : public cmd::CommandObserver {
 public:
  explicit RedisplayTracker(WindowManager& windows) : windows_(windows) {}
  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) override;

 private:
  WindowManager& windows_;
};

}  // namespace hefesto::view
