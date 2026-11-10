// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// editor::coordinate_units, the one builder of the session's coordinate
// units. Regression: the ui and wiring areas each kept a copy that ignored
// `units lambda` under `snap internal` (ui) or `units user` (wiring).

#include <catch2/catch_test_macros.hpp>

#include "hefesto/editor/coordinate_units.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace cmd = hefesto::cmd;
namespace editor = hefesto::editor;

TEST_CASE("the default unit is the units setting, else the snap grid",
          "[cap:cmd-coordinates-and-units][req:Default unit of un-suffixed values]") {
  editor::EditorState state;
  CHECK(editor::coordinate_units(state).default_unit == cmd::DefaultUnit::lambda);  // startup
  state.snap = editor::SnapMode::internal;
  CHECK(editor::coordinate_units(state).default_unit == cmd::DefaultUnit::internal);
  state.snap = editor::SnapMode::user;
  CHECK(editor::coordinate_units(state).default_unit == cmd::DefaultUnit::user);

  state.snap = editor::SnapMode::internal;
  state.units.unit = cmd::InputUnit::lambda;  // units wins over snap
  CHECK(editor::coordinate_units(state).default_unit == cmd::DefaultUnit::lambda);
  state.units.unit = cmd::InputUnit::user;
  CHECK(editor::coordinate_units(state).default_unit == cmd::DefaultUnit::user);
  state.units.unit = cmd::InputUnit::microns;
  CHECK(editor::coordinate_units(state).default_unit == cmd::DefaultUnit::microns);
  state.units.unit = cmd::InputUnit::internal;
  CHECK(editor::coordinate_units(state).default_unit == cmd::DefaultUnit::internal);

  CHECK(editor::coordinate_units(state).magic_compat);
  CHECK_FALSE(editor::coordinate_units(state, false).magic_compat);
  CHECK_FALSE(editor::coordinate_units(state).grid.has_value());  // no window
}
