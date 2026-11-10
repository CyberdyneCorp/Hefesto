// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The bridge from display::LayoutScene to GPU batches (capability
// hefesto-rendering, display-style-files).

#include <catch2/catch_test_macros.hpp>

#include "hefesto/render/layout_scene.hpp"

namespace display = hefesto::display;
namespace render = hefesto::render;

namespace {

display::ResolvedStyle style(std::size_t index, display::FillKind fill, float r) {
  display::ResolvedStyle s;
  s.index = index;
  s.fill = fill;
  s.r = r;
  s.a = 0.5F;
  return s;
}

}  // namespace

TEST_CASE("scene layers become batches in redisplay order with PDK colors",
          "[cap:hefesto-rendering][req:Style-file-faithful 2D drawing]") {
  display::LayoutScene scene;
  display::SceneLayer poly{style(52, display::FillKind::solid, 1.0F), false, {{0, 0, 10, 10}}, {}, {}};
  display::SceneLayer metal{style(53, display::FillKind::stipple, 0.25F), false, {{5, 5, 20, 20}, {30, 30, 40, 40}}, {}, {}};
  metal.style.stipple = display::Stipple{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
  metal.style.outline = 0xFF;
  metal.outline = display::boundary_segments(metal.rects);
  scene.layers = {poly, metal};

  const auto batches = render::to_batches(scene);
  REQUIRE(batches.styles.size() == 3);  // poly fill, metal fill, metal outline
  CHECK(batches.kinds == std::vector<render::BatchKind>{render::BatchKind::fill, render::BatchKind::fill,
                                                         render::BatchKind::outline});
  CHECK(batches.styles[0].r == 1.0F);
  CHECK(batches.styles[0].a == 0.5F);
  CHECK_FALSE(batches.styles[0].stippled);
  CHECK(batches.styles[1].stippled);
  CHECK(batches.styles[1].r == 0.0F);  // Q-UI-35: 2 * 0.25 - 0.8, clamped
  CHECK(batches.styles[1].stipple == metal.style.stipple);
  CHECK(batches.styles[2].a == 1.0F);
  REQUIRE(batches.rects.batches().size() == 3);
  CHECK(batches.rects.batches()[1].count == 2);
  CHECK(batches.rects.batches()[2].count == 8);  // two separate squares: 4 sides each
}

TEST_CASE("outline-only styles draw no fill; cross styles draw diagonals in their line pattern",
          "[cap:display-style-files][req:Outline and fill rendering semantics]") {
  display::LayoutScene scene;
  display::SceneLayer outline{style(52, display::FillKind::outline, 1.0F), false, {{0, 0, 4, 4}}, {}, {}};
  outline.style.outline = 0xCC;
  outline.outline = display::boundary_segments(outline.rects);
  display::SceneLayer cross{style(53, display::FillKind::cross, 1.0F), false, {{0, 0, 4, 4}, {8, 8, 12, 12}}, {}, {}};
  cross.style.outline = 0xF0;
  scene.layers = {outline, cross};
  const auto batches = render::to_batches(scene);
  CHECK(batches.kinds == std::vector<render::BatchKind>{render::BatchKind::outline, render::BatchKind::diagonals});
  CHECK(batches.styles[0].dash == 0xCC);
  CHECK_FALSE(batches.styles[0].diagonals);
  CHECK(batches.styles[1].diagonals);
  CHECK(batches.styles[1].dash == 0xF0);
  CHECK(batches.styles[1].a == 1.0F);  // lines are opaque
  REQUIRE(batches.rects.batches().size() == 2);
  CHECK(batches.rects.batches()[1].count == 2);  // one instance per rectangle
}

TEST_CASE("split tiles become triangles in their layer's fill style",
          "[cap:hefesto-rendering][req:Style-file-faithful 2D drawing]") {
  display::LayoutScene scene;
  display::SceneLayer poly{style(52, display::FillKind::stipple, 1.0F), false, {{0, 0, 10, 10}}, {}, {}};
  poly.triangles.push_back({{10, 0, 20, 10}, hefesto::db::Diagonal::nw_se, false});
  poly.triangles.push_back({{20, 0, 30, 10}, hefesto::db::Diagonal::ne_sw, true});
  scene.layers = {poly};
  scene.bounds = {0, 0, 30, 10};
  const auto batches = render::to_batches(scene);
  CHECK(batches.kinds == std::vector<render::BatchKind>{render::BatchKind::fill, render::BatchKind::triangles});
  CHECK(batches.styles[1].stippled);
  REQUIRE(batches.polygons.batches.size() == 1);
  CHECK(batches.polygons.batches[0].layer == 1);
  CHECK(batches.polygons.batches[0].count == 6);
  const auto& v = batches.polygons.vertices;
  // `\` left part: the lower-left triangle (10,0) (10,10) (20,0).
  CHECK(v[0].x == 10.0F);
  CHECK(v[0].y == 0.0F);
  CHECK(v[1].x == 10.0F);
  CHECK(v[1].y == 10.0F);
  CHECK(v[2].x == 20.0F);
  CHECK(v[2].y == 0.0F);
  // `/` right part: the lower-right triangle (30,0) (20,0) (30,10).
  CHECK(v[3].x == 30.0F);
  CHECK(v[3].y == 0.0F);
}

TEST_CASE("font label glyphs are filled and skipped under 5 pixels of text height",
          "[cap:font-labels][req:Font label rendering]") {
  display::LayoutScene scene;
  display::SceneLabel label;
  label.rect = {0, 0, 0, 0};
  label.style = style(display::kStyleLabels, display::FillKind::solid, 1.0F);
  label.font_label = true;
  label.size = 16;  // 2 units: the text is 5 pixels high from 2.5 pixels per unit
  label.outline_polygons = {{{0, 0}, {4, 0}, {4, 4}, {0, 4}}};
  scene.labels.push_back(label);
  const auto batches = render::to_batches(scene);
  CHECK(batches.kinds == std::vector<render::BatchKind>{render::BatchKind::label, render::BatchKind::glyphs});
  REQUIRE(batches.polygons.batches.size() == 1);
  CHECK(batches.polygons.batches[0].count == 6);  // one slab, one trapezoid
  CHECK(batches.polygons.min_pixels_per_unit[0] == 2.5);
  std::vector<render::LayerBatch> visible;
  render::visible_polygon_batches(batches.polygons, 2.49, visible);
  CHECK(visible.empty());
  render::visible_polygon_batches(batches.polygons, 2.5, visible);
  CHECK(visible.size() == 1);
}

TEST_CASE("labels, port lines and cell boxes are grouped per style", "[cap:ui-windows-and-views][req:Label display]") {
  display::LayoutScene scene;
  for (int i = 0; i < 300; ++i) {
    display::SceneLabel label;
    label.rect = {i, 0, i + 1, 1};
    label.style = style(display::kStyleLabels, display::FillKind::solid, 1.0F);
    if (i == 0) {
      label.port_lines.push_back({{0, 1}, {1, 1}});
      label.port_line_style = style(display::kStylePortConnection, display::FillKind::solid, 0.0F);
    }
    scene.labels.push_back(label);
  }
  display::SceneCell cell;
  cell.bbox = {0, 0, 100, 50};
  cell.style = style(display::kStyleCellBox, display::FillKind::solid, 1.0F);
  scene.cells.push_back(cell);
  const auto batches = render::to_batches(scene);
  CHECK_FALSE(batches.truncated);
  CHECK(batches.kinds == std::vector<render::BatchKind>{render::BatchKind::label, render::BatchKind::port_line,
                                                         render::BatchKind::cell_box});
  CHECK(batches.style_index[2] == display::kStyleCellBox);
  CHECK(batches.rects.batches()[0].count == 1200);
}

namespace {

display::HighlightItem highlight(display::HighlightShape shape, std::size_t index, const hefesto::db::Rect& r) {
  display::HighlightItem it;
  it.shape = shape;
  it.style = index;
  it.rect = r;
  display::resolve_highlight(it, nullptr);  // no display styles: opaque white, solid outline
  return it;
}

}  // namespace

TEST_CASE("highlights become batches drawn after the layout, in client order",
          "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  display::LayoutScene scene;
  scene.layers = {display::SceneLayer{style(52, display::FillKind::solid, 1.0F), false, {{0, 0, 10, 10}}, {}, {}}};
  scene.bounds = {0, 0, 10, 10};
  display::HighlightScene hl;
  hl.items.push_back(highlight(display::HighlightShape::outline, 1, {0, 0, 4, 4}));  // the box
  display::HighlightItem selection = highlight(display::HighlightShape::line, 6, {});
  selection.line = display::SceneSegment{{2, 2}, {8, 2}};
  hl.items.push_back(selection);
  hl.items.push_back(highlight(display::HighlightShape::text, 6, {2, 2, 8, 8}));  // text: the front end's
  hl.items.push_back(highlight(display::HighlightShape::triangle, 3, {0, 0, 10, 10}));

  const auto batches = render::to_batches(scene, &hl);
  // layout fill, box lines, selection lines, triangle fill (text draws nothing).
  REQUIRE(batches.kinds.size() == 4);
  CHECK(batches.kinds[0] == render::BatchKind::fill);
  CHECK(batches.kinds[1] == render::BatchKind::highlight);
  CHECK(batches.style_index[1] == 1);
  CHECK(batches.style_index[2] == 6);
  CHECK(batches.style_index[3] == 3);
  CHECK(batches.styles[2].r == 1.0F);  // white
  CHECK(batches.styles[2].a == 1.0F);
  REQUIRE(batches.rects.batches().size() == 3);
  CHECK(batches.rects.batches()[1].count == 4);  // the box's four sides
  CHECK(batches.rects.batches()[2].count == 1);  // one selection edge
  CHECK(batches.polygons.batches.size() == 1);

  // The same highlights alone (a separate pass over a retained layout).
  const auto alone = render::highlight_batches(hl);
  CHECK(alone.kinds.size() == 3);
  CHECK(alone.rects.batches().front().layer == 0);
}

TEST_CASE("highlight boxes draw the style's fill and outline; crosses draw their point",
          "[cap:ui-feedback-and-highlights][req:Feedback display]") {
  display::HighlightScene hl;
  display::HighlightItem pale = highlight(display::HighlightShape::box, 3, {0, 0, 10, 10});
  pale.resolved.fill = display::FillKind::stipple;
  pale.resolved.outline = 0;
  hl.items.push_back(pale);
  hl.items.push_back(highlight(display::HighlightShape::cross, 6, {5, 5, 5, 5}));
  const auto batches = render::highlight_batches(hl);
  REQUIRE(batches.kinds.size() == 2);  // pale fill only (no outline), the cross's point
  CHECK(batches.styles[0].stippled);
  REQUIRE(batches.rects.batches().size() == 2);
  CHECK(batches.rects.instances()[1] == render::RectInstance{5, 5, 5, 5});
}
