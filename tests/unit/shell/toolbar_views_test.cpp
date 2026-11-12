// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer toolbar and reorder dialog models (docs/design/mockups/M4-Managers.dc.html
// tiles 5 and 6; openspec/changes/m4-scripting task 10.4).

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "hefesto/shell/toolbar_views.hpp"

using namespace hefesto;
using namespace hefesto::shell;

TEST_CASE("the layer palette's gestures issue the toolbar row commands",
          "[cap:ui-gui-managers][req:Layer toolbar]") {
  // Scenario "Paint from toolbar": middle click on poly in .layout1.
  CHECK(palette_command(".layout1", "poly", false, PaletteGesture::middle) ==
        std::optional<std::string>{".layout1 paint poly"});
  CHECK(palette_command("", "poly", false, PaletteGesture::click) == std::optional<std::string>{"see poly"});
  CHECK(palette_command("", "poly", false, PaletteGesture::right) == std::optional<std::string>{"see no poly"});
  CHECK(palette_command("", "poly", false, PaletteGesture::shift_middle) == std::optional<std::string>{"erase poly"});
  CHECK(palette_command("", "poly", false, PaletteGesture::key_s) ==
        std::optional<std::string>{"select more area poly"});
  CHECK(palette_command("", "poly", false, PaletteGesture::key_shift_s) ==
        std::optional<std::string>{"select less area poly"});
  CHECK(palette_command("", "poly", false, PaletteGesture::key_l) == std::optional<std::string>{"tech lock poly"});
  CHECK_FALSE(palette_command("", "poly", false, PaletteGesture::key_u));
  // A locked row binds only see and u.
  CHECK_FALSE(palette_command("", "fence", true, PaletteGesture::middle));
  CHECK(palette_command("", "fence", true, PaletteGesture::key_u) == std::optional<std::string>{"tech unlock fence"});
  CHECK(palette_command("", "fence", true, PaletteGesture::click) == std::optional<std::string>{"see fence"});
}

TEST_CASE("the palette follows the toolbar order", "[cap:ui-gui-managers][req:Layer toolbar]") {
  gui::ToolbarModel toolbar;
  toolbar.layers = {"errors", "labels", "subcell", "m1", "m2"};  // from `set current_toolbar {m1 m2}`
  const std::vector<std::string> names = {"poly", "m2", "ndiff", "m1"};
  CHECK(toolbar_order(names, toolbar) == std::vector<std::size_t>{3, 1, 0, 2});
}

TEST_CASE("the reorder dialog moves, groups and deletes layers",
          "[cap:ui-gui-managers][req:Toolbar order presets file]") {
  ReorderModel dialog({"locali", "metal1", "mcon", "poly", "ndiff"}, {"current_toolbar", "mine"});
  CHECK(dialog.presets() == std::vector<std::string>{"current_toolbar", "mine"});
  CHECK(dialog.rows().front() == ReorderRow{"locali", 1});
  CHECK(dialog.filtered("MET") == std::vector<std::size_t>{1});
  CHECK_FALSE(dialog.changed());

  CHECK(dialog.move(3, 0));  // poly to the top
  CHECK(dialog.layers() == std::vector<std::string>{"poly", "locali", "metal1", "mcon", "ndiff"});
  CHECK(dialog.selection() == std::set<std::size_t>{0});
  CHECK(dialog.changed());

  dialog.set_selection({2, 3});  // metal1 and mcon on one toolbar row
  CHECK(dialog.group_selected());
  const auto rows = dialog.rows();
  CHECK(rows[2] == ReorderRow{"metal1", 3});
  CHECK(rows[3] == ReorderRow{"mcon", 3});
  CHECK(rows[4] == ReorderRow{"ndiff", 4});

  dialog.set_selection({4});
  CHECK(dialog.delete_selected());
  CHECK(dialog.layers() == std::vector<std::string>{"poly", "locali", "metal1", "mcon"});
  CHECK_FALSE(dialog.delete_selected());  // nothing selected
}

TEST_CASE("Save rewrites the presets file's current_toolbar line, as data",
          "[cap:ui-gui-managers][req:Toolbar order presets file]") {
  ReorderModel dialog({"m1", "m2"}, {"current_toolbar", "mine"});
  dialog.replace({"poly", "m1"});  // Load "mine"
  CHECK(dialog.saved_text("set current_toolbar {m1 m2}\nset mine {poly m1}\n") ==
        "set current_toolbar {poly m1}\nset mine {poly m1}\n");
  CHECK(dialog.saved_text("") == "set current_toolbar {poly m1}\n");  // no file yet
}
