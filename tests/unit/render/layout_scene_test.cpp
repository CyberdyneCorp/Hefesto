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
  // layout fill, the box's four thick sides (one layer each), selection
  // lines, triangle fill (text draws nothing).
  REQUIRE(batches.kinds.size() == 7);
  CHECK(batches.kinds[0] == render::BatchKind::fill);
  CHECK(batches.kinds[1] == render::BatchKind::highlight);
  for (std::size_t i = 1; i <= 4; ++i) CHECK(batches.style_index[i] == 1);
  CHECK(batches.style_index[5] == 6);
  CHECK(batches.style_index[6] == 3);
  CHECK(batches.styles[5].r == 1.0F);  // white
  CHECK(batches.styles[5].a == 1.0F);
  REQUIRE(batches.rects.batches().size() == 6);
  for (std::size_t i = 1; i <= 4; ++i) CHECK(batches.rects.batches()[i].count == 1);  // one side each
  CHECK(batches.rects.batches()[5].count == 1);  // one selection edge
  CHECK(batches.polygons.batches.size() == 1);

  // The same highlights alone (a separate pass over a retained layout).
  const auto alone = render::highlight_batches(hl);
  CHECK(alone.kinds.size() == 6);
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

// --- pixel-width highlight outlines (M4 CARRY, task 11.6) ----------------------------

TEST_CASE("highlight outlines are one logical pixel wide at any pixel ratio",
          "[cap:hefesto-rendering][req:High-DPI and color management]") {
  display::HighlightScene hl;
  hl.items.push_back(highlight(display::HighlightShape::outline, 6, {0, 0, 10, 10}));  // a selection outline
  display::HighlightItem edge = highlight(display::HighlightShape::line, 6, {});
  edge.line = display::SceneSegment{{0, 0}, {10, 0}};
  hl.items.push_back(edge);
  const auto standard = render::highlight_batches(hl);
  REQUIRE(standard.styles.size() == 1);
  CHECK(standard.styles[0].line_width == 1);
  CHECK(standard.styles[0].line_shift_x == 0);
  CHECK(standard.styles[0].line_shift_y == 0);
  const auto retina = render::highlight_batches(hl, render::HighlightOptions{2.0});
  REQUIRE(retina.styles.size() == 1);
  CHECK(retina.styles[0].line_width == 2);  // two framebuffer pixels = one logical pixel
  CHECK(retina.rects.batches()[0].count == 5);
  CHECK(render::highlight_line_width(render::HighlightOptions{0.5}) == 1);
  CHECK(render::highlight_line_width(render::HighlightOptions{1.5}) == 2);
  CHECK(render::highlight_line_width(render::HighlightOptions{500.0}) == 63);
}

TEST_CASE("the thick box draws two-pixel sides inside the box",
          "[cap:ui-windows-and-views][req:Box display]") {
  display::HighlightScene hl;
  hl.items.push_back(highlight(display::HighlightShape::outline, display::kStyleBox, {0, 0, 40, 20}));
  const auto batches = render::highlight_batches(hl, render::HighlightOptions{2.0});
  REQUIRE(batches.styles.size() == 4);
  const auto& rects = batches.rects.instances();
  REQUIRE(rects.size() == 4);
  // left, right, bottom, top; each 4 framebuffer pixels (2 logical) wide,
  // moved inward by half its width (4 half pixels).
  CHECK(rects[0] == render::RectInstance{0, 0, 0, 20});
  CHECK(rects[1] == render::RectInstance{40, 0, 40, 20});
  CHECK(rects[2] == render::RectInstance{0, 0, 40, 0});
  CHECK(rects[3] == render::RectInstance{0, 20, 40, 20});
  for (const auto& style : batches.styles) CHECK(style.line_width == 4);
  CHECK(batches.styles[0].line_shift_x == 4);
  CHECK(batches.styles[0].line_shift_y == 0);
  CHECK(batches.styles[1].line_shift_x == -4);
  CHECK(batches.styles[2].line_shift_y == 4);
  CHECK(batches.styles[3].line_shift_y == -4);

  // Outside the edit cell (medium highlights) the box is thick too; a thin
  // box (outline highlights, under 4 pixels) is a plain one-pixel outline.
  display::HighlightScene medium;
  medium.items.push_back(highlight(display::HighlightShape::outline, display::kStyleMediumHighlights, {0, 0, 4, 4}));
  CHECK(render::highlight_batches(medium).styles.size() == 4);
  display::HighlightScene thin;
  thin.items.push_back(highlight(display::HighlightShape::outline, display::kStyleOutlineHighlights, {0, 0, 1, 4}));
  const auto plain = render::highlight_batches(thin);
  REQUIRE(plain.styles.size() == 1);
  CHECK(plain.styles[0].line_width == 1);
  CHECK(plain.rects.batches()[0].count == 4);
}

TEST_CASE("the thick box keeps its place in the client order",
          "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  display::HighlightScene hl;
  hl.items.push_back(highlight(display::HighlightShape::outline, 6, {0, 0, 2, 2}));
  hl.items.push_back(highlight(display::HighlightShape::outline, display::kStyleBox, {0, 0, 10, 10}));
  hl.items.push_back(highlight(display::HighlightShape::outline, 6, {5, 5, 8, 8}));
  const auto batches = render::highlight_batches(hl);
  REQUIRE(batches.style_index.size() == 6);
  CHECK(batches.style_index[0] == 6);
  for (std::size_t i = 1; i <= 4; ++i) CHECK(batches.style_index[i] == display::kStyleBox);
  CHECK(batches.style_index[5] == 6);
}
