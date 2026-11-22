// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wind3d` commands (view-3d "3D view commands", "Default view";
// m10-3d-view tasks 8.2, 8.3; Q-MISC-71, 73, 78). Owner: WIND3D.

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <string>

#include "client_fixture.hpp"

namespace view3d = hefesto::view3d;
using view3d::Mode;
using view3d::test::ClientSession;
using view3d::test::first_error;

namespace {

// An open window in layout-layer mode (the default view of Magic's
// layout-layer display: (-50, -25, 0), s_xy 2/110, s_z 25).
void open_layout(ClientSession& s) {
  s.ok("specialopen wind3d c3d");
  REQUIRE(s.w3d("cif").ok());
  REQUIRE(s.window().mode == Mode::layout_layer);
}

}  // namespace

TEST_CASE("Relative rotation", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  CHECK(s.w3d_value("view 0 10 0 rel") == "");
  CHECK(s.w3d_value("view 0 10 0 relative") == "");
  CHECK(s.w3d_value("view") == "0.0 20.0 0.0");
  CHECK(s.w3d_value("view 1 2 3 abs") == "");
  CHECK(s.w3d_value("view") == "1.0 2.0 3.0");
  CHECK(s.w3d_value("view 4 5 6") == "");
  CHECK(s.w3d_value("view") == "4.0 5.0 6.0");
}

TEST_CASE("Relative scroll in screen units", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  s.w3d("zoom 0.02 25");
  s.w3d("scroll -50 -25 0");
  CHECK(s.w3d_value("scroll 0.25 0 0 rel") == "");
  CHECK(s.w3d_value("scroll") == "-37.5 -25.0 0.0");
}

TEST_CASE("relative scroll matches Magic's float values in both modes (MISC-3D-03)",
          "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  s.w3d("scroll 0.25 0 0 rel");
  CHECK(s.w3d_value("scroll") == "-362.5 -250.0 0.0");
  s.w3d("cif");
  CHECK(s.w3d_value("scroll") == "-36.25 -25.0 0.0");
  s.w3d("scroll 0.25 0 0 rel");
  CHECK(s.w3d_value("scroll") == "-22.5 -25.0 0.0");
}

TEST_CASE("Zoom rejects zero", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  const std::string before = s.w3d_value("zoom");
  CHECK(first_error(s.w3d("zoom 0 1")) ==
        "VIEW3D-CLIENT-004: Error: zoom values/factors must be positive and nonzero");
  CHECK(first_error(s.w3d("zoom -1 1")) ==
        "VIEW3D-CLIENT-004: Error: zoom values/factors must be positive and nonzero");
  CHECK(first_error(s.w3d("zoom 1 0 rel")) ==
        "VIEW3D-CLIENT-004: Error: zoom values/factors must be positive and nonzero");
  CHECK(s.w3d_value("zoom") == before);
  CHECK(s.w3d_value("zoom 2 0.5 rel") == "");
  CHECK(s.w3d_value("zoom") == "0.0036363634280860424 125.0");
}

TEST_CASE("every 3D command's usage error", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  const hefesto::view3d::ViewState before = s.window().view;
  const std::pair<const char*, const char*> cases[] = {
      {"view 1 2 3 sideways", "Usage: view angle_x angle_y angle_z absolute|relative"},
      {"view 1 2", "Usage: view [angle_x angle_y angle_z [relative|absolute]]"},
      {"view 1 2 3 rel 5", "Usage: view [angle_x angle_y angle_z [relative|absolute]]"},
      {"scroll 1 2 3 re", "Usage: scroll pos_x pos_y pos_z absolute|relative"},
      {"scroll 1", "Usage: scroll [pos_x pos_y pos_z [absolute|relative]]"},
      {"zoom 1 2 x", "Usage: zoom scale_xy scale_z relative|absolute"},
      {"zoom 1", "Usage: zoom [scale_xy scale_z [relative|absolute]]"},
      {"level 1 2", "Usage: level [n]"},
      {"level sideways", "Usage: level [<n>|up|down]"},
      {"defaults now", "Usage: defaults"},
      {"refresh now", "Usage: refresh"},
      {"closewindow now", "Usage: closewindow"},
      {"help me", "Usage: help"},
      {"cif now", "Usage: cif"},
      {"render", "Usage: render name [height thick [style]]"},
      {"render MET1 1", "Usage: render name [height thick [style]]"},
      {"render MET1 x 1", "Usage: render name [height thick [style]]"},
      {"see", "Usage: see [no] layer"},
      {"see a b c", "Usage: see [no] layer"},
      {"cutbox 1 2", "Usage: cutbox [none|box|llx lly urx ur]"},
  };
  for (const auto& [line, text] : cases) {
    INFO(line);
    CHECK(first_error(s.w3d(line)) == std::string{"VIEW3D-CLIENT-001: "} + text);
  }
  CHECK(s.window().view == before);
  CHECK(s.editor.view3d.has_window());
}

