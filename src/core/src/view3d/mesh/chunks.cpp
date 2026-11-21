// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Chunk plan of a mesh build (openspec/changes/m10-3d-view/design.md D6
// "Chunks", "Determinism"): the clip area is covered by a square quadtree
// whose root has the clip area's lower-left corner and a power-of-two
// side; a node is split while it holds more than about 65,536 tiles or is
// wider than kMaxChunkSide, and nodes with no tile are dropped. The leaves
// are the chunks, numbered by the Morton code of their lower-left cell at
// the deepest level, and listed in that (Z) order, so the plan depends only
// on the geometry.
//
// An incremental rebuild keeps the chunks it has and adds leaves of the
// same quadtree only where changed paint fell outside every chunk
// (plan_new_chunks); chunks are never re-split after the first build.
//
// Owner: MESH (design D3).

#include <algorithm>

#include "internal.hpp"

namespace hefesto::view3d::detail {

namespace {

struct Node {
  std::int64_t x0 = 0;
  std::int64_t y0 = 0;
  std::int64_t side = 1;
  int depth = 0;
  std::uint32_t ix = 0;  // position among the nodes of its depth
  std::uint32_t iy = 0;
};

class Planner {
 public:
  Planner(const FlatLayout& flat, const db::Rect& clip, std::size_t max_tiles)
      : flat_(flat), clip_(clip), max_tiles_(max_tiles) {}

  /// An incremental plan: only nodes meeting `changed` and holding no
  /// `existing` chunk area become leaves.
  void restrict_to(std::span<const db::Rect> existing, std::span<const db::Rect> changed) {
    existing_ = existing;
    changed_ = changed;
    incremental_ = true;
  }

  void visit(const Node& node) {
    const db::Rect area = area_of(node);
    if (area.is_null()) {
      return;
    }
    if (incremental_) {
      const Overlap overlap = overlap_of(area);
      if (overlap == Overlap::skip) {
        return;
      }
      if (overlap == Overlap::holds_existing) {
        if (node.depth < kMaxChunkDepth && node.side > 1) {
          split(node);
        }
        return;
      }
    }
    leaf_or_split(node, area);
  }

  [[nodiscard]] std::vector<ChunkCell> take() { return std::move(out_); }

 private:
  enum class Overlap : std::uint8_t { skip, holds_existing, free };

  // How a node of an incremental plan relates to the existing chunks:
  // skipped when it meets no changed rectangle or lies inside a chunk (that
  // chunk is rebuilt as it is), split when it holds chunks, else free.
  [[nodiscard]] Overlap overlap_of(const db::Rect& area) const {
    const auto meets = [&area](const db::Rect& r) { return !intersect(area, r).is_null(); };
    if (std::none_of(changed_.begin(), changed_.end(), meets)) {
      return Overlap::skip;
    }
    bool holds = false;
    for (const db::Rect& chunk : existing_) {
      if (!meets(chunk)) {
        continue;
      }
      if (intersect(area, chunk) == area) {
        return Overlap::skip;
      }
      holds = true;
    }
    return holds ? Overlap::holds_existing : Overlap::free;
  }

  void leaf_or_split(const Node& node, const db::Rect& area) {
    const std::size_t tiles = count_tiles(area, max_tiles_ + 1);
    if (tiles == 0) {
      return;
    }
    const bool can_split = node.depth < kMaxChunkDepth && node.side > 1;
    if (can_split && (tiles > max_tiles_ || node.side > kMaxChunkSide)) {
      split(node);
      return;
    }
    const int shift = kMaxChunkDepth - node.depth;
    out_.push_back(ChunkCell{area, morton(node.ix << shift, node.iy << shift)});
  }

  // Children in Z order: lower-left, lower-right, upper-left, upper-right.
  void split(const Node& node) {
    const std::int64_t half = node.side / 2;
    for (std::uint32_t q = 0; q < 4; ++q) {
      const std::uint32_t dx = q & 1U;
      const std::uint32_t dy = q >> 1U;
      visit(Node{node.x0 + (dx * half), node.y0 + (dy * half), half, node.depth + 1, (node.ix << 1U) | dx,
                 (node.iy << 1U) | dy});
    }
  }

  [[nodiscard]] db::Rect area_of(const Node& node) const {
    const auto clamp = [](std::int64_t v, db::Coord lo, db::Coord hi) {
      return static_cast<db::Coord>(std::clamp<std::int64_t>(v, lo, hi));
    };
    return db::Rect{clamp(node.x0, clip_.x0, clip_.x1), clamp(node.y0, clip_.y0, clip_.y1),
                    clamp(node.x0 + node.side, clip_.x0, clip_.x1), clamp(node.y0 + node.side, clip_.y0, clip_.y1)};
  }

  // The tiles of every flat type overlapping `area`, counted up to `limit`.
  [[nodiscard]] std::size_t count_tiles(const db::Rect& area, std::size_t limit) const {
    std::size_t n = 0;
    for (const FlatType& type : flat_.types) {
      type.plane.enumerate(area, tech::TypeMask::of(type.type), [&](const db::TileView& tile) {
        if (!intersect(tile.area, area).is_null()) {
          ++n;
        }
        return n < limit;
      });
      if (n >= limit) {
        break;
      }
    }
    return n;
  }

  const FlatLayout& flat_;
  db::Rect clip_;
  std::size_t max_tiles_;
  std::span<const db::Rect> existing_;
  std::span<const db::Rect> changed_;
  bool incremental_ = false;
  std::vector<ChunkCell> out_;
};

std::int64_t root_side(const db::Rect& clip) {
  std::int64_t side = 1;
  while (side < std::max(clip.width(), clip.height())) {
    side *= 2;
  }
  return side;
}

std::uint32_t spread(std::uint32_t v) noexcept {
  v &= 0xFFFFU;
  v = (v | (v << 8U)) & 0x00FF00FFU;
  v = (v | (v << 4U)) & 0x0F0F0F0FU;
  v = (v | (v << 2U)) & 0x33333333U;
  v = (v | (v << 1U)) & 0x55555555U;
  return v;
}

}  // namespace

std::uint32_t morton(std::uint32_t x, std::uint32_t y) noexcept {
  return spread(x) | (spread(y) << 1U);
}

std::vector<ChunkCell> plan_chunks(const FlatLayout& flat, const db::Rect& clip, std::size_t max_tiles) {
  if (clip.is_null()) {
    return {};
  }
  Planner planner(flat, clip, std::max<std::size_t>(max_tiles, 1));
  planner.visit(Node{clip.x0, clip.y0, root_side(clip), 0, 0, 0});
  return planner.take();
}

std::vector<ChunkCell> plan_new_chunks(const FlatLayout& flat, const db::Rect& clip, std::span<const db::Rect> existing,
                                       std::span<const db::Rect> changed, std::size_t max_tiles) {
  if (clip.is_null() || changed.empty()) {
    return {};
  }
  Planner planner(flat, clip, std::max<std::size_t>(max_tiles, 1));
  planner.restrict_to(existing, changed);
  planner.visit(Node{clip.x0, clip.y0, root_side(clip), 0, 0, 0});
  return planner.take();
}

}  // namespace hefesto::view3d::detail
