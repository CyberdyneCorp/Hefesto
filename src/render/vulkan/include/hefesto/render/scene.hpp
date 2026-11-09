// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GPU-ready rectangle scene: one instance per rectangle, grouped by layer so
// that the renderer issues one instanced draw per layer (capability
// hefesto-performance, requirement "Interactive frame rate").
//
// This header is Vulkan-free: it is plain data shared by the renderer, the
// benchmark and the desktop shell.

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace hefesto::render {

/// An axis-aligned rectangle in internal database units: (x0, y0) is the
/// lower-left corner and (x1, y1) the upper-right one. This is the exact
/// layout of one GPU instance (16 bytes).
struct RectInstance {
  std::int32_t x0 = 0;
  std::int32_t y0 = 0;
  std::int32_t x1 = 0;
  std::int32_t y1 = 0;

  friend bool operator==(const RectInstance&, const RectInstance&) = default;
};
static_assert(sizeof(RectInstance) == 16, "RectInstance is uploaded as-is to the GPU");

/// A rectangle tagged with its layer index, as produced by readers or generators.
struct LayeredRect {
  RectInstance rect;
  std::uint8_t layer = 0;
};

/// A contiguous run of instances of one layer inside RectScene::instances().
struct LayerBatch {
  std::uint32_t layer = 0;
  std::uint32_t first = 0;
  std::uint32_t count = 0;

  friend bool operator==(const LayerBatch&, const LayerBatch&) = default;
};

/// Bounding box of a scene in internal units. Empty scenes have empty() true.
struct Bounds {
  std::int64_t x0 = 0;
  std::int64_t y0 = 0;
  std::int64_t x1 = 0;
  std::int64_t y1 = 0;
  bool valid = false;

  [[nodiscard]] bool empty() const noexcept { return !valid; }
  [[nodiscard]] std::int64_t width() const noexcept { return x1 - x0; }
  [[nodiscard]] std::int64_t height() const noexcept { return y1 - y0; }
};

/// A world-space rectangle (the visible window of the camera).
struct WorldWindow {
  double x0 = 0.0;
  double y0 = 0.0;
  double x1 = 0.0;
  double y1 = 0.0;
};

/// Rectangles sorted by layer (stable within a layer) with one batch per
/// non-empty layer, in increasing layer order. Draw order is therefore layer
/// order, which is also the order of the style table.
class RectScene {
 public:
  RectScene() = default;

  /// Builds a scene with a counting sort by layer: O(n), stable.
  [[nodiscard]] static RectScene from_layered(std::span<const LayeredRect> rects);

  [[nodiscard]] std::span<const RectInstance> instances() const noexcept { return instances_; }
  [[nodiscard]] std::span<const LayerBatch> batches() const noexcept { return batches_; }
  [[nodiscard]] const Bounds& bounds() const noexcept { return bounds_; }
  [[nodiscard]] std::size_t size() const noexcept { return instances_.size(); }
  [[nodiscard]] std::uint64_t byte_size() const noexcept {
    const std::uint64_t count = instances_.size();  // implicit: size_t and uint64_t may be the same type
    return count * sizeof(RectInstance);
  }

 private:
  std::vector<RectInstance> instances_;
  std::vector<LayerBatch> batches_;
  Bounds bounds_;
};

/// Fill of a layer: straight (non-premultiplied) RGBA in 0..1 and an
/// optional 8x8 stipple in Magic's OpenGL orientation (display-style-files
/// "Stipple patterns", decision X-5): stipple[0] is the bottom pixel row of
/// each 8x8 cell anchored to the bottom-left of the drawing area, and the
/// most significant bit is the leftmost pixel; a pixel is painted only where
/// its bit is 1.
struct LayerStyle {
  float r = 1.0F;
  float g = 1.0F;
  float b = 1.0F;
  float a = 1.0F;
  bool stippled = false;
  std::array<std::uint8_t, 8> stipple{};
};

/// A fixed 8-entry style table for the spike, loosely modeled on Magic's
/// sky130 colors (diff, poly, li1, contact, met1, via, met2, met3). Real
/// layer styles come from the PDK display styles (hefesto-design-system,
/// "Layer colors come from the PDK, not the theme").
[[nodiscard]] std::vector<LayerStyle> spike_layer_styles();

}  // namespace hefesto::render
