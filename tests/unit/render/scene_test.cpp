// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "hefesto/render/scene.hpp"

using hefesto::render::LayerBatch;
using hefesto::render::LayeredRect;
using hefesto::render::RectInstance;
using hefesto::render::RectScene;

TEST_CASE("scene groups rectangles by layer, stable within a layer",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  const std::vector<LayeredRect> rects{
      {{0, 0, 1, 1}, 3}, {{1, 1, 2, 2}, 0}, {{2, 2, 3, 3}, 3}, {{-5, 4, 9, 7}, 1}, {{3, 3, 4, 4}, 0},
  };
  const RectScene scene = RectScene::from_layered(rects);
  REQUIRE(scene.size() == 5);
  const std::vector<LayerBatch> expected{{0, 0, 2}, {1, 2, 1}, {3, 3, 2}};
  CHECK(std::vector<LayerBatch>(scene.batches().begin(), scene.batches().end()) == expected);
  CHECK(scene.instances()[0] == RectInstance{1, 1, 2, 2});
  CHECK(scene.instances()[1] == RectInstance{3, 3, 4, 4});
  CHECK(scene.instances()[3] == RectInstance{0, 0, 1, 1});
  CHECK(scene.instances()[4] == RectInstance{2, 2, 3, 3});
  CHECK(scene.bounds().x0 == -5);
  CHECK(scene.bounds().y1 == 7);
  CHECK(scene.byte_size() == 5 * sizeof(RectInstance));
}

TEST_CASE("empty scene has no batches and empty bounds", "[cap:hefesto-performance][req:Interactive frame rate]") {
  const RectScene scene = RectScene::from_layered({});
  CHECK(scene.batches().empty());
  CHECK(scene.bounds().empty());
}
