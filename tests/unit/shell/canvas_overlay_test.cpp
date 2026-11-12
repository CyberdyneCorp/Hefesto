// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Geometry of the canvas overlay (rulers, minimap, zoom control) and the
// mapping between the core window's ViewMapping and the renderer camera
// (design.md D5).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <utility>
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

namespace {

display::SceneLayer scene_layer(float r, float g, float b, display::FillKind fill, std::vector<db::Rect> rects) {
  display::SceneLayer layer;
  layer.style.r = r;
  layer.style.g = g;
  layer.style.b = b;
  layer.style.a = 1.0F;
  layer.style.fill = fill;
  layer.rects = std::move(rects);
  return layer;
}

/// The thumbnail's color at minimap pixel (x, y); nullopt where nothing is painted.
std::optional<Rgba> thumbnail_at(const std::vector<MinimapFill>& fills, double x, double y) {
  for (const MinimapFill& f : fills) {
    if (f.rect.contains(x, y)) return f.color;
  }
  return std::nullopt;
}

}  // namespace

// Regression (UI review): the minimap showed a grey rectangle and no layout.
TEST_CASE("the minimap shows a thumbnail of the layout in the PDK colors",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  const db::Rect bounds{0, 0, 200, 100};
  display::LayoutScene scene;
  // Red on the left half, blue over the top-right quarter, an outline-only layer everywhere.
  scene.layers.push_back(scene_layer(1.0F, 0.0F, 0.0F, display::FillKind::solid, {{0, 0, 100, 100}}));
  scene.layers.push_back(scene_layer(0.0F, 0.0F, 1.0F, display::FillKind::solid, {{100, 50, 200, 100}}));
  scene.layers.push_back(scene_layer(0.0F, 1.0F, 0.0F, display::FillKind::outline, {{0, 0, 200, 100}}));
  const Rgba black{0.0F, 0.0F, 0.0F, 1.0F};
  const auto fills = minimap_thumbnail(scene, bounds, black, kMinimapWidth, kMinimapHeight);
  REQUIRE_FALSE(fills.empty());
  const auto g = minimap_geometry(bounds, render::WorldWindow{}, kMinimapWidth, kMinimapHeight);
  REQUIRE(g.has_value());
  const double qx = g->content.w / 4.0;
  const double qy = g->content.h / 4.0;
  const auto left = thumbnail_at(fills, g->content.x + qx, g->content.y + 2.0 * qy);
  const auto top_right = thumbnail_at(fills, g->content.x + 3.0 * qx, g->content.y + qy);
  const auto bottom_right = thumbnail_at(fills, g->content.x + 3.0 * qx, g->content.y + 3.0 * qy);
  REQUIRE(left.has_value());
  CHECK(left->r == Approx(1.0F));
  CHECK(left->b == Approx(0.0F));
  REQUIRE(top_right.has_value());  // y up in the world: the top quarter is drawn at the top
  CHECK(top_right->b == Approx(1.0F));
  CHECK_FALSE(bottom_right.has_value());  // only the outline layer there: not drawn
  for (const MinimapFill& f : fills) {
    CHECK(f.rect.x >= g->content.x - 1e-9);
    CHECK(f.rect.x + f.rect.w <= g->content.x + g->content.w + 1e-9);
    CHECK(f.rect.y + f.rect.h <= g->content.y + g->content.h + 1e-9);
  }
  // Runs of one color are merged: a solid half is a few fills per row, not one per cell.
  CHECK(fills.size() < static_cast<std::size_t>(2.0 * g->content.h));  // at most ~4 runs per 2 px row

  // Translucent and stippled layers blend over what is below them.
  display::LayoutScene glass;
  glass.layers.push_back(scene_layer(1.0F, 1.0F, 1.0F, display::FillKind::stipple, {{0, 0, 200, 100}}));
  const auto half = minimap_thumbnail(glass, bounds, black, kMinimapWidth, kMinimapHeight);
  REQUIRE_FALSE(half.empty());
  CHECK(half.front().color.r == Approx(0.5F));
  CHECK(half.front().color.a == Approx(1.0F));

  CHECK(minimap_thumbnail(display::LayoutScene{}, bounds, black, kMinimapWidth, kMinimapHeight).empty());
  CHECK(minimap_thumbnail(scene, db::Rect{0, 0, -1, -1}, black, kMinimapWidth, kMinimapHeight).empty());
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

TEST_CASE("the DRC tag sits below right of the found tile and stays inside the canvas",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  // DRC-Browser board: tile 208..236 x 104..224, tag at (250, 236).
  const PixelRect tile{208.0, 104.0, 28.0, 120.0};
  const PixelPoint p = drc_tag_origin(tile, 300.0, 28.0, 1000.0, 700.0);
  CHECK(p.x == Approx(250.0));
  CHECK(p.y == Approx(236.0));
  // No room on the right: left of the tile.
  const PixelPoint left = drc_tag_origin(PixelRect{900.0, 104.0, 28.0, 20.0}, 300.0, 28.0, 1000.0, 700.0);
  CHECK(left.x == Approx(900.0 - kDrcTagGapX - 300.0));
  // No room below: above the tile.
  const PixelPoint above = drc_tag_origin(PixelRect{208.0, 650.0, 28.0, 30.0}, 300.0, 28.0, 1000.0, 700.0);
  CHECK(above.y == Approx(650.0 - kDrcTagGapY - 28.0));
  // A tile off the canvas: the tag is clamped inside (rulers and insets kept clear).
  const PixelPoint off = drc_tag_origin(PixelRect{-500.0, -500.0, 10.0, 10.0}, 300.0, 28.0, 1000.0, 700.0);
  CHECK(off.x == Approx(static_cast<double>(kRulerSize + kCanvasInset)));
  CHECK(off.y == Approx(static_cast<double>(kRulerSize + kCanvasInset)));
  const PixelPoint far = drc_tag_origin(PixelRect{5000.0, 5000.0, 10.0, 10.0}, 300.0, 28.0, 1000.0, 700.0);
  CHECK(far.x == Approx(1000.0 - kCanvasInset - 300.0));
  CHECK(far.y == Approx(700.0 - kCanvasInset - 28.0));
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
  // Regression (UI review): zero was ".0" and "-0.1" was stacked one
  // character per line ("-", ".", "1"), clipped in the 20 px ruler.
  CHECK(compact_ruler_label("0.0") == "0");
  CHECK(compact_ruler_label("-0.00") == "0");
  CHECK(compact_ruler_label("-0.1") == "-.1");
  // JetBrains Mono 10 (6 px advance) fits three characters in the 20 px ruler.
  const std::size_t three = ruler_label_max_chars(20.0, 6.0);
  CHECK(three == 3);
  CHECK(ruler_label_max_chars(20.0, 0.0) == 1);
  CHECK(ruler_label_max_chars(4.0, 6.0) == 1);
  for (const char* label : {"-0.1", "0.1", "0.0", "2.7", "-0.5", "-2", "100"}) {
    CHECK(ruler_label_lines(compact_ruler_label(label), three).size() == 1);
  }
  // A label that does not fit puts its sign on its own line, then the rest.
  CHECK(ruler_label_lines("0", 2) == std::vector<std::string>{"0"});
  CHECK(ruler_label_lines("-500", 3) == std::vector<std::string>{"-", "500"});
  CHECK(ruler_label_lines("-1.5k", 3) == std::vector<std::string>{"-", "1.5", "k"});
  CHECK(ruler_label_lines("1.25", 3) == std::vector<std::string>{"1.2", "5"});
  CHECK(ruler_label_lines("ab", 0) == std::vector<std::string>{"a", "b"});
  for (const char* label : {"-500", "1.5k", "-.25", "12345"}) {
    for (std::size_t width : {std::size_t{1}, std::size_t{2}, three}) {
      for (const std::string& line : ruler_label_lines(compact_ruler_label(label), width)) CHECK(line.size() <= width);
    }
  }
}

TEST_CASE("label text is placed like Magic's GrPutText", "[cap:ui-windows-and-views][req:Label display]") {
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

TEST_CASE("font-label glyphs fill with the nonzero rule", "[cap:ui-windows-and-views][req:Label display]") {
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

TEST_CASE("the minimap draws outline-only and cross styles, labels and cell boxes",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  const db::Rect bounds{0, 0, 200, 100};
  const Rgba black{0.0F, 0.0F, 0.0F, 1.0F};
  display::LayoutScene scene;
  scene.layers.push_back(scene_layer(0.0F, 1.0F, 0.0F, display::FillKind::outline, {{0, 0, 200, 100}}));
  scene.layers.push_back(scene_layer(0.0F, 0.0F, 1.0F, display::FillKind::cross, {{150, 10, 190, 40}}));
  display::SceneCell cell;
  cell.bbox = db::Rect{20, 20, 80, 80};
  cell.style.r = cell.style.g = cell.style.b = 1.0F;
  cell.style.a = 1.0F;
  scene.cells.push_back(cell);
  display::SceneLabel label;
  label.rect = db::Rect{120, 70, 120, 70};
  label.style.r = 1.0F;
  label.style.a = 1.0F;
  scene.labels.push_back(label);
  const auto fills = minimap_thumbnail(scene, bounds, black, kMinimapWidth, kMinimapHeight);
  const auto g = minimap_geometry(bounds, render::WorldWindow{}, kMinimapWidth, kMinimapHeight);
  REQUIRE(g.has_value());
  const double s = g->content.w / 200.0;  // pixels per unit
  const auto at = [&](double wx, double wy) { return thumbnail_at(fills, g->content.x + wx * s, g->content.y + (100.0 - wy) * s); };
  // The well's edge shows, its inside does not.
  REQUIRE(at(1.0, 50.0).has_value());
  CHECK(at(1.0, 50.0)->g > 0.5F);
  CHECK_FALSE(at(100.0, 10.0).has_value());
  // The contact's cross fills its rectangle at half strength.
  REQUIRE(at(170.0, 25.0).has_value());
  CHECK(at(170.0, 25.0)->b > 0.2F);
  // The cell box outline, not its inside.
  REQUIRE(at(20.5, 50.0).has_value());
  CHECK_FALSE(at(50.0, 50.0).has_value());
  // The label point.
  REQUIRE(at(120.0, 70.0).has_value());
  CHECK(at(120.0, 70.0)->r > 0.8F);
}

TEST_CASE("cell box captions sit on the thirds of the box like Magic's dbwBBoxFunc",
          "[cap:cmd-selection][req:Selection model]") {
  // Regression (Desktop Alpha): the selected top cell's name and id overlapped.
  const PixelRect r{100.0, 60.0, 300.0, 90.0};
  const auto name = cell_caption_center(r, 400.0, CellCaption::name, 28.0);
  const auto id = cell_caption_center(r, 400.0, CellCaption::id, 28.0);
  REQUIRE(name);
  REQUIRE(id);
  CHECK(name->x == Approx(250.0));
  CHECK(name->y == Approx(90.0));   // 1/3 down (Magic: 2/3 up)
  CHECK(id->y == Approx(120.0));    // 2/3 down
  CHECK(id->y - name->y >= 20.0);   // apart by more than a caption line
  // The id's line is taken from the box clipped to the window; the name's is not.
  const PixelRect tall{0.0, -300.0, 200.0, 900.0};
  CHECK(cell_caption_center(tall, 300.0, CellCaption::name, 28.0)->y == Approx(0.0));
  CHECK(cell_caption_center(tall, 300.0, CellCaption::id, 28.0)->y == Approx(200.0));
  CHECK_FALSE(cell_caption_center(PixelRect{0.0, 0.0, 100.0, 20.0}, 300.0, CellCaption::name, 28.0));
}
