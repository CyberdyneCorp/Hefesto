// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wizard commands VIEW provides (capability ui-wizard-and-debug):
// `*watch` and its tile overlay, `*coord`, `*winddebug`, `*files`.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/db/plane.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "hefesto/view/tile_overlay.hpp"
#include "view_session.hpp"

namespace db = hefesto::db;
namespace view = hefesto::view;
namespace display = hefesto::display;
namespace tech = hefesto::tech;
using hefesto::test::all_text;
using hefesto::test::ViewSession;

namespace {

struct WizardSession : ViewSession {
  db::CellDef* top = nullptr;
  view::LayoutWindow* win = nullptr;

  WizardSession() {
    top = cell("top", {0, 0, 100, 100});
    win = &window(top);
    editor.edit_cell = top;
  }
};

}  // namespace

TEST_CASE("*watch sets and clears the tile overlay of the window", "[cap:ui-wizard-and-debug][req:Tile structure overlay]") {
  WizardSession s;
  const std::string plane{s.editor.technology->plane_name(1)};
  REQUIRE_FALSE(plane.empty());
  (void)s.win->take_redisplay();
  REQUIRE(s.run("*watch", {plane, "types"}).ok());
  REQUIRE(s.editor.tile_overlays.contains(s.win->id()));
  const view::TileOverlay& o = s.editor.tile_overlays.at(s.win->id());
  CHECK(o.plane == 1);
  CHECK(o.types);
  CHECK_FALSE(o.demo);
  CHECK(o.def == s.top);
  CHECK(s.win->pending_redisplay().all);
  // Scenario "Watch off".
  REQUIRE(s.run("*watch").ok());
  CHECK_FALSE(s.editor.tile_overlays.contains(s.win->id()));
}

TEST_CASE("*watch rejects unknown planes and options", "[cap:ui-wizard-and-debug][req:Tile structure overlay]") {
  WizardSession s;
  const auto bad = s.run("*watch", {"nosuchplane"});
  CHECK_FALSE(bad.ok());
  CHECK(bad.diagnostics.back().code == view::codes::kWatchPlane);
  CHECK(bad.diagnostics.back().text.starts_with("Unrecognized plane: nosuchplane.  Legal names are:\n\t"));
  const std::string plane{s.editor.technology->plane_name(1)};
  const auto opt = s.run("*watch", {plane, "bogus"});
  CHECK_FALSE(opt.ok());
  CHECK(all_text(opt) == "Gee, you don't sound like a wizard!\nUsage: *watch [plane] [demo] [types]");
}

TEST_CASE("the tile overlay outlines every tile in the tile-debug style with a label",
          "[cap:ui-wizard-and-debug][req:Tile structure overlay]") {
  WizardSession s;
  db::PaintRow row{};
  row.fill(tech::TileType{9});
  REQUIRE(s.top->plane(1).paint({10, 10, 20, 20}, row).ok());
  s.editor.tile_overlays[s.win->id()] = view::TileOverlay{1, false, false, s.top, db::kIdentity};
  const display::HighlightScene scene = view::tile_overlay_scene(s.editor, *s.win, {0, 0, 30, 30});
  REQUIRE_FALSE(scene.items.empty());
  std::size_t outlines = 0;
  bool painted = false;
  for (const auto& it : scene.items) {
    if (it.shape == display::HighlightShape::outline) {
      ++outlines;
      CHECK(it.style == view::kStyleDrawTile);
      painted = painted || it.rect == db::Rect{10, 10, 20, 20};
    } else {
      CHECK(it.shape == display::HighlightShape::text);
    }
  }
  CHECK(painted);
  CHECK(outlines * 2 == scene.items.size());  // one label per tile
  // No overlay in another window.
  view::LayoutWindow& other = s.window(s.top);
  CHECK(view::tile_overlay_scene(s.editor, other, {0, 0, 30, 30}).empty());
}

TEST_CASE("*coord prints the box, root and edit cell in root and edit coordinates",
          "[cap:ui-wizard-and-debug][req:Layout wizard commands]") {
  // Scenario "coord output".
  WizardSession s;
  s.editor.box = db::Rect{0, 0, 10, 10};
  const auto r = s.run("*coord");
  REQUIRE(r.ok());
  const std::string out = all_text(r);
  INFO(out);
  CHECK(out.find("Box:\tr=(0,0)::(10,10)\te=(0,0)::(10,10)") != std::string::npos);
  CHECK(out.find("Root cell:\tr=(0,0)::(1,1)\te=(0,0)::(1,1)") != std::string::npos);
  CHECK(out.find("Edit cell:\tr=(0,0)::(100,100)\te=(0,0)::(100,100)") != std::string::npos);
  // With a pointer in the window, its point and the window's root follow.
  s.editor.command_point = hefesto::cmd::PointerState{s.win->id(), {5, 6}, {5, 6, 6, 7}};
  const std::string pointed = all_text(s.run("*coord"));
  CHECK(pointed.starts_with("Point:\tr=(5,6)::(6,7)\te=(5,6)::(6,7)\n"));
  CHECK(pointed.find("Root cell:\tr=(0,0)::(100,100)") != std::string::npos);
}

TEST_CASE("*winddebug toggles command printing", "[cap:ui-wizard-and-debug][req:Window-client wizard commands]") {
  WizardSession s;
  CHECK(all_text(s.run("*winddebug")) == "Window command debugging set to TRUE");
  CHECK(s.editor.windows.print_commands());
  CHECK(all_text(s.run("*winddebug")) == "Window command debugging set to FALSE");
  CHECK_FALSE(s.editor.windows.print_commands());
  CHECK(all_text(s.run("*winddebug", {"x"})) == "Usage:  *winddebug");
}

TEST_CASE("*files reports the first 20 file descriptors", "[cap:ui-wizard-and-debug][req:Window-client wizard commands]") {
  WizardSession s;
  const auto r = s.run("*files");
  REQUIRE(r.ok());
  const std::string out = all_text(r);
  CHECK(out.find(" open files, ") != std::string::npos);
  CHECK(out.ends_with(" unopened file descriptors left"));
}

TEST_CASE("the wizard commands are registered by the view area", "[cap:ui-wizard-and-debug][req:Window-client wizard commands]") {
  hefesto::cmd::CommandRegistry registry;
  REQUIRE(view::register_view_commands(registry));
  for (const char* name : {"*watch", "*coord", "*winddebug", "*files", "feedback"}) {
    INFO(name);
    REQUIRE(registry.find(name) != nullptr);
  }
}
