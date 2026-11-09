// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Geometry of the canvas overlay (rulers, minimap, zoom control) and the
// mapping between the core window's ViewMapping and the renderer camera
// (design.md D5).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "hefesto/shell/canvas_overlay.hpp"

using namespace hefesto;
using namespace hefesto::shell;
using Catch::Approx;

TEST_CASE("ruler ticks use 1-2-5 steps in display units", "[cap:hefesto-app-ux][req:Main window layout]") {
  const UnitScale lambda{1, 1, std::nullopt};
  const auto ticks = ruler_ticks(0.0, 1000.0, 800.0, lambda, 120.0);
  REQUIRE(ticks.size() >= 2);
  CHECK(ticks[0].label == "0");
  CHECK(ticks[1].label == "200");  // 1000 units over 800 px, >= 120 px apart: step 200
  CHECK(ticks[1].pixel == Approx(160.0));
  // Vertical ruler: world y decreases down the screen.
  const auto vertical = ruler_ticks(100.0, 0.0, 400.0, lambda, 120.0);
  REQUIRE_FALSE(vertical.empty());
  CHECK(vertical.front().label == "0");
  CHECK(vertical.front().pixel == Approx(400.0));
  const UnitScale microns{1, 1, 0.005};
  const auto um = ruler_ticks(0.0, 276.0, 420.0, microns, 120.0);
  REQUIRE(um.size() >= 2);
  CHECK(um[1].label == "0.5");
  CHECK(ruler_ticks(0.0, 0.0, 100.0, lambda).empty());
}

TEST_CASE("minimap fits the cell and outlines the visible window", "[cap:hefesto-app-ux][req:Main window layout]") {
  const db::Rect bounds{0, 0, 100, 200};
  const render::WorldWindow visible{0.0, 0.0, 50.0, 100.0};
  const auto g = minimap_geometry(bounds, visible, kMinimapWidth, kMinimapHeight, 12.0);
  REQUIRE(g.has_value());
  CHECK(g->content.h == Approx(kMinimapHeight - 24.0));  // limited by height
  CHECK(g->content.w == Approx(g->content.h / 2.0));
  CHECK(g->content.x + g->content.w / 2.0 == Approx(kMinimapWidth / 2.0));
  CHECK(g->view.w == Approx(g->content.w / 2.0));
  CHECK(g->view.y + g->view.h == Approx(g->content.y + g->content.h));  // lower half: bottom aligned
  const db::Point center = minimap_to_world(bounds, kMinimapWidth, kMinimapHeight, kMinimapWidth / 2.0,
                                            kMinimapHeight / 2.0, 12.0);
  CHECK(center.x == 50);
  CHECK(center.y == 100);
  CHECK_FALSE(minimap_geometry(db::Rect{0, 0, -1, -1}, visible, 168, 112).has_value());
}

TEST_CASE("zoom control and minimap sit where the mockup puts them", "[cap:hefesto-app-ux][req:Main window layout]") {
  const OverlayLayout l = overlay_layout(868.0, 772.0);
  CHECK(l.zoom_out.x == Approx(36.0));
  CHECK(l.zoom_out.y + l.zoom_out.h == Approx(772.0 - kCanvasInset));
  CHECK(l.minimap.w == Approx(kMinimapWidth));
  CHECK(l.minimap.h == Approx(kMinimapHeight));
  CHECK(l.minimap.x + l.minimap.w == Approx(868.0 - kCanvasInset));
  CHECK(hit_test(l, l.zoom_in.x + 1.0, l.zoom_in.y + 1.0) == OverlayHit::zoom_in);
  CHECK(hit_test(l, l.zoom_out.x + 1.0, l.zoom_out.y + 1.0) == OverlayHit::zoom_out);
  CHECK(hit_test(l, l.minimap.x + 5.0, l.minimap.y + 5.0) == OverlayHit::minimap);
  CHECK(hit_test(l, 400.0, 300.0) == OverlayHit::none);
  CHECK(zoom_percent_text(4.0, 1.0) == "400%");
  CHECK(zoom_percent_text(0.05, 1.0) == "5.0%");
  CHECK(zoom_percent_text(1.0, 0.0) == "100%");
}

