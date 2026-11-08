// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Deterministic synthetic layout for the M0 canvas spike (design.md D9):
// N rectangles over 8 layers, with a size and density mix modeled on a sky130
// standard-cell region (many small shapes, a few long routes). The same seed
// gives the same rectangles, bit for bit, on every platform and compiler:
// only integer arithmetic is used (no <random> distributions, whose output is
// implementation-defined).

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/render/scene.hpp"

namespace hefesto::render {

struct SyntheticLayoutParams {
  std::uint64_t count = 10'000'000;
  std::uint64_t seed = 0x48'45'46'45'53'54'4F'00ULL;  // "HEFESTO\0"
  /// Die edge in internal units (1 unit = 1 nm here: 5 mm by 5 mm).
  std::int32_t die_size = 5'000'000;
  /// Standard-cell row pitch (sky130 hd: 2.72 um).
  std::int32_t row_pitch = 2'720;
};

/// Number of layers the generator uses (indices 0..7, see spike_layer_styles()).
inline constexpr std::uint8_t kSyntheticLayerCount = 8;

/// splitmix64: tiny, fast, and fully specified, so it is portable.
class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}
  std::uint64_t next() noexcept;
  /// Uniform integer in [0, bound) by multiply-shift (bound > 0).
  std::uint32_t below(std::uint32_t bound) noexcept;

 private:
  std::uint64_t state_;
};

[[nodiscard]] std::vector<LayeredRect> generate_synthetic_layout(const SyntheticLayoutParams& params);

/// FNV-1a over the generated rectangles, to check determinism across runs
/// and platforms (the benchmark prints it).
[[nodiscard]] std::uint64_t fingerprint(const std::vector<LayeredRect>& rects) noexcept;

}  // namespace hefesto::render