TEST_CASE("non-numeric values change nothing and print nothing, as in Magic", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  const hefesto::view3d::ViewState before = s.window().view;
  for (const char* line : {"view a 1 2", "scroll 1 b 2 rel", "zoom x 2", "cutbox 1 2 3 x"}) {
    const auto r = s.w3d(line);
    INFO(line);
    CHECK(r.ok());
    CHECK(r.diagnostics.empty());
  }
  CHECK(s.window().view == before);
  CHECK_FALSE(s.window().cut_box.has_value());
  // StrIsNumeric accepts a numeric prefix, as atof reads it.
  s.w3d("view 5x 0 0");
  CHECK(s.w3d_value("view") == "5.0 0.0 0.0");
}

TEST_CASE("level is clamped at zero", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  CHECK(s.w3d_value("level") == "1");
  s.w3d("level up");
  CHECK(s.w3d_value("level") == "2");
  s.w3d("level -5");
  CHECK(s.w3d_value("level") == "0");
  s.w3d("level down");
  CHECK(s.w3d_value("level") == "0");
  s.w3d("level 3");
  CHECK(s.w3d_value("level") == "3");
  // Regression: atoi overflow and `level up` at INT_MAX are undefined in
  // Magic; the value saturates.
  s.w3d("level 99999999999");
  CHECK(s.window().level == INT_MAX);
  s.w3d("level up");
  CHECK(s.window().level == INT_MAX);
}

TEST_CASE("help lists the 3D window's commands", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  s.take_printed();
  REQUIRE(s.w3d("help").ok());
  const std::string printed = s.take_printed();
  CHECK(printed.starts_with("VIEW3D-CLIENT-008: \nWind3D command summary:\n    cif\t\t\tswitch to/from CIF layers"));
  CHECK(printed.find("    zoom [xy z]\t\tspecify render volume scale\n\nType '?' in the window to get a key macro "
                     "summary.") != std::string::npos);
}

TEST_CASE("the cif round trip returns Magic's float values (MISC-3D-02)", "[cap:view-3d][req:Default view]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  s.w3d("cif");
  CHECK(s.w3d_value("scroll") == "-50.0 -25.0 0.0");
  CHECK(s.w3d_value("zoom") == "0.0181818176060915 25.0");
  s.w3d("defaults");
  CHECK(s.w3d_value("zoom") == "0.0181818176060915 25.0");
  s.w3d("cif");
  CHECK(s.w3d_value("scroll") == "-500.0 -250.0 0.0");
  CHECK(s.w3d_value("zoom") == "0.0018181817140430212 250.0");
  s.w3d("defaults");
  CHECK(s.w3d_value("scroll") == "-500.0 -250.0 0.0");
  CHECK(s.w3d_value("zoom") == "0.0018181817140430212 250.0");
}

TEST_CASE("the camera's f is the scale factor after the GCF reduction", "[cap:view-3d][req:Default view]") {
  // m10 tasks.md 8.1 note (d): `scalefactor 10 nanometers` reduces to f = 1.
  ClientSession s;
  s.ok("cif ostyle nano");
  CHECK(view3d::current_factor(s.editor) == 1.0F);
  s.ok("specialopen wind3d c3d");
  CHECK(s.w3d_value("scroll") == "-50.0 -25.0 0.0");
}

