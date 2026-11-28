// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Hefesto random stream (capability simulation-sweeps "Design of
// experiments": SplitMix64) and the Monte Carlo sample seeds built on it
// (simulation-monte-carlo "Sample seeds"). Both are pure integer arithmetic,
// so they give the same values on every platform and do not depend on the
// worker or the order in which samples run (design D14).
//
// Owner: SIMADV (M9 R3, task 15.1/15.3).

#pragma once

#include <cstdint>

namespace hefesto::sim {

/// v_t(S): the t-th value (t = 0, 1, ...) of the stream with seed S.
[[nodiscard]] std::uint64_t random_value(std::uint64_t seed, std::uint64_t t) noexcept;

/// u = (v >> 11) * 2^-53, uniform in [0, 1).
[[nodiscard]] double random_unit(std::uint64_t value) noexcept;

/// The stream as a sequence: next() returns v_0, v_1, ...
class RandomStream {
 public:
  explicit RandomStream(std::uint64_t seed) noexcept : seed_(seed) {}
  std::uint64_t next() noexcept { return random_value(seed_, t_++); }
  double next_unit() noexcept { return random_unit(next()); }

 private:
  std::uint64_t seed_;
  std::uint64_t t_ = 0;
};

/// "Sample seeds": s_i = 1 + (v_0(B * 2^32 + i) mod 2147483646), for base
/// seed B and sample index i. The caller checks B's range.
[[nodiscard]] std::uint32_t sample_seed_value(std::uint32_t base, std::uint32_t index) noexcept;

/// The seed of the nominal sample.
inline constexpr std::uint32_t kNominalSeed = 1;

/// Base seed range of "Monte Carlo dimension".
inline constexpr std::uint32_t kMaxBaseSeed = 2'147'483'647;

}  // namespace hefesto::sim
