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

TEST_CASE("outline-only and cross styles draw no fill", "[cap:display-style-files][req:Outline and fill rendering semantics]") {
  display::LayoutScene scene;
  display::SceneLayer outline{style(52, display::FillKind::outline, 1.0F), false, {{0, 0, 4, 4}}, {}, {}};
  outline.style.outline = 0xFF;
  outline.outline = display::boundary_segments(outline.rects);
  display::SceneLayer cross{style(53, display::FillKind::cross, 1.0F), false, {{0, 0, 4, 4}}, {}, {}};
  scene.layers = {outline, cross};
  const auto batches = render::to_batches(scene);
  CHECK(batches.kinds == std::vector<render::BatchKind>{render::BatchKind::outline});
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
