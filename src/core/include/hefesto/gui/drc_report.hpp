// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DRC manager and its report file (openspec/changes/m4-scripting/
// design.md D12; capability ui-gui-managers "DRC manager and DRC report
// file"; Q-SCRIPT-8, Q-SCRIPT-26): `magic::drc_save_report ?cell? ?file?`,
// `magic::drc_load_report ?file?` and the DRC manager's model.
//
// Native ports of drcmgr.tcl that run the same engine commands nested in
// the calling command (`select top cell`, `cellname list self`, `load`,
// `drc check`, `drc list count`, `drc listall why`, `cif scale out`,
// `feedback add`, `suspendall`/`resumeall`). Q-SCRIPT-26 (fix, both
// magic-compat settings): the error boxes are always collected in internal
// units and the user's units are restored afterwards, so the file does not
// depend on `units`.
//
// (Magic ref: tcltk/drcmgr.tcl:1-300)
//
// Owner: GUIMGR (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/gui/managers.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::gui {

/// The default report file of both procedures.
inline constexpr std::string_view kDefaultDrcReport = "drc.out";

/// The report text for `cell` (line 1 `<cell> <drc list count>`), the
/// separator of 40 `-`, then per rule: the rule text, the separator, one
/// ` %.3f %.3f %.3f %.3f` line per box (internal units × `scale`), the
/// separator; then a final empty line. `errors[i].boxes` hold
/// "llx lly urx ury" in internal units.
[[nodiscard]] std::string format_drc_report(std::string_view cell, std::string_view count,
                                            const std::vector<DrcManagerError>& errors, double scale);

/// `magic::drc_save_report ?cell? ?file?`: checks `cell` (or the top cell of
/// the window), writes the report to `file` (default drc.out, relative to
/// the session's working directory) and reloads the original cell.
[[nodiscard]] cmd::CommandResult drc_save_report(cmd::CommandContext& ctx, std::string_view cell,
                                                 std::string_view file);

/// One box line of a report file: the four words of the first match of
/// Tcl's `regexp { ([^ ]+) ([^ ]+) ([^ ]+) ([^ ]+)}`, or nullopt.
struct ReportBox {
  std::string llx, lly, urx, ury;
};
[[nodiscard]] std::optional<ReportBox> match_report_box(std::string_view line);

/// The rules and boxes of a report file as drc_load_report reads it: a line
/// beginning with a space adds a box (when it matches) to the last rule
/// text; a line starting with neither a space nor `-` sets the rule text.
/// Boxes are kept as "llx lly urx ury" in µm.
[[nodiscard]] std::vector<DrcManagerError> parse_drc_report(std::string_view text);

/// `magic::drc_load_report ?file?`: adds feedback `<rule>` with style
/// `vert_highlights` for every box of the file, as a polygon in µm.
[[nodiscard]] cmd::CommandResult drc_load_report(cmd::CommandContext& ctx, std::string_view file);

/// The DRC manager's tree (drcmanager update): `drc listall why` of the top
/// cell of the window, in its order (Q-SCRIPT-8), boxes in internal units.
[[nodiscard]] DrcManagerModel drc_manager_model(cmd::CommandContext& ctx);

/// The DRC manager's Zoom on `box` ("llx lly urx ury", internal units):
/// `box values` in internal units, `findbox zoom`, `zoom 2`, with the
/// user's units restored (drcmgr.tcl drccallback zoom).
[[nodiscard]] cmd::CommandResult drc_manager_zoom(cmd::CommandContext& ctx, std::string_view box);

}  // namespace hefesto::gui
