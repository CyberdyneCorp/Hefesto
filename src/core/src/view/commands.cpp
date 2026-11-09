// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Registration of the core view commands (capability ui-windows-and-views).

#include "hefesto/view/commands.hpp"

#include "view/command_util.hpp"

namespace hefesto::view {

bool register_view_commands(cmd::CommandRegistry& registry) {
  bool ok = detail::register_view_ops(registry);
  ok = detail::register_window_ops(registry) && ok;
  ok = detail::register_display_ops(registry) && ok;
  return ok;
}

bool register_view_and_zoom(cmd::CommandRegistry& registry) {
  return detail::register_view_zoom(registry);
}

}  // namespace hefesto::view