TEST_CASE("Output style changed in mask-layer mode", "[cap:view-3d][req:3D view commands]") {
  // Q-MISC-78: ON keeps Magic's drift, OFF rescales by the applied f.
  SECTION("magic-compat ON") {
    ClientSession s(true);
    open_layout(s);
    REQUIRE(s.w3d_value("zoom") == "0.0181818176060915 25.0");
    s.w3d("cif");
    s.ok("cif ostyle coarse");
    s.w3d("cif");
    CHECK(s.window().view.scale_z == 50.0F);
    CHECK(s.w3d_value("scroll") == "-100.0 -50.0 0.0");
  }
  SECTION("magic-compat OFF") {
    ClientSession s(false);
    open_layout(s);
    s.w3d("cif");
    s.ok("cif ostyle coarse");
    s.w3d("cif");
    CHECK(s.window().view.scale_z == 25.0F);
    CHECK(s.w3d_value("scroll") == "-50.0 -25.0 0.0");
  }
}

TEST_CASE("defaults in mask-layer mode after a style change", "[cap:view-3d][req:Default view]") {
  for (const bool compat : {true, false}) {
    ClientSession s(compat);
    s.ok("specialopen wind3d c3d");
    s.ok("cif ostyle coarse");
    s.w3d("defaults");
    CHECK(s.w3d_value("scroll") == "-250.0 -125.0 0.0");
    CHECK(s.window().view.scale_z == 125.0F);
  }
}

TEST_CASE("Visibility sets are per mode", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  REQUIRE(s.w3d("see no MET1").ok());
  const std::vector<bool> hidden_met1 = {false, true, false};
  CHECK(s.window().visibility.mask_hidden == hidden_met1);
  s.w3d("cif");
  CHECK(s.window().visibility.layout_hidden.empty());
  REQUIRE(s.w3d("see no metal1").ok());
  CHECK(s.window().visibility.layout_hidden.test(*s.editor.technology->find_type("metal1").id));
  s.w3d("cif");
  CHECK(s.window().visibility.mask_hidden == hidden_met1);
  s.w3d("see MET1");
  CHECK(s.window().visibility.mask_hidden == std::vector<bool>{false, false, false});
  // A change of the output style resets the mask set (Q-MISC-71).
  s.w3d("see no MET2");
  s.ok("cif ostyle coarse");
  CHECK(s.w3d_value("level") == "1");
  CHECK(s.window().visibility.mask_hidden.empty());
  // `defaults` resets both sets.
  s.w3d("see no MET1");
  s.w3d("defaults");
  CHECK(s.window().visibility == hefesto::view3d::VisibilitySets{});
}

TEST_CASE("see reports unknown layers with Magic's messages", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  CHECK(first_error(s.w3d("see NOSUCH")) ==
        "VIEW3D-CLIENT-006: CIF name \"NOSUCH\" doesn't exist in style \"gdsii\".\n"
        "The valid CIF layer names are: POLY, MET1, MET2.");
  s.w3d("cif");
  CHECK(first_error(s.w3d("see nosuch")).find("nosuch") != std::string::npos);
  CHECK(s.window().visibility.layout_hidden.empty());
  // `see X Y` with X other than `no` shows Y (w3dSeeLayers).
  s.w3d("see no metal1");
  s.w3d("see yes metal1");
  CHECK(s.window().visibility.layout_hidden.empty());
}

