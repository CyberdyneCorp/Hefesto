// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/canvas_index.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace hefesto::render {

namespace {

std::int64_t ceil_div(std::int64_t a, std::int64_t b) {
  return (a + b - 1) / b;
}

std::int32_t to_i32(std::int64_t v) {
  return static_cast<std::int32_t>(
      std::clamp<std::int64_t>(v, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}

/// A LOD cell key: row in the high half, column in the low half (sorts row-major).
std::uint64_t cell_key(std::int64_t cx, std::int64_t cy) {
  return (static_cast<std::uint64_t>(cy) << 32U) | static_cast<std::uint64_t>(cx);
}
std::int64_t key_x(std::uint64_t key) {
  return static_cast<std::int64_t>(key & 0xFFFF'FFFFU);
}
std::int64_t key_y(std::uint64_t key) {
  return static_cast<std::int64_t>(key >> 32U);
}

void sort_unique(std::vector<std::uint64_t>& keys) {
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
}

}  // namespace

/// Builds a CanvasIndex in three steps: classify rectangles, derive the LOD
/// cells per layer and level, then counting-sort everything by table slot.
class CanvasIndexBuilder {
 public:
  CanvasIndexBuilder(std::span<const LayeredRect> rects, const CanvasIndexParams& params)
      : rects_(rects) {
    idx_.params_ = params;
    if (params.tiles == 0 || params.lod_base <= 0 || params.lod_levels == 0 || params.lod_levels > 12) {
      throw std::invalid_argument("CanvasIndexParams out of range");
    }
  }

  CanvasIndex build() {
    if (rects_.empty()) {
      return std::move(idx_);
    }
    init_geometry();
    classify_rects();
    build_lod_cells();
    sort_into_table();
    return std::move(idx_);
  }

 private:
  struct Cell {
    std::uint32_t slot;
    RectInstance rect;
  };

  [[nodiscard]] std::uint32_t levels() const { return idx_.params_.lod_levels; }

  void init_geometry() {
    std::uint32_t max_layer = 0;
    for (const LayeredRect& lr : rects_) {
      Bounds& b = idx_.bounds_;
      if (!b.valid) {
        b = Bounds{lr.rect.x0, lr.rect.y0, lr.rect.x1, lr.rect.y1, true};
      }
      b.x0 = std::min<std::int64_t>(b.x0, lr.rect.x0);
      b.y0 = std::min<std::int64_t>(b.y0, lr.rect.y0);
      b.x1 = std::max<std::int64_t>(b.x1, lr.rect.x1);
      b.y1 = std::max<std::int64_t>(b.y1, lr.rect.y1);
      max_layer = std::max<std::uint32_t>(max_layer, lr.layer);
    }
    const auto t = static_cast<std::int64_t>(idx_.params_.tiles);
    idx_.tile_w_ = std::max<std::int64_t>(ceil_div(idx_.bounds_.width(), t), 1);
    idx_.tile_h_ = std::max<std::int64_t>(ceil_div(idx_.bounds_.height(), t), 1);
    idx_.layers_ = max_layer + 1;
    idx_.groups_ = 2 * levels() + 1;
    cell_keys_.assign(static_cast<std::size_t>(idx_.layers_) * levels(), {});
  }

  [[nodiscard]] std::uint32_t tile_of(std::int64_t x, std::int64_t y) const {
    const auto t = static_cast<std::int64_t>(idx_.params_.tiles);
    const std::int64_t tx = std::clamp<std::int64_t>((x - idx_.bounds_.x0) / idx_.tile_w_, 0, t - 1);
    const std::int64_t ty = std::clamp<std::int64_t>((y - idx_.bounds_.y0) / idx_.tile_h_, 0, t - 1);
    return static_cast<std::uint32_t>(ty * t + tx);
  }

  /// Size class: the smallest k with max side < S_k, or K for larger rectangles.
  [[nodiscard]] std::uint32_t size_class(const RectInstance& r) const {
    const std::int64_t side = std::max<std::int64_t>(std::int64_t{r.x1} - r.x0, std::int64_t{r.y1} - r.y0);
    for (std::uint32_t k = 0; k < levels(); ++k) {
      if (side < idx_.cell_size(k)) {
        return k;
      }
    }
    return levels();
  }

  /// Records the slot of every rectangle and seeds the LOD cells of its class
  /// with the cell containing its center.
  void classify_rects() {
    rect_slots_.resize(rects_.size());
    for (std::size_t i = 0; i < rects_.size(); ++i) {
      const RectInstance& r = rects_[i].rect;
      const std::uint32_t cls = size_class(r);
      rect_slots_[i] = static_cast<std::uint32_t>(idx_.slot(rects_[i].layer, cls, tile_of(r.x0, r.y0)));
      if (cls < levels()) {
        const std::int64_t s = idx_.cell_size(cls);
        const std::int64_t cx = ((std::int64_t{r.x0} + r.x1) / 2 - idx_.bounds_.x0) / s;
        const std::int64_t cy = ((std::int64_t{r.y0} + r.y1) / 2 - idx_.bounds_.y0) / s;
        keys_for(rects_[i].layer, cls).push_back(cell_key(cx, cy));
      }
    }
  }

  std::vector<std::uint64_t>& keys_for(std::uint32_t layer, std::uint32_t level) {
    return cell_keys_[static_cast<std::size_t>(layer) * levels() + level];
  }

  void build_lod_cells() {
    for (std::uint32_t layer = 0; layer < idx_.layers_; ++layer) {
      for (std::uint32_t level = 0; level < levels(); ++level) {
        auto& keys = keys_for(layer, level);
        sort_unique(keys);
        emit_runs(layer, level, keys);
        if (level + 1 < levels()) {
          auto& next = keys_for(layer, level + 1);
          for (std::uint64_t k : keys) {
            next.push_back(cell_key(key_x(k) / 4, key_y(k) / 4));
          }
        }
        std::vector<std::uint64_t>().swap(keys);
      }
    }
  }

  /// Merges row-sorted cells into horizontal runs, cut where a run enters a new tile.
  void emit_runs(std::uint32_t layer, std::uint32_t level, const std::vector<std::uint64_t>& keys) {
    const std::int64_t s = idx_.cell_size(level);
    const std::uint32_t group = levels() + 1 + level;
    std::size_t i = 0;
    while (i < keys.size()) {
      const std::int64_t cy = key_y(keys[i]);
      const std::int64_t x0 = idx_.bounds_.x0 + key_x(keys[i]) * s;
      const std::int64_t y0 = idx_.bounds_.y0 + cy * s;
      const std::uint32_t tile = tile_of(x0, y0);
      std::size_t j = i + 1;
      while (j < keys.size() && key_y(keys[j]) == cy && key_x(keys[j]) == key_x(keys[j - 1]) + 1 &&
             tile_of(idx_.bounds_.x0 + key_x(keys[j]) * s, y0) == tile) {
        ++j;
      }
      const std::int64_t x1 = idx_.bounds_.x0 + (key_x(keys[j - 1]) + 1) * s;
      const RectInstance run{to_i32(x0), to_i32(y0), to_i32(x1), to_i32(y0 + s)};
      cells_.push_back(Cell{static_cast<std::uint32_t>(idx_.slot(layer, group, tile)), run});
      i = j;
    }
  }

  void sort_into_table() {
    const std::size_t slots = static_cast<std::size_t>(idx_.layers_) * idx_.groups_ * idx_.tile_count();
    const std::size_t total = rects_.size() + cells_.size();
    if (total > std::numeric_limits<std::uint32_t>::max()) {
      throw std::length_error("CanvasIndex: too many instances");
    }
    idx_.table_.assign(slots, {});
    for (std::uint32_t s : rect_slots_) ++idx_.table_[s].count;
    for (const Cell& c : cells_) ++idx_.table_[c.slot].count;
    std::uint32_t offset = 0;
    for (auto& range : idx_.table_) {
      range.first = offset;
      offset += range.count;
    }
    std::vector<std::uint32_t> cursor(slots);
    for (std::size_t s = 0; s < slots; ++s) cursor[s] = idx_.table_[s].first;
    idx_.instances_.resize(total);
    for (std::size_t i = 0; i < rects_.size(); ++i) idx_.instances_[cursor[rect_slots_[i]]++] = rects_[i].rect;
    for (const Cell& c : cells_) idx_.instances_[cursor[c.slot]++] = c.rect;
    idx_.rect_count_ = rects_.size();
  }

  std::span<const LayeredRect> rects_;
  CanvasIndex idx_;
  std::vector<std::uint32_t> rect_slots_;
  std::vector<std::vector<std::uint64_t>> cell_keys_;  // per (layer, level)
  std::vector<Cell> cells_;
};

CanvasIndex CanvasIndex::build(std::span<const LayeredRect> rects, const CanvasIndexParams& params) {
  return CanvasIndexBuilder(rects, params).build();
}

std::int64_t CanvasIndex::cell_size(std::uint32_t level) const noexcept {
  return std::int64_t{params_.lod_base} << (2U * level);
}

int CanvasIndex::lod_level_for(double units_per_pixel) const noexcept {
  if (units_per_pixel * 4.0 <= static_cast<double>(params_.lod_base)) {
    return -1;
  }
  for (std::uint32_t level = 0; level < params_.lod_levels; ++level) {
    if (static_cast<double>(cell_size(level)) >= units_per_pixel) {
      return static_cast<int>(level);
    }
  }
  return static_cast<int>(params_.lod_levels) - 1;
}

void CanvasIndex::visible_ranges(const WorldWindow& window, double units_per_pixel,
                                 std::vector<LayerBatch>& out) const {
  out.clear();
  if (table_.empty()) {
    return;
  }
  const int lod = lod_level_for(units_per_pixel);
  const std::uint32_t first_class = lod < 0 ? 0U : static_cast<std::uint32_t>(lod) + 1U;
  const std::uint32_t k = params_.lod_levels;
  for (std::uint32_t layer = 0; layer < layers_; ++layer) {
    if (lod >= 0) {
      const auto level = static_cast<std::uint32_t>(lod);
      append_group(layer, k + 1 + level, cell_size(level), window, out);
    }
    for (std::uint32_t cls = first_class; cls < k; ++cls) {
      append_group(layer, cls, cell_size(cls), window, out);
    }
    // Rectangles larger than every cell: one range, never culled by tile.
    const Range& first = table_[slot(layer, k, 0)];
    const Range& last = table_[slot(layer, k, tile_count() - 1)];
    if (last.first + last.count > first.first) {
      out.push_back(LayerBatch{layer, first.first, last.first + last.count - first.first});
    }
  }
}

void CanvasIndex::append_group(std::uint32_t layer, std::uint32_t group, std::int64_t reach,
                               const WorldWindow& window, std::vector<LayerBatch>& out) const {
  // A shape stored in tile t starts in t and extends at most `reach` units
  // right and up, so tiles left of / below the window by `reach` can matter.
  const auto t = static_cast<double>(params_.tiles);
  auto tile_index = [t](double v, double origin, double size) {
    return static_cast<std::uint32_t>(std::clamp(std::floor((v - origin) / size), 0.0, t - 1.0));
  };
  const auto bx = static_cast<double>(bounds_.x0);
  const auto by = static_cast<double>(bounds_.y0);
  const auto tw = static_cast<double>(tile_w_);
  const auto th = static_cast<double>(tile_h_);
  const auto r = static_cast<double>(reach);
  if (window.x1 < bx - r || window.y1 < by - r || window.x0 > static_cast<double>(bounds_.x1) ||
      window.y0 > static_cast<double>(bounds_.y1)) {
    return;
  }
  const std::uint32_t tx0 = tile_index(window.x0 - r, bx, tw);
  const std::uint32_t tx1 = tile_index(window.x1, bx, tw);
  const std::uint32_t ty0 = tile_index(window.y0 - r, by, th);
  const std::uint32_t ty1 = tile_index(window.y1, by, th);
  for (std::uint32_t ty = ty0; ty <= ty1; ++ty) {
    const Range& a = table_[slot(layer, group, ty * params_.tiles + tx0)];
    const Range& b = table_[slot(layer, group, ty * params_.tiles + tx1)];
    const std::uint32_t count = b.first + b.count - a.first;
    if (count == 0) {
      continue;
    }
    if (!out.empty() && out.back().layer == layer && out.back().first + out.back().count == a.first) {
      out.back().count += count;
    } else {
      out.push_back(LayerBatch{layer, a.first, count});
    }
  }
}

}  // namespace hefesto::render
