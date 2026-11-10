// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `expand selection` / `unexpand selection` (cmd-hierarchy "Expand and
// unexpand commands"; m2 design D10 "expand toggle on a selection"): the
// selected instances change, without the box.

#include <catch2/catch_test_macros.hpp>

#include "hier_session.hpp"
#include "hefesto/view/commands.hpp"

namespace hefesto::cell::test {
namespace {

#define EXPAND_REQ "[cap:cmd-hierarchy][req:Expand and unexpand commands]"

TEST_CASE("expand selection acts on the selected instances only", EXPAND_REQ) {
  HierSession s;
  REQUIRE(view::register_view_commands(s.registry));
  db::CellDef* leaf = s.cell("leaf");
  s.paint(*leaf, "metal1", db::Rect{0, 0, 4, 4});
  const db::CellUse* a = s.place(*s.top, *leaf, db::translation(0, 0), "a");
  const db::CellUse* b = s.place(*s.top, *leaf, db::translation(100, 0), "b");
  view::LayoutWindow& w = *s.editor.windows.find(s.window);
  s.editor.box.reset();  // `selection` needs no box
  s.select_use(*b);
  s.ok("expand selection");
  CHECK_FALSE(w.is_expanded(*a));
  CHECK(w.is_expanded(*b));
  s.ok("expand selection toggle");
  CHECK_FALSE(w.is_expanded(*b));
  s.ok("expand selection");
  s.ok("unexpand selection");
  CHECK_FALSE(w.is_expanded(*b));
}

}  // namespace
}  // namespace hefesto::cell::test
