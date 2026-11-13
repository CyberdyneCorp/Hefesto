// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/util/libc_random.hpp"

namespace hefesto::util {

void LibcRandom::seed(std::uint32_t value) {
  std::array<std::int32_t, kDeg> r{};
  r[0] = value == 0 ? 1 : static_cast<std::int32_t>(value);
  for (std::size_t i = 1; i < kDeg; ++i) {
    // 16807 * r[i-1] % (2^31 - 1) without overflow (Schrage's method).
    const std::int32_t hi = r[i - 1] / 127773;
    const std::int32_t lo = r[i - 1] % 127773;
    std::int32_t word = 16807 * lo - 2836 * hi;
    if (word < 0) {
      word += 2147483647;
    }
    r[i] = word;
  }
  for (std::size_t i = 0; i < kDeg; ++i) {
    state_[i] = static_cast<std::uint32_t>(r[i]);
  }
  front_ = kSep;
  rear_ = 0;
  for (int i = 0; i < 10 * static_cast<int>(kDeg); ++i) {
    static_cast<void>(next());
  }
}

std::int64_t LibcRandom::next() {
  state_[front_] += state_[rear_];
  const std::uint32_t result = state_[front_] >> 1U;
  front_ = (front_ + 1) % kDeg;
  rear_ = (rear_ + 1) % kDeg;
  return static_cast<std::int64_t>(result);
}

}  // namespace hefesto::util
