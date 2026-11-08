// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "hefesto/render/canvas_index.hpp"
#include "hefesto/render/synthetic_layout.hpp"

using hefesto::render::CanvasIndex;
using hefesto::render::LayerBatch;
using hefesto::render::LayeredRect;
using hefesto::render::RectInstance;
using hefesto::render::SplitMix64;
using hefesto::render::WorldWindow;

namespace {

bool intersects(const RectInstance& r, const WorldWindow& w) {
  return r.x1 >= w.x0 && r.x0 <= w.x1 && r.y1 >= w.y0 && r.y0 <= w.y1;
}

/// Multiplicity of each rectangle in the drawn ranges.
std::size_t drawn_count(const CanvasIndex& index, const std::vector<LayerBatch>& ranges, const LayeredRect& lr) {
  std::size_t n = 0;
  for (const LayerBatch& b : ranges) {
    if (b.layer != lr.layer) continue;
    const auto span = index.instances().subspan(b.first, b.count);
    n += static_cast<std::size_t>(std::count(span.begin(), span.end(), lr.rect));
  }
  return n;
}

std::vector<LayeredRect> sample_layout(std::uint64_t count) {
  hefesto::render::SyntheticLayoutParams p;
  p.count = count;
  p.die_size = 400'000;  // small die: shapes of every size class cross tiles
  return hefesto::render::generate_synthetic_layout(p);
}

}  // namespace

TEST_CASE("zoomed in, culling never drops a visible rectangle",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  const auto rects = sample_layout(3000);
  const CanvasIndex index = CanvasIndex::build(rects, {16, 256, 6});
  REQUIRE(index.rect_count() == rects.size());
  SplitMix64 rng(7);
  std::vector<LayerBatch> ranges;
  for (int trial = 0; trial < 20; ++trial) {
    const double x = rng.below(400'000);
    const double y = rng.below(400'000);
    const double half = 1000.0 + rng.below(40'000);
    const WorldWindow window{x - half, y - half, x + half, y + half};
    index.visible_ranges(window, 1.0, ranges);  // 1 unit per pixel: no LOD
    REQUIRE(index.lod_level_for(1.0) == -1);
    for (const LayeredRect& lr : rects) {
      if (intersects(lr.rect, window)) {
        REQUIRE(drawn_count(index, ranges, lr) >= 1);
      }
    }
  }
}

TEST_CASE("ranges are in layer order and inside the instance buffer",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  const auto rects = sample_layout(5000);
  const CanvasIndex index = CanvasIndex::build(rects);
  std::vector<LayerBatch> ranges;
  for (double upp : {1.0, 100.0, 1000.0, 50'000.0}) {
    index.visible_ranges({-1e9, -1e9, 1e9, 1e9}, upp, ranges);
    REQUIRE_FALSE(ranges.empty());
    for (std::size_t i = 0; i < ranges.size(); ++i) {
      CHECK(ranges[i].first + ranges[i].count <= index.instances().size());
      if (i > 0) {
        CHECK(ranges[i - 1].layer <= ranges[i].layer);
      }
    }
  }
}

TEST_CASE("level of detail draws cells instead of sub-pixel rectangles",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  const auto rects = sample_layout(20'000);
  const CanvasIndex index = CanvasIndex::build(rects, {16, 256, 6});
  CHECK(index.lod_level_for(64.0) == -1);
  CHECK(index.lod_level_for(65.0) == 0);
  CHECK(index.lod_level_for(1000.0) == 1);
  CHECK(index.lod_level_for(1e9) == 5);

  std::vector<LayerBatch> ranges;
  index.visible_ranges({-1e9, -1e9, 1e9, 1e9}, 1.0, ranges);
  std::size_t all = 0;
  for (const auto& r : ranges) all += r.count;
  CHECK(all == rects.size());  // no LOD: every rectangle, no cells

  // Whole die on ~8 px: only the few rectangles longer than a 64 um cell are
  // drawn one by one; everything else is represented by level-4 cells.
  index.visible_ranges({-1e9, -1e9, 1e9, 1e9}, 50'000.0, ranges);
  REQUIRE(index.lod_level_for(50'000.0) == 4);
  std::size_t lod = 0;
  for (const auto& r : ranges) lod += r.count;
  CHECK(lod > 0);
  CHECK(lod < rects.size() / 4);
}

TEST_CASE("empty index draws nothing", "[cap:hefesto-performance][req:Interactive frame rate]") {
  const CanvasIndex index = CanvasIndex::build({});
  std::vector<LayerBatch> ranges{{0, 0, 1}};
  index.visible_ranges({0, 0, 1, 1}, 1.0, ranges);
  CHECK(ranges.empty());
}
