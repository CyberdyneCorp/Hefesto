// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// editor::coordinate_units. Shared file owned by INTEGRATION.

#include "hefesto/editor/coordinate_units.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::editor {

namespace {

// The unit of un-suffixed values: `units` when set, else the `snap` grid.
cmd::DefaultUnit default_unit(const EditorState& editor) {
  switch (editor.units.unit) {
    case cmd::InputUnit::internal:
      return cmd::DefaultUnit::internal;
    case cmd::InputUnit::lambda:
      return cmd::DefaultUnit::lambda;
    case cmd::InputUnit::user:
      return cmd::DefaultUnit::user;
    case cmd::InputUnit::microns:
      return cmd::DefaultUnit::microns;
    case cmd::InputUnit::default_units:
      break;
  }
  switch (editor.snap) {
    case SnapMode::internal:
      return cmd::DefaultUnit::internal;
    case SnapMode::user:
      return cmd::DefaultUnit::user;
    case SnapMode::lambda:
      break;
  }
  return cmd::DefaultUnit::lambda;
}

}  // namespace

cmd::CoordinateUnits coordinate_units(const EditorState& editor, bool magic_compat) {
  cmd::CoordinateUnits u;
  u.internal_per = editor.grid.internal;
  u.lambda_per = editor.grid.lambda;
  u.default_unit = default_unit(editor);
  u.magic_compat = magic_compat;
  if (const auto& cif = editor.technology->cif_output_grid()) {
    // The style is kept as read (per lambda); Magic rescales it with every
    // grid change (CIFTechOutputScale), i.e. by lambda/internal of the
    // session grid, reduced.
    const std::int64_t scale = cif->scale_factor * editor.grid.lambda;
    const std::int64_t expander = cif->expander * editor.grid.internal;
    const std::int64_t common = std::max<std::int64_t>(std::gcd(scale, expander), 1);
    u.cif_scale = std::array<std::int64_t, 2>{scale / common, expander / common};
  }
  if (const view::LayoutWindow* w = editor.windows.current()) {
    const db::Rect g = w->grid().spacing;
    u.grid = std::array<std::int64_t, 4>{g.x0, g.y0, g.x1, g.y1};
  }
  return u;
}

}  // namespace hefesto::editor
