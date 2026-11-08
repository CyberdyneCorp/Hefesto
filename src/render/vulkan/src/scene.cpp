// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/scene.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace hefesto::render {

namespace {

constexpr std::size_t kLayerSlots = std::numeric_limits<std::uint8_t>::max() + 1U;

void extend(Bounds& bounds, const RectInstance& r) {
  if (!bounds.valid) {
    bounds = Bounds{r.x0, r.y0, r.x1, r.y1, true};
    return;
  }
  bounds.x0 = std::min<std::int64_t>(bounds.x0, r.x0);
  bounds.y0 = std::min<std::int64_t>(bounds.y0, r.y0);
  bounds.x1 = std::max<std::int64_t>(bounds.x1, r.x1);
  bounds.y1 = std::max<std::int64_t>(bounds.y1, r.y1);
}

}  // namespace

RectScene RectScene::from_layered(std::span<const LayeredRect> rects) {
  if (rects.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::length_error("RectScene: more than 2^32-1 rectangles");
  }

  std::array<std::uint32_t, kLayerSlots> counts{};
  for (const LayeredRect& lr : rects) {
    ++counts[lr.layer];
  }

  RectScene scene;
  std::array<std::uint32_t, kLayerSlots> cursor{};
  std::uint32_t offset = 0;
  for (std::size_t layer = 0; layer < kLayerSlots; ++layer) {
    cursor[layer] = offset;
    if (counts[layer] != 0) {
      scene.batches_.push_back(LayerBatch{static_cast<std::uint32_t>(layer), offset, counts[layer]});
    }
    offset += counts[layer];
  }

  scene.instances_.resize(rects.size());
  for (const LayeredRect& lr : rects) {
    scene.instances_[cursor[lr.layer]++] = lr.rect;
    extend(scene.bounds_, lr.rect);
  }
  return scene;
}

std::vector<LayerStyle> spike_layer_styles() {
  // Semi-transparent fills so overlaps stay readable, as in Magic's stipples.
  return {
      {0.20F, 0.75F, 0.30F, 0.55F},  // 0 diff
      {0.85F, 0.20F, 0.20F, 0.55F},  // 1 poly
      {0.60F, 0.45F, 0.85F, 0.50F},  // 2 li1
      {0.95F, 0.95F, 0.95F, 0.80F},  // 3 contact
      {0.30F, 0.55F, 0.95F, 0.45F},  // 4 met1
      {0.95F, 0.70F, 0.20F, 0.80F},  // 5 via
      {0.85F, 0.35F, 0.75F, 0.40F},  // 6 met2
      {0.25F, 0.85F, 0.85F, 0.35F},  // 7 met3
  };
}

}  // namespace hefesto::render
