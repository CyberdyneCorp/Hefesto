// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PointerState: where the pointer is when a button or key macro runs
// (openspec/changes/m2-interactive-editing/design.md D4; Magic's
// WindCurrentWindow/ToolGetPoint state). The shell sets it in
// editor::EditorState::command_point before dispatching a button or key
// macro, and `setpoint` sets it from a script. Commands that work "at the
// cursor" read it: `box ... cursor`, point selection, `paint`/`erase` by
// example, `wire`, `cursor`, `select ... at`.
//
// Owner: INTEGRATION (shared interface); DISPATCH maintains the member.

#pragma once

#include <cstdint>

#include "hefesto/db/geometry.hpp"

namespace hefesto::cmd {

struct PointerState {
  /// The window under the pointer (a view::WindowId).
  std::int32_t window = 0;
  /// The pointer in the window's root coordinates, not yet snapped
  /// (cmd::snap_point applies "Pointer snapping").
  db::Point point;
  /// The root-coordinate area covered by the screen pixel under the
  /// pointer (point selection tolerance; a zero-size rectangle at `point`
  /// when the scale is one pixel per unit or more).
  db::Rect pixel_area;

  friend bool operator==(const PointerState&, const PointerState&) = default;
};

}  // namespace hefesto::cmd
