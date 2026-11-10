// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session's coordinate units for cmd::parse_distance / parse_coordinate
// (capability cmd-coordinates-and-units "Default unit of un-suffixed
// values", "Unit suffixes"): the grid scale, the default unit (`units`,
// else `snap`), the CIF output scale and the current window's grid. One
// builder for every area that reads coordinates (db, ui, view, wiring).
// (Magic ref: commands/CmdSubrs.c cmdScaleCoord, dbwind/DBWprocs.c)
//
// Owner: INTEGRATION (shared; openspec/changes/m2-interactive-editing
// design.md D2).

#pragma once

#include "hefesto/cmd/coordinates.hpp"

namespace hefesto::editor {

struct EditorState;

/// The coordinate units of `editor` with the given `magic-compat` setting.
[[nodiscard]] cmd::CoordinateUnits coordinate_units(const EditorState& editor, bool magic_compat = true);

}  // namespace hefesto::editor