TEST_CASE("render reports and sets a mask layer's values", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  // No display style table in the test session: the pale-highlight style.
  CHECK(s.w3d_value("render MET1") == "1.3760000467300415 0.36000001430511475 -49");
  CHECK(s.w3d_value("render MET2") == "0.0 0.0 -49");
  CHECK(first_error(s.w3d("render NOSUCH")) == "VIEW3D-CLIENT-006: Unknown CIF layer \"NOSUCH\"");
  CHECK(first_error(s.w3d("render NOSUCH 1 1")) == "VIEW3D-CLIENT-006: Unknown CIF layer \"NOSUCH\"");
  CHECK(s.w3d_value("render MET1 2 0.5") == "");
  CHECK(s.w3d_value("render MET1") == "2.0 0.5 -49");
  s.w3d("render MET1 2 0.5 7");
  CHECK(s.w3d_value("render MET1") == "2.0 0.5 7");
  s.w3d("render MET1 3 1 -2");  // a negative style is ignored, as Magic's `style >= 0`
  CHECK(s.w3d_value("render MET1") == "3.0 1.0 7");
  s.w3d("render MET1 3 1 x");  // a style that is not an integer too
  CHECK(s.w3d_value("render MET1") == "3.0 1.0 7");
  // Magic re-reads a style when it becomes current: the values start over
  // (the client sees the change at its next command).
  s.ok("cif ostyle coarse");
  CHECK(s.w3d_value("render MET1") == "0.0 0.0 -49");
  s.ok("cif ostyle gdsii");
  CHECK(s.w3d_value("render MET1") == "1.3760000467300415 0.36000001430511475 -49");
}

TEST_CASE("Bad style reported when the layer stack is built",
          "[cap:view-3d][req:Render statement in the cifoutput section]") {
  // Regression (PR #81 review): the stack was built with a null sink, so
  // the error never reached the user.
  std::string tech{view3d::test::kTech};
  const std::string good = "render MET1 metal1 1.376 0.36";
  tech.replace(tech.find(good), good.size(), "render MET1 nosuchstyle 1.376 0.36");
  ClientSession s(true, tech);
  CHECK(s.take_printed().find("Bad render style") == std::string::npos);  // nothing at load
  hefesto::display::StyleSet styles;
  styles.table.styles.resize(hefesto::display::kFixedStyleCount);
  hefesto::display::DisplayStyle metal1;
  metal1.ordinal = 20;
  metal1.long_name = "metal1";
  styles.table.styles.push_back(metal1);
  styles.table.layout_count = 1;
  s.editor.styles = std::move(styles);
  s.ok("specialopen wind3d c3d");
  s.take_printed();
  const std::string error = "VIEW3D-STACK-002: Error:  Bad render style for CIF layer.\n";
  for (int build = 0; build < 2; ++build) {
    CHECK(s.w3d_value("render MET1") == "0.0 0.0 -49");  // the statement is ignored
    CHECK(s.take_printed() == error);
  }
}

TEST_CASE("render values outlive the window while the style stays current", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  s.w3d("render MET2 4 2");
  s.w3d("closewindow");
  s.ok("specialopen wind3d c3d .again");
  CHECK(s.w3d_value("render MET2") == "4.0 2.0 -49");
}

TEST_CASE("cutbox forms", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  CHECK(s.w3d_value("cutbox") == "none");
  s.w3d("cutbox 0 0 40 20");
  CHECK(s.w3d_value("cutbox") == "0 0 40 20");
  s.w3d("cutbox none");
  CHECK(s.w3d_value("cutbox") == "none");
  s.editor.box = hefesto::db::Rect{10, 10, 20, 30};
  s.w3d("cutbox box");
  CHECK(s.w3d_value("cutbox") == "10 10 20 30");
  s.w3d("cutbox anything");  // redraws, changes nothing
  CHECK(s.w3d_value("cutbox") == "10 10 20 30");
  s.w3d("defaults");  // Set3DDefaults: no clipping
  CHECK(s.w3d_value("cutbox") == "none");
}

TEST_CASE("every successful change redraws the window", "[cap:view-3d][req:3D view commands]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  int expected = s.display.redraws;
  for (const char* line : {"view 0 1 0 rel", "scroll 0 0 0", "zoom 1 1 rel", "refresh", "level up", "defaults",
                           "cif", "see no metal1", "cutbox none"}) {
    INFO(line);
    REQUIRE(s.w3d(line).ok());
    CHECK(s.display.redraws == ++expected);
  }
  s.w3d("zoom 0 1");
  s.w3d("view 1 2");
  CHECK(s.display.redraws == expected);
}
