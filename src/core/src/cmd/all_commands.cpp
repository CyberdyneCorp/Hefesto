// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/all_commands.hpp"

#include <utility>

#include "hefesto/cell/commands.hpp"
#include "hefesto/cmd/builtins.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/view/commands.hpp"

namespace hefesto::cmd {

bool register_all_commands(CommandRegistry& registry) {
  return register_all_commands(registry, pdk::CommandHosts{});
}

bool register_all_commands(CommandRegistry& registry, pdk::CommandHosts pdk_hosts) {
  // Every area registers even if an earlier one failed, so one duplicate
  // name does not hide the rest of the command set.
  bool ok = register_builtins(registry);
  ok = tech::register_tech_commands(registry) && ok;
  ok = db::register_db_commands(registry) && ok;
  ok = cell::register_cell_commands(registry) && ok;
  ok = view::register_view_commands(registry) && ok;
  ok = view::register_view_and_zoom(registry) && ok;
  ok = pdk::register_pdk_commands(registry, std::move(pdk_hosts)) && ok;
  return ok;
}

diag::Outcome<editor::EditorState*> require_editor(const CommandContext& ctx) {
  if (ctx.editor() == nullptr) {
    return diag::make(diag::codes::kCmdSession, "this command needs an editor session and none is attached");
  }
  return ctx.editor();
}

}  // namespace hefesto::cmd
