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
#include "hefesto/antenna/antenna.hpp"
#include "hefesto/drc/commands/command.hpp"
#include "hefesto/extract/command/commands.hpp"
#include "hefesto/extresist/command/command.hpp"
#include "hefesto/gds/command/command.hpp"
#include "hefesto/gui/commands.hpp"
#include "hefesto/lef/command/command.hpp"
#include "hefesto/lvs/cmd/commands.hpp"
#include "hefesto/netlist/sim/sim.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "hefesto/oasis/command/command.hpp"
#include "hefesto/pcell/framework/gencell.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/schem/cmd/commands.hpp"
#include "hefesto/script/binding/commands.hpp"
#include "hefesto/script/pdk/commands.hpp"
#include "hefesto/select/commands.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/tools/commands.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/undo/commands.hpp"
#include "hefesto/view/commands.hpp"
#include "hefesto/view3d/client/commands.hpp"
#include "hefesto/view3d/export/command.hpp"
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
  // `ext2spice` (with M4's `scale` option) is registered by the M7 netlist
  // area below (netlist/spice/spice.hpp).
  ok = pcell::register_pcell_commands(registry) && ok;
  ok = tools::register_tools_commands(registry) && ok;
  ok = gui::register_gui_commands(registry) && ok;
  // M5 (openspec/changes/archive/2026-11-24-m5-mask-gds-cif/design.md D3).
  ok = gds::register_gds_command(registry) && ok;
  ok = cif::register_cif_command(registry) && ok;
  // M6 (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D3, D14, D15).
  ok = lef::register_lef_def_commands(registry) && ok;
  ok = oasis::register_oasis_command(registry) && ok;
  // M8 (openspec/changes/m8-schematic/design.md D12): `schematic`.
  ok = !schem::register_schematic_command(registry).has_value() && ok;
  // M7 (openspec/changes/m7-extraction/design.md D4).
  ok = extract::register_extract_commands(registry) && ok;
  ok = netlist::spice::register_spice_commands(registry) && ok;
  ok = netlist::sim::register_sim_commands(registry) && ok;
  ok = extresist::register_extresist_command(registry) && ok;
  ok = antenna::register_antenna_command(registry) && ok;
  // M10 (openspec/changes/m10-3d-view/design.md D9): the wind3d client.
  ok = view3d::register_wind3d_commands(registry) && ok;
  ok = view3d::register_export3d_command(registry) && ok;
  // M9 (openspec/changes/m9-lvs-simulation design.md "Area contracts" LVSCMD).
  ok = lvs::register_lvs_commands(registry) && ok;
  return ok;
}

diag::Outcome<editor::EditorState*> require_editor(const CommandContext& ctx) {
  if (ctx.editor() == nullptr) {
    return diag::make(diag::codes::kCmdSession, "this command needs an editor session and none is attached");
  }
  return ctx.editor();
}

}  // namespace hefesto::cmd