TEST_CASE("the camera follows the core window's view mapping", "[cap:ui-windows-and-views][req:View mapping]") {
  render::Camera camera(500.0, 250.0, 0.5);
  camera.set_viewport(800, 600);
  const view::ViewMapping mapping = mapping_from_camera(camera);
  CHECK(mapping.screen.x1 == 799);
  CHECK(mapping.screen.y1 == 599);
  CHECK(mapping.scale == 32768);  // 0.5 px per unit in 1/65536 px

  render::Camera back;
  back.set_viewport(800, 600);
  camera_from_mapping(mapping, back);
  CHECK(back.pixels_per_unit() == Approx(0.5));
  CHECK(back.center_x() == Approx(500.0).margin(2.0));
  CHECK(back.center_y() == Approx(250.0).margin(2.0));
  CHECK(back.viewport_width() == 800);

  // ViewMapping::fit (the core's formula) shows the area in the camera too.
  const view::ViewMapping fit = view::ViewMapping::fit(view::ScreenRect{0, 0, 799, 599}, db::Rect{0, 0, 1000, 1000});
  render::Camera fitted;
  fitted.set_viewport(800, 600);
  camera_from_mapping(fit, fitted);
  const render::WorldWindow w = fitted.visible_window();
  CHECK(w.x0 <= 0.0);
  CHECK(w.x1 >= 1000.0);
  CHECK(w.y0 <= 0.0 + 1.0);
  CHECK(w.y1 >= 1000.0 - 1.0);
}

TEST_CASE("the box outline maps world to canvas pixels", "[cap:hefesto-app-ux][req:Main window layout]") {
  render::Camera camera(0.0, 0.0, 2.0);
  camera.set_viewport(400, 200);
  const PixelRect r = world_to_pixels(camera, db::Rect{0, 0, 10, 10}, 2.0);
  CHECK(r.x == Approx(100.0));  // center of a 200-logical-px-wide canvas
  CHECK(r.y == Approx(40.0));
  CHECK(r.w == Approx(10.0));
  CHECK(r.h == Approx(10.0));
}

TEST_CASE("left ruler labels are compact and never wider than the ruler",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  CHECK(compact_ruler_label("0.5") == ".5");
  CHECK(compact_ruler_label("-0.25") == "-.25");
  CHECK(compact_ruler_label("1000") == "1k");
  CHECK(compact_ruler_label("-1500") == "-1.5k");
  CHECK(compact_ruler_label("-500") == "-500");
  CHECK(compact_ruler_label("0") == "0");
  CHECK(compact_ruler_label("2.7") == "2.7");
  // A label that does not fit is stacked one character per line.
  CHECK(ruler_label_lines("0", 2) == std::vector<std::string>{"0"});
  CHECK(ruler_label_lines("-500", 2) == std::vector<std::string>{"-", "5", "0", "0"});
  for (const char* label : {"-500", "1.5k", "-.25", "12345"}) {
    for (const std::string& line : ruler_label_lines(compact_ruler_label(label), 2)) CHECK(line.size() <= 2);
  }
}

TEST_CASE("label text is placed like Magic's GrPutText", "[cap:display-style-files][req:Label display]") {
  const PixelRect r{100.0, 200.0, 20.0, 10.0};
  // Center: centered on the rectangle.
  const PixelPoint c = label_text_origin(r, db::kCenter, 30.0, 12.0);
  CHECK(c.x == Approx(95.0));
  CHECK(c.y == Approx(199.0));
  // North: above the top edge (y down), 5 px away, centered horizontally.
  const PixelPoint n = label_text_origin(r, db::kNorth, 30.0, 12.0);
  CHECK(n.x == Approx(95.0));
  CHECK(n.y == Approx(200.0 - 5.0 - 12.0));
  // East: right of the right edge, vertically centered.
  const PixelPoint e = label_text_origin(r, 3, 30.0, 12.0);
  CHECK(e.x == Approx(125.0));
  CHECK(e.y == Approx(199.0));
  // South-west: left of the left edge, below the bottom edge.
  const PixelPoint sw = label_text_origin(r, 6, 30.0, 12.0);
  CHECK(sw.x == Approx(100.0 - 5.0 - 30.0));
  CHECK(sw.y == Approx(215.0));
}

