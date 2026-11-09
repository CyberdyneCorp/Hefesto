// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Core view commands (capability ui-windows-and-views), registered by
// cmd::register_all_commands(). Handlers mutate view::WindowManager in
// editor::EditorState and notify observers; the desktop shell redraws.
//   xview, scroll, center, findbox, expand, unexpand, see, grid, redraw,
//   reset, updatedisplay, openwindow, closewindow, windowpositions,
//   windownames, windowcaption, windowborder, windowscrollbars, over,
//   underneath, crosshair
// and, through register_view_and_zoom(), `view` and `zoom`.
// Coordination (design.md D5): the desktop shell's M0 `view`/`zoom`
// placeholders are gone; register_all_commands() registers both through
// register_view_and_zoom() (kept separate so the hand-over stayed one line).
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <string_view>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::view {

/// Adds the view commands to `registry`. Returns false if a name was taken.
bool register_view_commands(cmd::CommandRegistry& registry);

/// Adds the core `view` and `zoom` commands (implemented; see D5 above).
bool register_view_and_zoom(cmd::CommandRegistry& registry);

/// "Change-driven redisplay": true for the commands that report every
/// display change they make themselves, through WindowManager::area_changed
/// (paint, erase, splitpaint, label) or a window notification (the view and
/// window commands), or that change nothing drawn (box, snap). Any other
/// command may change what a window shows in ways not reported yet.
[[nodiscard]] bool reports_own_redisplay(std::string_view command);

/// Observes the dispatcher for a front end that redraws incrementally: after
/// a command that does not report its own redisplay, or an editing command
/// that failed part-way, every window is marked for a whole redisplay; then
/// the pending redisplay is performed (WindowManager::update_display).
class RedisplayTracker final : public cmd::CommandObserver {
 public:
  explicit RedisplayTracker(WindowManager& windows) : windows_(windows) {}
  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) override;

 private:
  WindowManager& windows_;
};

}  // namespace hefesto::view
