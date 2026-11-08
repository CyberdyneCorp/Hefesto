// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/synthetic_layout.hpp"

#include <algorithm>
#include <array>

namespace hefesto::render {

namespace {

/// One kind of shape: its layer, its share of the population (per mille) and
/// its size range in internal units (nm). Values follow sky130 hd cells.
struct ShapeKind {
  std::uint8_t layer;
  std::uint32_t weight;
  std::int32_t min_w, max_w, min_h, max_h;
};

constexpr std::array<ShapeKind, 8> kKinds{{
    {0, 120, 300, 2'000, 420, 1'500},        // diff: transistor active areas
    {1, 150, 150, 150, 500, 2'000},          // poly: gates
    {2, 170, 170, 170, 300, 3'000},          // li1: local interconnect
    {3, 250, 170, 170, 170, 170},            // contacts (mcon/licon)
    {4, 150, 1'000, 50'000, 140, 300},       // met1: horizontal routes
    {5, 80, 150, 150, 150, 150},             // via
    {6, 60, 140, 300, 2'000, 100'000},       // met2: vertical routes
    {7, 20, 10'000, 500'000, 300, 800},      // met3: a few long routes
}};

constexpr std::uint32_t kTotalWeight = 1000;

const ShapeKind& pick_kind(std::uint32_t ticket) noexcept {
  std::uint32_t acc = 0;
  for (const ShapeKind& kind : kKinds) {
    acc += kind.weight;
    if (ticket < acc) {
      return kind;
    }
  }
  return kKinds.back();
}

std::int32_t in_range(SplitMix64& rng, std::int32_t lo, std::int32_t hi) noexcept {
  const auto span = static_cast<std::uint32_t>(hi - lo) + 1U;
  return lo + static_cast<std::int32_t>(rng.below(span));
}

LayeredRect make_rect(SplitMix64& rng, const SyntheticLayoutParams& p) noexcept {
  const ShapeKind& kind = pick_kind(rng.below(kTotalWeight));
  const std::int32_t w = std::min(in_range(rng, kind.min_w, kind.max_w), p.die_size);
  const std::int32_t h = std::min(in_range(rng, kind.min_h, kind.max_h), p.die_size);
  const std::int32_t x = in_range(rng, 0, p.die_size - w);
  // Shapes snap to standard-cell rows, which gives the striped density of a
  // placed design; long vertical routes simply cross rows.
  const auto rows = static_cast<std::uint32_t>(std::max(p.die_size / p.row_pitch, 1));
  const auto row = static_cast<std::int32_t>(rng.below(rows));
  const std::int32_t y = std::min(row * p.row_pitch + in_range(rng, 0, p.row_pitch - 1), p.die_size - h);
  return LayeredRect{RectInstance{x, y, x + w, y + h}, kind.layer};
}

}  // namespace

std::uint64_t SplitMix64::next() noexcept {
  std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31U);
}

std::uint32_t SplitMix64::below(std::uint32_t bound) noexcept {
  const std::uint64_t high = next() >> 32U;
  return static_cast<std::uint32_t>((high * bound) >> 32U);
}

std::vector<LayeredRect> generate_synthetic_layout(const SyntheticLayoutParams& params) {
  SplitMix64 rng(params.seed);
  std::vector<LayeredRect> rects;
  rects.reserve(params.count);
  for (std::uint64_t i = 0; i < params.count; ++i) {
    rects.push_back(make_rect(rng, params));
  }
  return rects;
}

std::uint64_t fingerprint(const std::vector<LayeredRect>& rects) noexcept {
  std::uint64_t hash = 0xCBF29CE484222325ULL;
  auto mix = [&hash](std::uint32_t word) {
    for (unsigned shift = 0; shift < 32U; shift += 8U) {
      hash ^= (word >> shift) & 0xFFU;
      hash *= 0x100000001B3ULL;
    }
  };
  for (const LayeredRect& r : rects) {
    mix(static_cast<std::uint32_t>(r.rect.x0));
    mix(static_cast<std::uint32_t>(r.rect.y0));
    mix(static_cast<std::uint32_t>(r.rect.x1));
    mix(static_cast<std::uint32_t>(r.rect.y1));
    mix(r.layer);
  }
  return hash;
}

}  // namespace hefesto::render
