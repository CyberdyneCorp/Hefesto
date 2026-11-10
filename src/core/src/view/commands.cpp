// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Registration of the core view commands (capability ui-windows-and-views).

#include "hefesto/view/commands.hpp"

#include <algorithm>
#include <array>
#include <span>

#include "hefesto/view/color_window.hpp"
#include "view/command_util.hpp"

namespace hefesto::view {

namespace {

/// Editing commands that call WindowManager::area_changed on success.
constexpr auto kReportingEdits = std::to_array<std::string_view>({"paint", "erase", "splitpaint", "label"});

/// Commands that notify windows themselves or draw nothing.
constexpr auto kDisplayCommands = std::to_array<std::string_view>({
    "*coord", "*files", "*watch", "*winddebug", "box", "center", "closewindow", "crosshair", "echo", "expand",
    "feedback", "findbox", "grid", "grow", "help",
    "openwindow", "over", "redraw", "reset", "scroll", "see", "snap", "specialopen", "underneath", "unexpand",
    "updatedisplay", "view", "windowborder", "windowcaption", "windownames", "windowpositions", "windowscrollbars",
    "xview", "zoom"});

bool listed(std::span<const std::string_view> names, std::string_view name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

bool reports_own_redisplay(std::string_view command) {
  return listed(kReportingEdits, command) || listed(kDisplayCommands, command);
}

void RedisplayTracker::on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  const bool edit = listed(kReportingEdits, invocation.name);
  if (!reports_own_redisplay(invocation.name) || (edit && !result.ok())) {
    windows_.redisplay_all();
  }
}

bool register_view_commands(cmd::CommandRegistry& registry) {
  bool ok = detail::register_view_ops(registry);
  ok = detail::register_window_ops(registry) && ok;
  ok = detail::register_display_ops(registry) && ok;
  ok = detail::register_wizard_commands(registry) && ok;
  ok = detail::register_feedback_command(registry) && ok;
  for (cmd::CommandSpec& spec : color_client_commands()) {  // the `color` client's table
    ok = !registry.add(std::move(spec), "color").has_value() && ok;
  }
  return ok;
}

bool register_view_and_zoom(cmd::CommandRegistry& registry) {
  return detail::register_view_zoom(registry);
}

}  // namespace hefesto::view
