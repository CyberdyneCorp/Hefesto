// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer toolbar and its presets file (capability ui-gui-managers
// "Layer toolbar", "Toolbar order presets file"; Q-SCRIPT-11, Q-SCRIPT-28).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "gui_fixture.hpp"
#include "hefesto/gui/diag_codes.hpp"
#include "hefesto/gui/toolbar_presets.hpp"

namespace gui = hefesto::gui;
using gui::test::Session;
using Layers = std::vector<std::string>;

TEST_CASE("Toolbar file is not executed", "[cap:ui-gui-managers][req:Layer toolbar]") {
  Session s;
  s.write("/work/small_toolbars.tcl", "set current_toolbar {m1 m2}\nfile delete x.mag\n");
  s.write("/work/x.mag", "magic\n");
  gui::ToolbarModel model;
  const auto r = s.in_command([&](hefesto::cmd::CommandContext& ctx) {
    model = gui::build_toolbar(ctx, {});
    return hefesto::cmd::CommandResult::success();
  });
  REQUIRE(r.ok());
  CHECK(model.layers == Layers{"m1", "m2"});
  CHECK(model.presets_file == "small_toolbars.tcl");
  CHECK(s.services.files().exists("/work/x.mag"));
  // A diagnostic names the second line (Hefesto only: Magic sources the file).
  REQUIRE(r.diagnostics.size() == 1);
  CHECK(r.diagnostics[0].code == gui::codes::kToolbarStatement);
  CHECK(r.diagnostics[0].native_only);
  CHECK(r.diagnostics[0].location ==
        hefesto::diag::Location{hefesto::diag::SourceLocation{"small_toolbars.tcl", 2}});
  // `maketoolbar` reads it the same way.
  const auto made = s.run("maketoolbar .layout1.pane.top");
  REQUIRE(made.ok());
  CHECK(made.diagnostics.size() == 1);
}

TEST_CASE("the toolbar lists the special and technology layers", "[cap:ui-gui-managers][req:Layer toolbar]") {
  Session s;
  gui::ToolbarModel model;
  gui::ToolbarModel plain;
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               model = gui::build_toolbar(ctx, {});
               plain = gui::build_toolbar(ctx, {true, true});
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  REQUIRE(model.layers.size() >= 5);
  CHECK(Layers(model.layers.begin(), model.layers.begin() + 3) == Layers{"errors", "labels", "subcell"});
  CHECK(std::find(model.layers.begin(), model.layers.end(), "poly") != model.layers.end());
  CHECK(model.presets_file.empty());
  CHECK(plain.layers.front() != "errors");
  // "Paint from toolbar".
  const std::string w = ".layout1.pane.top.magic";
  CHECK(gui::toolbar_row_command(w, "poly", false, gui::ToolbarAction::button2) == Layers{w, "paint", "poly"});
  CHECK(gui::toolbar_row_command(w, "poly", false, gui::ToolbarAction::shift_button2) == Layers{w, "erase", "poly"});
  CHECK(gui::toolbar_row_command(w, "poly", false, gui::ToolbarAction::button3) == Layers{w, "see", "no", "poly"});
  CHECK(gui::toolbar_row_command(w, "poly", false, gui::ToolbarAction::key_shift_s) ==
        Layers{w, "select", "less", "area", "poly"});
  CHECK(gui::toolbar_row_command(w, "poly", false, gui::ToolbarAction::key_l) == Layers{w, "tech", "lock", "poly"});
  CHECK_FALSE(gui::toolbar_row_command(w, "poly", false, gui::ToolbarAction::key_u).has_value());
  // Locked rows: see and `u` only.
  CHECK_FALSE(gui::toolbar_row_command(w, "poly", true, gui::ToolbarAction::key_p).has_value());
  CHECK(gui::toolbar_row_command(w, "poly", true, gui::ToolbarAction::key_u) == Layers{w, "tech", "unlock", "poly"});
  CHECK(gui::toolbar_row_command(w, "poly", true, gui::ToolbarAction::button1) == Layers{w, "see", "poly"});
  CHECK(gui::toolbar_row_message("poly", true) == "poly (locked)");
  // `see` updates the icons; aliases are ignored.
  gui::ToolbarModel icons{{"poly", "metal1"}, {}, {}, {}};
  gui::toolbar_apply_see(icons, {"no", "poly,metal1"});
  CHECK(icons.hidden == Layers{"poly", "metal1"});
  gui::toolbar_apply_see(icons, {"metal1", "*", "$"});
  CHECK(icons.hidden == Layers{"poly"});
}

TEST_CASE("the presets file is read as data", "[cap:ui-gui-managers][req:Toolbar order presets file]") {
  const gui::PresetFile f = gui::parse_toolbar_presets(
      "# comment\n\n  set current_toolbar {m1 m2}\r\nset mine \"poly  m1\"\nset bad {a {b}}\nset x $y\n"
      "set q \"[exec rm]\"\nputs hi\nset mine {m3}\nset");
  CHECK(f.names() == Layers{"current_toolbar", "mine"});
  REQUIRE(f.find("mine") != nullptr);
  CHECK(f.find("mine")->layers == Layers{"m3"});  // the last set counts
  CHECK(f.presets[1].layers == Layers{"poly", "m1"});
  std::vector<std::size_t> skipped;
  for (const auto& st : f.skipped) {
    skipped.push_back(st.line);
  }
  CHECK(skipped == std::vector<std::size_t>{5, 6, 7, 8, 10});
  CHECK(gui::parse_toolbar_presets("").presets.empty());
  // Save: every `set current_toolbar {...}` line is rewritten.
  CHECK(gui::rewrite_current_toolbar("set current_toolbar {a b}\nset mine {x}\n  set current_toolbar {} ;# c\n",
                                     {"poly", "m1"}) ==
        "set current_toolbar {poly m1}\nset mine {x}\n  set current_toolbar {poly m1} ;# c\n");
  CHECK(gui::rewrite_current_toolbar("set current_toolbar {", {"a"}) == "set current_toolbar {");
  CHECK(gui::write_current_toolbar(gui::ToolbarModel{{"a", "b"}, {}, {}, {}}) == "set current_toolbar {a b}\n");
  CHECK(gui::toolbar_presets_file_name("sky130A") == "sky130A_toolbars.tcl");
}

TEST_CASE("Preset persisted", "[cap:ui-gui-managers][req:Toolbar order presets file]") {
  Session s;
  Layers names;
  hefesto::diag::Outcome<Layers> none{Layers{}};
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               names = gui::toolbar_preset_names(ctx);  // no file
               none = gui::load_toolbar_preset(ctx, "mine");
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(names == Layers{"default_toolbar"});
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().text.find("small_toolbars.tcl") != std::string::npos);

  s.write("/work/small_toolbars.tcl", "set current_toolbar {m1 m2}\nset mine {poly m1}\n");
  hefesto::diag::Outcome<Layers> loaded{Layers{}};
  hefesto::diag::Outcome<Layers> missing{Layers{}};
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               names = gui::toolbar_preset_names(ctx);
               loaded = gui::load_toolbar_preset(ctx, "mine");
               missing = gui::load_toolbar_preset(ctx, "other");
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(names == Layers{"current_toolbar", "mine"});
  REQUIRE(loaded.ok());
  CHECK(loaded.value() == Layers{"poly", "m1"});
  CHECK(s.read("/work/small_toolbars.tcl") == "set current_toolbar {poly m1}\nset mine {poly m1}\n");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == gui::codes::kToolbarPreset);
  CHECK(missing.error().text.find("small_toolbars.tcl") != std::string::npos);
}
