// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/all_commands.hpp"

#include <utility>

#include "hefesto/cell/commands.hpp"
#include "hefesto/cif/command/command.hpp"
#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/units.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/drc/commands/command.hpp"
#include "hefesto/gds/command/command.hpp"
#include "hefesto/gui/commands.hpp"
#include "hefesto/pcell/framework/gencell.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/script/binding/commands.hpp"
#include "hefesto/script/pdk/commands.hpp"
#include "hefesto/select/commands.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/tools/commands.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/undo/commands.hpp"
#include "hefesto/view/commands.hpp"
#include "hefesto/wiring/commands.hpp"

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
  // M2 areas (openspec/changes/m2-interactive-editing/design.md D1/D3).
  ok = register_units_commands(registry) && ok;
  ok = undo::register_undo_commands(registry) && ok;
  ok = select::register_select_commands(registry) && ok;
  ok = ui::register_ui_commands(registry) && ok;
  ok = wiring::register_wiring_commands(registry) && ok;
  // M3 areas (openspec/changes/m3-drc/design.md D1/D3).
  ok = drc::register_drc_command(registry) && ok;
  // M4 areas (openspec/changes/m4-scripting/design.md D1/D3).
  ok = script::register_binding_commands(registry) && ok;
  ok = script::register_pdk_commands(registry) && ok;
  ok = pcell::register_pcell_commands(registry) && ok;
  ok = tools::register_tools_commands(registry) && ok;
  ok = gui::register_gui_commands(registry) && ok;
  // M5 (openspec/changes/m5-mask-gds-cif/design.md D3).
  ok = gds::register_gds_command(registry) && ok;
  ok = cif::register_cif_command(registry) && ok;
  return ok;
}

diag::Outcome<editor::EditorState*> require_editor(const CommandContext& ctx) {
  if (ctx.editor() == nullptr) {
    return diag::make(diag::codes::kCmdSession, "this command needs an editor session and none is attached");
  }
  return ctx.editor();
}

}  // namespace hefesto::cmd
