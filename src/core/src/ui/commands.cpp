// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Registration of the dispatch, input and script-helper commands. The
// handlers live in session_commands.cpp (help, send, *bypass, source,
// sleep, history, logcommands, netlist), macro_commands.cpp (macro, imacro,
// autoincr), tool_commands.cpp (tool, cursor, setpoint, pushbutton and the
// tools.tcl button helpers), stack_commands.cpp (box/edit stacks,
// suspendall/resumeall) and measure_commands.cpp (ruler, measure,
// unmeasure, genkey). Owner: DISPATCH.

#include "hefesto/ui/commands.hpp"

#include "ui/command_util.hpp"

namespace hefesto::ui {

bool register_ui_commands(cmd::CommandRegistry& registry) {
  bool ok = detail::register_session_commands(registry);
  ok = detail::register_macro_commands(registry) && ok;
  ok = detail::register_tool_commands(registry) && ok;
  ok = detail::register_stack_commands(registry) && ok;
  ok = detail::register_measure_commands(registry) && ok;
  return ok;
}

}  // namespace hefesto::ui
