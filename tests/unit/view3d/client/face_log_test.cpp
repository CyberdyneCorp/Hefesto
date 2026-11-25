// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D window's face log (view3d/client/face_log.hpp; m10 task 12.4):
// the HEFESTO_GL_CAPTURE file hefesto-cli writes in the format of Magic's
// GL capture shim, so that tools/golden/gl_capture/faces.py compares the
// two (design D13). Owner: WIND3D.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "client_fixture.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/view3d/client/face_log.hpp"

namespace view3d = hefesto::view3d;
namespace display = hefesto::display;
using view3d::test::ClientSession;

namespace {

constexpr std::size_t kMetal1 = display::kFixedStyleCount + 20;
constexpr std::size_t kCross = display::kFixedStyleCount + 21;
constexpr std::size_t kMetal3 = display::kFixedStyleCount + 22;
constexpr std::size_t kUnnamed = display::kFixedStyleCount + 23;

display::StyleSet style_set() {
  display::StyleSet set;
  set.table.styles.resize(kUnnamed + 1);
  set.table.styles[kMetal1].long_name = "metal1";
  set.table.styles[kCross].long_name = "contact_X";
  set.table.styles[kCross].fill = display::FillKind::cross;
  set.table.styles[kMetal3].long_name = "metal3";
  set.table.styles[kMetal3].fill = display::FillKind::stipple;
  return set;
}

view3d::StackLayer layer(std::size_t style) {
  view3d::StackLayer l;
  l.name = "l" + std::to_string(style);
  l.style = style;
  return l;
}

// One chunk at (10, 20): a rectangle, a clockwise triangle and the bottom
// side twice (once flagged outward-left, reversed) on layer 0, and the same
// rectangle on layers 1 (a cross style), 2 (hidden) and 3 (no long name).
view3d::Scene scene() {
  view3d::Scene s;
  s.stack = view3d::LayerStack(view3d::Mode::layout_layer,
                               {layer(kMetal1), layer(kCross), layer(kMetal3), layer(kUnnamed)}, true, 1.0);
  s.hidden = {false, false, true, false};
  auto mesh = std::make_shared<view3d::MeshModel>();
  mesh->layer_heights = {{1.0F, 0.5F}, {1.0F, 0.5F}, {1.0F, 0.5F}, {1.0F, 0.5F}};
  view3d::Chunk c;
  c.origin_x = 10.0;
  c.origin_y = 20.0;
  c.rects.push_back({0.0F, 0.0F, 5.0F, 5.0F, 0});
  c.triangles.push_back(view3d::Triangle{{0.0F, 0.0F, 5.0F}, {0.0F, 5.0F, 0.0F}, 0, 1.0F, 1.0F});
  c.sides.push_back({0.0F, 0.0F, 5.0F, 0.0F, 0, 0, 0.0F, 0.0F});
  c.sides.push_back({5.0F, 0.0F, 0.0F, 0.0F, 0, view3d::side_flag::outward_left, 0.0F, 0.0F});
  for (view3d::LayerId id = 1; id <= 3; ++id) {
    c.rects.push_back({0.0F, 0.0F, 5.0F, 5.0F, id});
  }
  mesh->chunks.push_back(std::move(c));
  s.mesh = std::move(mesh);
  return s;
}

}  // namespace

TEST_CASE("the face log has the GL capture shim's format and Magic's winding",
          "[cap:view-3d][req:Rendered geometry]") {
  view3d::ViewState view;
  view.scale_z = 2.0F;
  const display::StyleSet styles = style_set();
  // z top = -h * s_z = -2, z bottom = z top - t * s_z = -3 (triangle: -4).
  CHECK(view3d::face_log(scene(), view, &styles) ==
        "gl-capture 1\n"
        "poly metal1 4 10 20 -2 15 20 -2 15 25 -2 10 25 -2\n"   // top, counter-clockwise
        "poly metal1 4 10 25 -3 15 25 -3 15 20 -3 10 20 -3\n"   // bottom, reversed
        "poly #75 4 10 20 -2 15 20 -2 15 25 -2 10 25 -2\n"      // no long name
        "poly #75 4 10 25 -3 15 25 -3 15 20 -3 10 20 -3\n"
        "poly metal1 3 10 20 -2 15 20 -2 10 25 -2\n"            // a clockwise triangle turned
        "poly metal1 3 10 25 -4 15 20 -4 10 20 -4\n"
        "poly metal1 4 10 20 -3 15 20 -3 15 20 -2 10 20 -2\n"   // outside on the right of a -> b
        "poly metal1 4 10 20 -3 15 20 -3 15 20 -2 10 20 -2\n");  // outward-left: the same face
}

TEST_CASE("the face log without a style set draws every layer, named by style index",
          "[cap:view-3d][req:Rendered geometry]") {
  const std::string log = view3d::face_log(scene(), view3d::ViewState{}, nullptr);
  CHECK(log.find("poly #72 ") != std::string::npos);
  CHECK(log.find("poly #73 ") != std::string::npos);  // no style set: no fill to filter on
  CHECK(log.find("poly #74 ") == std::string::npos);  // hidden
}

TEST_CASE("only solid and stippled styles are in the face log in both modes (N3)",
          "[cap:view-3d][req:Rendered geometry]") {
  // W3Dmain.c:406, :439: a tile in a cross or outline style is not drawn,
  // so a contact also bound to a cross style (MISC-3D-15) has no faces in it.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ClientSession s(compat);
    s.editor.styles.emplace(style_set());
    auto& types = s.editor.styles->types.styles_of_type;
    types.resize(hefesto::tech::kMaxTileTypes);
    types[*s.editor.technology->find_type("metal1").id] = {kMetal1};
    types[*s.editor.technology->find_type("metal2").id] = {kMetal3, kCross};
    view3d::FaceLogDisplay log{s.services.view().files, "/t/gl.log", &s.display};
    s.editor.view3d.display = &log;
    s.ok("specialopen wind3d c3d");
    CHECK(s.display.redraws == 1);  // forwarded
    s.w3d("cif");                   // the layout layers (the window opens in mask-layer mode)
    const auto text = s.services.view().files.read("/t/gl.log");
    REQUIRE(text.ok());
    CHECK(text.value().starts_with("gl-capture 1\npoly "));
    CHECK(text.value().find("poly metal1 ") != std::string::npos);
    CHECK(text.value().find("poly metal3 ") != std::string::npos);
    CHECK(text.value().find("contact_X") == std::string::npos);
    s.w3d("closewindow");
    CHECK(s.display.closes == 1);
  }
}
