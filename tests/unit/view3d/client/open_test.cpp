// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Opening and closing the 3D window (view-3d "Opening the 3D window",
// "Platform mechanism of the 3D view"; m10-3d-view tasks 8.1, 8.4; Q-MISC-73,
// 76; Q-MISC-80). Owner: WIND3D.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "client_fixture.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/client.hpp"
#include "hefesto/ui/macros.hpp"

namespace view3d = hefesto::view3d;
using view3d::Mode;
using view3d::test::ClientSession;
using view3d::test::first_error;

TEST_CASE("specialopen wind3d opens one window on the named cell", "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  const auto opened = s.run("specialopen wind3d c3d");
  REQUIRE(opened.ok());
  CHECK(opened.value.to_string().empty());  // `.magic3d` is a top-level window: `wm protocol` succeeds
  const view3d::WindowState& w = s.window();
  CHECK(w.cell == "c3d");
  CHECK(w.toolkit_path == ".magic3d");
  CHECK(w.level == 1);
  CHECK_FALSE(w.cut_box.has_value());
  CHECK(s.display.redraws == 1);
  // A unique prefix of the client name opens it too (WindGetClient).
  s.ok("closewindow .magic3d");
  REQUIRE(s.run("specialopen w c3d .other").ok());
  CHECK(s.window().toolkit_path == ".other");
}

TEST_CASE("Second 3D window refused", "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  s.w3d("view 10 0 0");
  const auto again = s.run("specialopen wind3d");
  CHECK(first_error(again) == "VIEW3D-CLIENT-002: Only one 3D window allowed.");
  CHECK(first_error(s.run("specialopen wind3d nosuchcell")) == "VIEW3D-CLIENT-002: Only one 3D window allowed.");
  CHECK(s.window().view.view_x == 10.0F);  // the open window is untouched
}

TEST_CASE("Unknown cell", "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  const auto r = s.run("specialopen wind3d nosuchcell");
  CHECK(first_error(r) == "VIEW3D-CLIENT-003: Cells cannot be created in the 3D window.");
  CHECK_FALSE(s.editor.view3d.has_window());
  CHECK(s.editor.cells.find("nosuchcell") == nullptr);  // never created
}

TEST_CASE("the rendered cell is the box's cell, else the only window's root", "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  s.value("specialopen wind3d {}");  // an empty name is no name (W3Dcreate's strlen check)
  CHECK(s.window().cell == "c3d");
  s.ok("closewindow .magic3d");
  s.editor.box.reset();
  const auto ambiguous = s.run("specialopen wind3d");
  CHECK(first_error(ambiguous) == "VIEW3D-CLIENT-003: Ambiguous directive:  Put cursor box in one of the windows.");
  CHECK_FALSE(s.editor.view3d.has_window());
  REQUIRE(s.editor.windows.open_frame(s.cell).ok());
  s.value("specialopen wind3d {} .w2");  // a new path: J5 (ON) refuses the closed `.magic3d`
  CHECK(s.window().cell == "c3d");
}

TEST_CASE("the window opens in mask-layer mode with the f-scaled default view",
          "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  const view3d::WindowState& w = s.window();
  CHECK(w.mode == Mode::mask_layer);
  CHECK(w.applied_factor == 10.0F);
  CHECK(s.w3d_value("scroll") == "-500.0 -250.0 0.0");
  CHECK(s.w3d_value("zoom") == "0.0018181817140430212 250.0");  // MISC-3D-02
}

TEST_CASE("No output style", "[cap:view-3d][req:Opening the 3D window]") {
  // Q-MISC-73 amendment: no current output style opens layout-layer mode.
  ClientSession s;
  s.editor.mask.output_styles.current.reset();
  s.ok("specialopen wind3d c3d");
  CHECK(s.window().mode == Mode::layout_layer);
  CHECK(s.w3d_value("scroll") == "-50.0 -25.0 0.0");
  CHECK(s.w3d_value("zoom") == "0.0181818176060915 25.0");
  const auto toggled = s.w3d("cif");
  CHECK(first_error(toggled) == "VIEW3D-CLIENT-005: No CIF output style set!");
  CHECK(s.window().mode == Mode::layout_layer);
  CHECK(s.w3d_value("scroll") == "-50.0 -25.0 0.0");
  CHECK(first_error(s.w3d("render MET1")) == "VIEW3D-CLIENT-005: No CIF output style set!");
  CHECK(first_error(s.w3d("render MET1 1 1")) == "VIEW3D-CLIENT-005: No CIF output style set!");
  CHECK(first_error(s.w3d("see MET1")).find("MET1") != std::string::npos);  // layout mode: no such tile type
}

