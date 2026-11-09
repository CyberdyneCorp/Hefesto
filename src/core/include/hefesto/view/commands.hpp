// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Core view commands (capability ui-windows-and-views), registered by
// cmd::register_all_commands(). Handlers mutate view::WindowManager in
// editor::EditorState and notify observers; the desktop shell redraws.
//   xview, scroll, center, findbox, expand, unexpand, see, grid, redraw,
//   reset, updatedisplay, openwindow, closewindow, windowpositions,
//   windownames, windowcaption, windowborder, windowscrollbars, over,
//   underneath, crosshair
// and, through register_view_and_zoom(), `view` and `zoom`.
// Coordination (design.md D5): the desktop shell's M0 `view`/`zoom`
// placeholders are gone; register_all_commands() registers both through
// register_view_and_zoom() (kept separate so the hand-over stayed one line).
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::view {

/// Adds the view commands to `registry`. Returns false if a name was taken.
bool register_view_commands(cmd::CommandRegistry& registry);

/// Adds the core `view` and `zoom` commands (implemented; see D5 above).
bool register_view_and_zoom(cmd::CommandRegistry& registry);

}  // namespace hefesto::view
