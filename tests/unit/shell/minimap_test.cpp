// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The minimap's per-area thumbnail update (MinimapThumbnail, minimap.hpp;
// M4 CARRY task 11.6): a full rebuild equals minimap_thumbnail(), and an
// update from a partial scene over the dirty cells equals a full rebuild of
// the edited scene while compositing only those cells.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <random>
#include <vector>

#include "hefesto/shell/canvas_overlay.hpp"
#include "hefesto/shell/design_tokens.hpp"
#include "hefesto/shell/minimap.hpp"

using namespace hefesto::shell;
namespace db = hefesto::db;
namespace display = hefesto::display;

namespace {

display::SceneLayer layer(float r, float g, float b, display::FillKind fill, std::vector<db::Rect> rects) {
  display::SceneLayer l;
  l.style.r = r;
  l.style.g = g;
  l.style.b = b;
  l.style.a = 0.6F;
  l.style.fill = fill;
  l.rects = std::move(rects);
  return l;
}

/// A scene of `n` random rectangles over `bounds` on three layers (solid,
/// stipple, outline-only), plus labels and cell boxes.
display::LayoutScene random_scene(const db::Rect& bounds, int n, std::uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<db::Coord> x(bounds.x0, bounds.x1 - 1);
  std::uniform_int_distribution<db::Coord> y(bounds.y0, bounds.y1 - 1);
  std::uniform_int_distribution<db::Coord> size(0, 400);
  display::LayoutScene scene;
  scene.layers = {layer(1.0F, 0.2F, 0.2F, display::FillKind::solid, {}),
                  layer(0.2F, 0.2F, 1.0F, display::FillKind::stipple, {}),
                  layer(0.2F, 1.0F, 0.2F, display::FillKind::outline, {})};
  for (int i = 0; i < n; ++i) {
    const db::Coord x0 = x(rng);
    const db::Coord y0 = y(rng);
    const db::Rect r{x0, y0, std::min(bounds.x1, x0 + size(rng)), std::min(bounds.y1, y0 + size(rng))};
    scene.layers[static_cast<std::size_t>(i % 3)].rects.push_back(r);
  }
  for (int i = 0; i < 20; ++i) {
    display::SceneLabel label;
    const db::Coord lx = x(rng);
    const db::Coord ly = y(rng);
    label.rect = db::Rect{lx, ly, lx, ly};
    label.style.r = 1.0F;
    scene.labels.push_back(label);
  }
  display::SceneCell cell;
  cell.bbox = db::Rect{bounds.x0 + 100, bounds.y0 + 100, bounds.x0 + 3000, bounds.y0 + 2000};
  scene.cells.push_back(cell);
  return scene;
}

bool touches(const db::Rect& a, const db::Rect& b) {
  return a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1;
}

/// What display::build_scene over `area` gives the canvas: every item that
/// touches it, whole.
display::LayoutScene partial_of(const display::LayoutScene& scene, const db::Rect& area) {
  display::LayoutScene partial;
  for (const display::SceneLayer& l : scene.layers) {
    display::SceneLayer copy = l;
    copy.rects.clear();
    for (const db::Rect& r : l.rects) {
      if (touches(r, area)) copy.rects.push_back(r);
    }
    partial.layers.push_back(std::move(copy));
  }
  for (const display::SceneLabel& label : scene.labels) {
    if (touches(label.rect, area)) partial.labels.push_back(label);
  }
  for (const display::SceneCell& cell : scene.cells) {
    if (touches(cell.bbox, area)) partial.cells.push_back(cell);
  }
  return partial;
}

bool same_fills(const std::vector<MinimapFill>& a, const std::vector<MinimapFill>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto& x = a[i];
    const auto& y = b[i];
    if (x.rect.x != y.rect.x || x.rect.y != y.rect.y || x.rect.w != y.rect.w || x.rect.h != y.rect.h ||
        x.color.r != y.color.r || x.color.g != y.color.g || x.color.b != y.color.b) {
      return false;
    }
  }
  return true;
}

const db::Rect kBounds{-5000, -2000, 15000, 8000};
const MinimapFrame kFrame{kBounds, Rgba{0.0F, 0.0F, 0.0F, 1.0F}, kMinimapWidth, kMinimapHeight, 2.0, 12.0};

}  // namespace