TEST_CASE("an output style that disappears in mask-layer mode leaves the window in layout-layer mode",
          "[cap:view-3d][req:Opening the 3D window]") {
  // Regression: Magic dereferences the missing style on the next redraw;
  // Hefesto divides by the applied f and keeps working (both modes).
  for (const bool compat : {true, false}) {
    ClientSession s(compat);
    s.ok("specialopen wind3d c3d");
    s.editor.mask.output_styles.current.reset();
    CHECK(s.w3d_value("scroll") == "-50.0 -25.0 0.0");
    CHECK(s.window().mode == Mode::layout_layer);
  }
}

TEST_CASE("closing in layout-layer mode and reopening reports and draws the window's own mode",
          "[cap:view-3d][req:Opening the 3D window]") {
  // Q-MISC-76: Magic keeps the redisplay procedure of the last toggle.
  ClientSession s;
  s.ok("specialopen wind3d c3d .render.magic");
  s.w3d("cif");
  CHECK(s.window().mode == Mode::layout_layer);
  s.w3d("closewindow");
  s.ok("specialopen wind3d c3d .r2");
  CHECK(s.window().mode == Mode::mask_layer);
  CHECK(s.display.last.mode == Mode::mask_layer);
  CHECK(s.display.last.applied_factor == 10.0F);
}

TEST_CASE("specialopen leaves Magic's wm protocol message for a path below a top-level window",
          "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession on(true);
  CHECK(on.run("specialopen wind3d c3d .render.magic").value.to_string() ==
        "window \".render.magic\" isn't a top-level window");
  ClientSession off(false);
  CHECK(off.run("specialopen wind3d c3d .render.magic").value.to_string().empty());
}

TEST_CASE("No OpenGL display in Magic", "[cap:view-3d][req:Platform mechanism of the 3D view]") {
  // The session has no display at all; Magic would refuse anything but OGL.
  ClientSession s;
  REQUIRE_FALSE(s.editor.display_running);
  REQUIRE(s.run("specialopen wind3d c3d").ok());
  CHECK(s.editor.view3d.has_window());
}

TEST_CASE("the wind3d client is no longer an unavailable module", "[cap:view-3d][req:Opening the 3D window]") {
  const hefesto::ui::ClientInfo* client = hefesto::ui::find_client_exact("wind3d");
  REQUIRE(client != nullptr);
  CHECK(hefesto::ui::client_available(*client));
  const auto info = hefesto::diag::find_code("UI-CLIENT-002");
  REQUIRE(info.has_value());
  CHECK(std::string{info->summary}.find("wind3d") == std::string::npos);
  ClientSession s;
  CHECK(first_error(s.run("send wind3d zoom")) == "VIEW3D-CLIENT-009: No 3D window is open.");
}

TEST_CASE("the window's toolkit path runs the 3D window's commands only", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d .render.magic");
  s.take_printed();
  CHECK(s.value(".render.magic view") == "0.0 0.0 0.0");
  CHECK(s.value(".render.magic lev") == "1");  // abbreviations, as WindSendCommand's Lookup
  // A layout command is not a command of the 3D window (WIND_COMMANDS is clear).
  CHECK(s.value(".render.magic box") == "");
  CHECK(s.take_printed() == "VIEW3D-CLIENT-007: Unknown command:\nVIEW3D-CLIENT-008:  'box' at (20, 20)\n");
  CHECK(s.value(".render.magic logcommands suspend") == "");
  CHECK(s.take_printed() ==
        "VIEW3D-CLIENT-007: Unknown command:\nVIEW3D-CLIENT-008:  'logcommands' 'suspend' at (20, 20)\n");
}

TEST_CASE("the wrapper's scrollupdate tag on the 3D window prints Magic's *bypass failures only with magic-compat ON",
          "[cap:view-3d][req:3D view commands]") {
  // wrapper.tcl:905-924: magic::scrollupdate runs `*bypass logcommands
  // suspend|resume`, which the 3D window rejects (MISC-3D-02..04).
  for (const bool compat : {true, false}) {
    ClientSession s(compat);
    s.value("proc ::magic::scrollupdate {args} {}");
    s.value("magic::tag view {magic::scrollupdate %W}");
    s.ok("specialopen wind3d c3d .render.magic");
    s.take_printed();
    CHECK(s.value(".render.magic view") == "0.0 0.0 0.0");
    const std::string expected = compat ? "VIEW3D-CLIENT-007: Unknown command:\n"
                                          "VIEW3D-CLIENT-008:  'logcommands' 'suspend' at (20, 20)\n"
                                          "VIEW3D-CLIENT-007: Unknown command:\n"
                                          "VIEW3D-CLIENT-008:  'logcommands' 'resume' at (20, 20)\n"
                                        : "";
    CHECK(s.take_printed() == expected);
    CHECK(s.value(".render.magic level") == "1");  // not tagged
    CHECK(s.take_printed().empty());
  }
}

