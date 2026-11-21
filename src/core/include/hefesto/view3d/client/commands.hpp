// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wind3d` client: opening the window and its command table (view-3d
// "Opening the 3D window", "3D view commands"; openspec/changes/m10-3d-view/
// design.md D9; Q-MISC-71, 73, 76, 78).
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
// `.magic3d <cmd>` and from the command bar when the 3D view has focus.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); WIND3D fills in
// src/core/src/view3d/client/ (design D3, section 8).

#pragma once

#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/view3d/client/session.hpp"

namespace hefesto::cmd {
class CommandRegistry;
}

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

/// Opens the 3D window into `editor.view3d`.
[[nodiscard]] diag::Status open_wind3d(editor::EditorState& editor, const OpenRequest& request);

/// Registers the `wind3d` client's commands; false when a name was already
/// registered.
bool register_wind3d_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::view3d