TEST_CASE("font-label glyphs fill with the nonzero rule", "[cap:display-style-files][req:Label display]") {
  // An outer square with an inner square of the opposite direction: a hole.
  const std::vector<PixelPoint> outer{{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};
  const std::vector<PixelPoint> hole{{3.0, 3.0}, {3.0, 7.0}, {7.0, 7.0}, {7.0, 3.0}};
  const auto row = nonzero_spans({outer, hole}, 5.0);
  REQUIRE(row.size() == 2);
  CHECK(row[0].x0 == Approx(0.0));
  CHECK(row[0].x1 == Approx(3.0));
  CHECK(row[1].x0 == Approx(7.0));
  CHECK(row[1].x1 == Approx(10.0));
  // Same direction twice: still filled (nonzero, not even-odd).
  const std::vector<PixelPoint> same{{3.0, 3.0}, {7.0, 3.0}, {7.0, 7.0}, {3.0, 7.0}};
  const auto filled = nonzero_spans({outer, same}, 5.0);
  REQUIRE_FALSE(filled.empty());
  CHECK(filled.front().x0 == Approx(0.0));
  CHECK(filled.back().x1 == Approx(10.0));
  CHECK(nonzero_spans({outer}, 11.0).empty());
}

TEST_CASE("world points map to canvas pixels like world rectangles", "[cap:hefesto-app-ux][req:Main window layout]") {
  render::Camera cam(50.0, 50.0, 2.0);
  cam.set_viewport(200, 200);
  const PixelPoint p = world_point_to_pixels(cam, 0.0, 100.0, 2.0);
  const PixelRect r = world_to_pixels(cam, db::Rect{0, 0, 100, 100}, 2.0);
  CHECK(p.x == Approx(r.x));
  CHECK(p.y == Approx(r.y));
}

TEST_CASE("bitmap label text size follows the zoom as Magic picks it", "[cap:ui-windows-and-views][req:Label display]") {
  using hefesto::shell::bitmap_label_size;
  // B heights: 7 (10 px), 10 (14 px), 13 (18 px), 17 (24 px).
  CHECK_FALSE(bitmap_label_size(2.0));          // 3 × 2 <= 7: rectangle only
  CHECK(bitmap_label_size(2.5) == 10.0);        // under every B: the smallest size
  CHECK(bitmap_label_size(7.5) == 10.0);
  CHECK(bitmap_label_size(10.5) == 14.0);
  CHECK(bitmap_label_size(13.5) == 18.0);
  CHECK(bitmap_label_size(100.0) == 24.0);
}

TEST_CASE("the box is drawn with Magic's shapes, sides inside the rectangle",
          "[cap:ui-windows-and-views][req:Box display]") {
  using hefesto::display::BoxShape;
  using hefesto::shell::box_overlay;
  using hefesto::shell::PixelRect;
  // Thick: four 2-logical-pixel sides inside, whatever the HiDPI factor
  // (regression: on Retina they were 2 device pixels, a 1-logical-pixel outline).
  const auto thick = box_overlay(PixelRect{10, 10, 50, 40}, BoxShape::thick, 200, 200);
  REQUIRE(thick.size() == 4);
  CHECK(thick[0].x == 10.0);
  CHECK(thick[0].w == 2.0);
  CHECK(thick[1].x == 58.0);  // east side inside: 10 + 50 - 2
  // A side outside the canvas is not drawn.
  CHECK(box_overlay(PixelRect{-20, 10, 50, 40}, BoxShape::thick, 200, 200).size() == 3);
  // A horizontal line box keeps its two (thick) long sides only.
  CHECK(box_overlay(PixelRect{10, 10, 50, 0}, BoxShape::thick, 200, 200).size() == 2);
  // Thin: a 1-device-pixel outline.
  const auto thin = box_overlay(PixelRect{10, 10, 2, 40}, BoxShape::outline, 200, 200);
  REQUIRE(thin.size() == 4);
  CHECK(thin[0].w == 1.0);
  // A point: a cross plus a 3x3 square (2 + 4 rectangles).
  const auto point = box_overlay(PixelRect{10, 10, 0, 0}, BoxShape::point, 200, 200);
  REQUIRE(point.size() == 6);
  CHECK(point[0].w == 11.0);  // the cross spans ±5 logical pixels
}

TEST_CASE("a newly shown root is framed like the core's load frame",
          "[cap:ui-windows-and-views][req:Loading a cell into a window]") {
  // Regression: `load` of another cell kept the previous cell's camera while
  // the core mapping moved to the new cell's load frame, so the next core
  // zoom jumped. The canvas now frames a new root with frame_root().
  db::CellDef a("a", 4);
  a.set_bboxes(db::Rect{0, 0, 40, 20}, db::Rect{0, 0, 40, 20});
  db::CellDef b("b", 4);
  b.set_bboxes(db::Rect{-500, 100, 2500, 900}, db::Rect{-500, 100, 2500, 900});
  view::LayoutWindow win(1, &a);
  render::Camera camera;
  camera.set_viewport(800, 600);
  win.set_mapping(mapping_from_camera(camera));  // the canvas keeps the core screen = its viewport
  win.set_root(&b);
  render::Camera core_view;
  core_view.set_viewport(800, 600);
  camera_from_mapping(win.mapping(), core_view);
  frame_root(b, camera);
  CHECK(camera.pixels_per_unit() == Approx(core_view.pixels_per_unit()));
  CHECK(camera.center_x() == Approx(core_view.center_x()).margin(1.0));
  CHECK(camera.center_y() == Approx(core_view.center_y()).margin(1.0));

  // A cell not found on disk frames (0,0)-(1,1) with Magic's 60-unit minimum.
  db::CellDef missing("missing", 4);
  missing.flags().not_found = true;
  frame_root(missing, camera);
  const render::WorldWindow w = camera.visible_window();
  CHECK(w.x1 - w.x0 >= 60.0);
  CHECK(w.y1 - w.y0 >= 60.0);
}