TEST_CASE("a rebuilt minimap thumbnail equals minimap_thumbnail", "[cap:hefesto-app-ux][req:Main window layout]") {
  const display::LayoutScene scene = random_scene(kBounds, 3000, 1);
  MinimapThumbnail thumb;
  thumb.rebuild(scene, kFrame);
  CHECK(thumb.built_for(kFrame));
  CHECK(same_fills(thumb.fills(), minimap_thumbnail(scene, kBounds, kFrame.background, kFrame.width, kFrame.height,
                                                    kFrame.cell_px, kFrame.padding)));
  MinimapFrame grown = kFrame;
  grown.bounds.x1 += 10;
  CHECK_FALSE(thumb.built_for(grown));  // the layout grew: rebuild

  MinimapThumbnail empty;
  empty.rebuild(scene, MinimapFrame{db::Rect{0, 0, -1, -1}, kFrame.background, 168.0, 112.0});
  CHECK(empty.fills().empty());
  CHECK(empty.dirty_cells(db::Rect{0, 0, 10, 10}).empty());
}

TEST_CASE("an edit updates only the minimap cells under its area and matches a rebuild",
          "[cap:hefesto-rendering][req:Large-layout performance]") {
  display::LayoutScene scene = random_scene(kBounds, 3000, 2);
  MinimapThumbnail thumb;
  thumb.rebuild(scene, kFrame);
  const std::size_t all_cells = thumb.last_composited();

  // Paint a rectangle (on each layer in turn), erase one, then add a label.
  const std::vector<db::Rect> edits{{100, 100, 900, 450}, {-5000, -2000, -4990, -1990}, {14000, 7990, 15000, 8000},
                                    {3000, 3000, 3000, 6000}};
  for (std::size_t i = 0; i < edits.size(); ++i) {
    INFO("edit " << i);
    const db::Rect& area = edits[i];
    scene.layers[i % 3].rects.push_back(area);
    const auto dirty = thumb.dirty_area(area);
    REQUIRE(dirty.has_value());
    thumb.update(partial_of(scene, *dirty), area);
    CHECK(thumb.last_composited() < all_cells / 10);
    MinimapThumbnail full;
    full.rebuild(scene, kFrame);
    CHECK(same_fills(thumb.fills(), full.fills()));
  }
  // Erasing: the rectangle leaves the scene; the cells it covered are redone.
  const db::Rect gone = scene.layers[0].rects.front();
  scene.layers[0].rects.erase(scene.layers[0].rects.begin());
  thumb.update(partial_of(scene, *thumb.dirty_area(gone)), gone);
  MinimapThumbnail full;
  full.rebuild(scene, kFrame);
  CHECK(same_fills(thumb.fills(), full.fills()));
  // An area outside the bounds changes nothing.
  CHECK_FALSE(thumb.dirty_area(db::Rect{20000, 20000, 20010, 20010}).has_value());
}

TEST_CASE("the dirty cells cover every cell an area's rectangles can paint",
          "[cap:hefesto-rendering][req:Large-layout performance]") {
  display::LayoutScene scene;
  scene.layers.push_back(layer(1.0F, 0.0F, 0.0F, display::FillKind::solid, {}));
  MinimapThumbnail thumb;
  thumb.rebuild(scene, kFrame);
  // Thin and point areas on and between cell boundaries, across the bounds.
  std::mt19937 rng(7);
  std::uniform_int_distribution<db::Coord> x(kBounds.x0, kBounds.x1);
  std::uniform_int_distribution<db::Coord> y(kBounds.y0, kBounds.y1);
  for (int i = 0; i < 500; ++i) {
    const db::Coord x0 = x(rng);
    const db::Coord y0 = y(rng);
    const db::Rect area{x0, y0, std::min(kBounds.x1, x0 + (i % 4)), std::min(kBounds.y1, y0 + (i % 3) * 7)};
    scene.layers[0].rects.push_back(area);
    thumb.update(partial_of(scene, *thumb.dirty_area(area)), area);
  }
  MinimapThumbnail full;
  full.rebuild(scene, kFrame);
  CHECK(same_fills(thumb.fills(), full.fills()));
}

TEST_CASE("a per-area minimap update is far cheaper than a rebuild on a large scene",
          "[cap:hefesto-rendering][req:Large-layout performance][.benchmark]") {
  const display::LayoutScene scene = random_scene(kBounds, 2'000'000, 3);
  MinimapThumbnail thumb;
  const auto t0 = std::chrono::steady_clock::now();
  thumb.rebuild(scene, kFrame);
  const auto t1 = std::chrono::steady_clock::now();
  const db::Rect area{100, 100, 900, 450};
  const auto partial = partial_of(scene, *thumb.dirty_area(area));
  const auto t2 = std::chrono::steady_clock::now();
  thumb.update(partial, area);
  const auto t3 = std::chrono::steady_clock::now();
  const auto ms = [](auto d) { return std::chrono::duration<double, std::milli>(d).count(); };
  WARN("rebuild " << ms(t1 - t0) << " ms, update " << ms(t3 - t2) << " ms (" << thumb.last_composited()
                  << " cells)");
  CHECK(ms(t3 - t2) < ms(t1 - t0));
}
