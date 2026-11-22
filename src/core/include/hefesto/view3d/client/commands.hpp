// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wind3d` client: opening the window and its command table (view-3d
// "Opening the 3D window", "3D view commands"; openspec/changes/m10-3d-view/
// design.md D9; Q-MISC-71, 73, 76, 78; Q-MISC-80).
//
// open_wind3d follows "Opening the 3D window": the one-window rule ("Only
// one 3D window allowed."), cell resolution and its errors, level 1, and
// the mode rule (mask-layer when a current output style exists, otherwise
// layout-layer; Q-MISC-73 amendment). `specialopen wind3d` in
// view/window_ops.cpp and `render3d` call it.
//
// register_wind3d_commands adds the client's table (`view`, `scroll`,
// `zoom`, `refresh`, `level`, `defaults`, `closewindow`, `help`, `cif`,
// `render`, `see`, `cutbox`) in the `wind3d` client, reachable as
// `.magic3d <cmd>` (the window's toolkit path; the binding sends it to this
// table only, as Magic clears the window's WIND_COMMANDS flag) and from the
// command bar when the 3D view has focus; and `magic::render3d`, the
// wrapper's procedure (tcltk/wrapper.tcl:1589-1614), in the script table.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); WIND3D fills in
// src/core/src/view3d/client/ (design D3, section 8).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/view3d/client/session.hpp"

namespace hefesto::cmd {
class CommandContext;
class CommandRegistry;
}  // namespace hefesto::cmd

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::view3d {

/// The client name of the 3D window's command table.
inline constexpr std::string_view kWind3dClient = "wind3d";

/// What to open: `specialopen wind3d [cell [path]]` or `render3d [cell]`.
struct OpenRequest {
  std::string cell;                                // empty: the cell of the box's window
  std::string toolkit_path = kDefaultToolkitPath;  // ".render.magic" for render3d
};

/// Opens the 3D window into `ctx.editor()->view3d` (W3Dcreate). On success
/// the window is drawn and its toolkit path becomes a window command; the
/// result's value is what Magic's `specialopen` leaves as the Tcl result:
/// with magic-compat ON, `window "<path>" isn't a top-level window` for a
/// path below a top-level window (MakeWindowCommand's `wm protocol` fails
/// on it), else empty. Failures print Magic's message and open nothing.
[[nodiscard]] cmd::CommandResult open_wind3d(cmd::CommandContext& ctx, const OpenRequest& request);

/// Closes the 3D window, if one is open (W3Ddelete, GrTOGLDelete). Its
/// toolkit path is kept in Session::closed_paths and stops being a command.
/// False when no window was open.
bool close_wind3d(editor::EditorState& editor);

/// The current output style's f (cs_scaleFactor after the GCF reduction of
/// the style's scale, m10 tasks.md 8.1 note (d)); 1 without a style.
[[nodiscard]] float current_factor(const editor::EditorState& editor);

/// Brings the window up to date with the current output style (Q-MISC-71,
/// Q-MISC-78): when the style changed since the window last saw it, the
/// mask visibility set is reset (both modes); in mask-layer mode with
/// magic-compat OFF the view is rescaled from the f applied to the new f;
/// and when no output style is left the window returns to layout-layer
/// mode (Magic dereferences the missing style; fixed in both modes).
/// Called before every 3D command and by build_scene.
void sync_output_style(editor::EditorState& editor, bool magic_compat);

/// Whether `path` is the toolkit path of the open 3D window, or of a closed
/// one (Session::closed_paths). The binding sends a window command on the
/// first to the `wind3d` table only; the second is Magic's deleted command.
[[nodiscard]] bool is_open_path(const editor::EditorState& editor, std::string_view path);
[[nodiscard]] bool is_closed_path(const editor::EditorState& editor, std::string_view path);

/// WindSendCommand's answer to a word that is not a command of the 3D
/// window: "Unknown command:" (no newline, standard error), then the words
/// and the window point as windPrintCommand echoes them on standard output
/// (` 'w1' 'w2' at (20, 20)`; the point of a window command with no pointer
/// in the window, _tk_dispatch).
[[nodiscard]] std::vector<diag::Diagnostic> unknown_command_messages(const std::vector<std::string>& words);

/// Registers the `wind3d` client's commands and `magic::render3d`; false
/// when a name was already registered.
bool register_wind3d_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::view3d
