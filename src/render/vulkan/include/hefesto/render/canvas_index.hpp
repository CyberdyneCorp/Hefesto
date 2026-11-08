// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Spatial and level-of-detail index of the canvas (design.md D9 spike).
//
// Drawing every rectangle every frame does not reach 60 fps at 10 million
// rectangles (the brute-force numbers are in docs/spikes/canvas-spike.md), so
// the instances are ordered for two cheap per-frame decisions:
//
//   * Culling. The bounding box is cut into T x T tiles. Each rectangle goes
//     to the tile of its lower-left corner. Within a (layer, group) the
//     instances are tile-major, so the visible part of one tile row is one
//     contiguous instance range.
//
//   * Level of detail. Level L has square cells of S_L = base * 4^L units.
//     A rectangle whose larger side is below S_L belongs to size class <= L.
//     For each level, the cells touched by rectangles of classes <= L are
//     precomputed per layer and merged into horizontal runs. When a pixel
//     covers p units, the frame uses the smallest level with S_L >= p: it
//     draws the cells of that level (each about one to four pixels) instead
//     of the rectangles of classes <= L, and draws the larger rectangles
//     individually. Zoomed in (p <= base / 4) every rectangle is drawn.
//
// The LOD cells are drawn with the same pipeline as rectangles: the GPU sees
// one instance buffer and a list of draw ranges.

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "hefesto/render/scene.hpp"

namespace hefesto::render {

struct CanvasIndexParams {
  /// Tiles per side of the bounding box.
  std::uint32_t tiles = 64;
  /// Cell size of LOD level 0, in internal units.
  std::int32_t lod_base = 256;
  /// Number of LOD levels (cells grow by 4x per level).
  std::uint32_t lod_levels = 6;
};

class CanvasIndex {
 public:
  CanvasIndex() = default;

  [[nodiscard]] static CanvasIndex build(std::span<const LayeredRect> rects, const CanvasIndexParams& params = {});

  /// Every GPU instance: rectangles and LOD cells, in draw-range order.
  [[nodiscard]] std::span<const RectInstance> instances() const noexcept { return instances_; }
  [[nodiscard]] const Bounds& bounds() const noexcept { return bounds_; }
  [[nodiscard]] std::size_t rect_count() const noexcept { return rect_count_; }
  [[nodiscard]] std::size_t lod_cell_count() const noexcept { return instances_.size() - rect_count_; }
  [[nodiscard]] std::uint64_t byte_size() const noexcept {
    return static_cast<std::uint64_t>(instances_.size()) * sizeof(RectInstance);
  }

  /// The LOD level used at `units_per_pixel`, or -1 when every rectangle is drawn.
  [[nodiscard]] int lod_level_for(double units_per_pixel) const noexcept;

  /// Replaces `out` with the ranges to draw, in layer order (adjacent ranges merged).
  void visible_ranges(const WorldWindow& window, double units_per_pixel, std::vector<LayerBatch>& out) const;

 private:
  struct Range {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
  };

  [[nodiscard]] std::size_t slot(std::uint32_t layer, std::uint32_t group, std::uint32_t tile) const noexcept {
    return (static_cast<std::size_t>(layer) * groups_ + group) * tile_count() + tile;
  }
  [[nodiscard]] std::uint32_t tile_count() const noexcept { return params_.tiles * params_.tiles; }
  [[nodiscard]] std::int64_t cell_size(std::uint32_t level) const noexcept;
  void append_group(std::uint32_t layer, std::uint32_t group, std::int64_t reach, const WorldWindow& window,
                    std::vector<LayerBatch>& out) const;

  friend class CanvasIndexBuilder;

  CanvasIndexParams params_;
  Bounds bounds_;
  std::int64_t tile_w_ = 1;
  std::int64_t tile_h_ = 1;
  std::uint32_t layers_ = 0;
  std::uint32_t groups_ = 0;  // size classes 0..K, then LOD levels 0..K-1
  std::vector<Range> table_;  // per (layer, group, tile)
  std::vector<RectInstance> instances_;
  std::size_t rect_count_ = 0;
};

}  // namespace hefesto::render
