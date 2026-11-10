// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MODEL commands, part 1: snap and the registration of every db command.
// `box` is in commands_box.cpp, the paint commands in commands_paint.cpp,
// the misc editing commands in commands_misc.cpp; `getcell` and `flatten`
// are the CELL area's (cell/place_commands.cpp, cell/flatten_command.cpp).
// (Magic ref: commands/CmdRS.c CmdSnap)

#include "hefesto/db/commands.hpp"

#include <string>

#include "db/command_util.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::db {

namespace cu = cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;

namespace {

// --- snap --------------------------------------------------------------------

// CmdSnap (Magic ref: commands/CmdRS.c:2944-3021): setting returns nothing;
// `snap` / `snap list` return the grid name.
CommandResult cmd_snap(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  enum Choice { off, internal, lambda, grid, user, on, list };
  int n = list;
  if (!inv.args.empty()) {
    n = cu::lookup(inv.args[0], {"off", "internal", "lambda", "grid", "user", "on", "list"});
    if (n < 0) {
      cu::print(s.value(), "Usage: snap [internal | lambda | user]");
      return CommandResult::success();
    }
  }
  if (n == internal && s.value().editor->units.unit == cmd::InputUnit::default_units) {
    cu::set_units_internal(s.value());
    cu::warn(s.value(),
             "Warning:  snap setting is also changing units.  This usage is deprecated\n"
             "and may be removed in the future.  Use \"units\" to change units, and\n"
             "change units before setting snap to keep this message from appearing.");
  }
  switch (n) {
    case off:
    case internal:
      cu::set_snap_mode(s.value(), cu::Snap::internal);
      return CommandResult::success();
    case lambda:
      cu::set_snap_mode(s.value(), cu::Snap::lambda);
      return CommandResult::success();
    case list:
      return CommandResult::success(Value{std::string{cu::snap_name(cu::snap_mode(s.value()))}});
    default:
      cu::set_snap_mode(s.value(), cu::Snap::user);
      return CommandResult::success();
  }
}

}  // namespace

namespace cmdutil {

bool register_snap_command(cmd::CommandRegistry& registry) {
  return add(registry, "snap", "snap ?internal|lambda|user?", "Shows or sets the snap grid.", cmd_snap);
}

}  // namespace cmdutil

bool register_db_commands(cmd::CommandRegistry& registry) {
  bool ok = cmdutil::register_paint_commands(registry);
  ok = cmdutil::register_snap_command(registry) && ok;
  ok = cmdutil::register_misc_commands(registry) && ok;
  ok = cmdutil::register_label_commands(registry) && ok;
  ok = cmdutil::register_cell_commands(registry) && ok;
  return ok;
}

}  // namespace hefesto::db
