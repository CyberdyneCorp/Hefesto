// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SplitMix64 stream and Monte Carlo sample seeds (random.hpp).
//
// Owner: SIMADV.

#include "hefesto/sim/runset/random.hpp"

namespace hefesto::sim {

namespace {

constexpr std::uint64_t kGolden = 0x9E3779B97F4A7C15ULL;

constexpr std::uint64_t mix(std::uint64_t z) noexcept {
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31U);
}

}  // namespace

std::uint64_t random_value(std::uint64_t seed, std::uint64_t t) noexcept {
  return mix(seed + (t + 1) * kGolden);  // modulo 2^64
}

double random_unit(std::uint64_t value) noexcept {
  return static_cast<double>(value >> 11U) * 0x1.0p-53;
}

std::uint32_t sample_seed_value(std::uint32_t base, std::uint32_t index) noexcept {
  const std::uint64_t key = (std::uint64_t{base} << 32U) + index;
  return static_cast<std::uint32_t>(1 + random_value(key, 0) % 2'147'483'646ULL);
}

}  // namespace hefesto::sim