TEST_CASE("a closed 3D window's command answers invalid command name in both modes",
          "[cap:view-3d][req:Opening the 3D window]") {
  // Regression: Magic deletes the command on close (GrTOGLDelete), so
  // calling it is Tcl's "invalid command name", never a freed window.
  for (const bool compat : {true, false}) {
    ClientSession s(compat);
    s.ok("specialopen wind3d c3d");
    CHECK(s.value(".magic3d level") == "1");
    CHECK(s.eval(".magic3d closewindow").ok());
    CHECK_FALSE(s.editor.view3d.has_window());
    CHECK(s.display.closes == 1);
    const auto after = s.eval(".magic3d view");
    REQUIRE_FALSE(after.ok());
    CHECK(after.diagnostics.front().text == "invalid command name \".magic3d\"");
    CHECK_FALSE(s.eval(".magic3d").ok());
  }
}

TEST_CASE("a closed 3D window's toolkit path is no command and reopens in both modes",
          "[cap:view-3d][req:Opening the 3D window]") {
  // Regression (Q-MISC-80, MISC-3D-12): Magic's closewindow ends in
  // GrTOGLDelete, which deletes the path's command and destroys the window,
  // so `info commands` no longer lists it and a second specialopen opens.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ClientSession s(compat);
    s.ok("specialopen wind3d c3d");
    CHECK(s.value("info commands .magic3d") == ".magic3d");
    s.w3d("closewindow");
    CHECK(s.value("info commands .magic3d").empty());
    REQUIRE(s.run("specialopen wind3d c3d").ok());
    CHECK(s.value("info commands .magic3d") == ".magic3d");
    CHECK(s.value(".magic3d level") == "1");
    // One window at a time still holds.
    CHECK(first_error(s.run("specialopen wind3d c3d .other")) == "VIEW3D-CLIENT-002: Only one 3D window allowed.");
  }
}

TEST_CASE("closewindow with the 3D window's name closes it", "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d .render.magic");
  s.ok("closewindow .render.magic");
  CHECK_FALSE(s.editor.view3d.has_window());
}

TEST_CASE("render3d copies the box once", "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  s.editor.box = hefesto::db::Rect{0, 0, 10, 10};
  CHECK(s.value("magic::render3d") == "");
  CHECK(s.window().toolkit_path == ".render.magic");
  CHECK(s.value(".render.magic cutbox") == "0 0 10 10");
  CHECK(s.value("set Opts(cutbox)") == "1");
  s.editor.box = hefesto::db::Rect{20, 20, 30, 30};
  CHECK(s.value(".render.magic cutbox") == "0 0 10 10");
}

TEST_CASE("render3d with the box in another cell leaves no cut box", "[cap:view-3d][req:Opening the 3D window]") {
  ClientSession s;
  hefesto::db::CellDef* other = s.editor.cells.create("other", s.editor.technology->plane_count()).value();
  other->flags().available = true;
  s.editor.box_root = other;
  CHECK(s.value("magic::render3d c3d") == "");
  CHECK(s.value(".render.magic cutbox") == "none");
}

TEST_CASE("each default key of the 3D window runs its command", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  CHECK_FALSE(s.editor.macros.has_table("wind3d"));  // installed when a 3D window opens
  s.ok("specialopen wind3d c3d");
  int keys = 0;
  for (const hefesto::ui::StartupBinding& line : hefesto::ui::startup_bindings()) {
    if (line.table != "wind3d") {
      continue;
    }
    ++keys;
    const auto key = hefesto::ui::parse_key_name(line.key);
    REQUIRE(key.ok());
    const hefesto::ui::Macro* macro = s.editor.macros.find("wind3d", key.value());
    REQUIRE(macro != nullptr);
    CHECK(macro->text == line.text);
    const auto r = s.w3d(macro->text);
    INFO(line.key << " -> " << macro->text);
    CHECK(r.ok());
  }
  CHECK(keys == 25);  // proto.magicrc.in:285-310
  CHECK(s.editor.macros.find("wind3d", hefesto::ui::parse_key_name("L").value())->text == "level up");
}
