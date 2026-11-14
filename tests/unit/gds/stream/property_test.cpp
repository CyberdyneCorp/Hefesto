// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Randomized property tests of the stream layer (tasks.md 9.9):
// - decode_real8(encode_real8(x)) == x for every binary64 value the format
//   holds (a normalized binary64 always fits the 56 mantissa bits), and the
//   encoding is monotonic in x;
// - decode_date(encode_date(t)) == t for every second of the 32-bit range,
//   under UTC and a pinned non-UTC zone, in both magic-compat settings.
//
// The random seed is Catch2's (--rng-seed), printed on failure. A failing
// seed is added to kRegressionSeeds so it is replayed on every run.
// Owner: GDSR.

#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "hefesto/gds/stream/dates.hpp"
#include "hefesto/gds/stream/encode.hpp"

namespace gds = hefesto::gds;

namespace {

// Seeds that once found a failure (none yet); each is replayed on every run.
constexpr std::array<std::uint64_t, 0> kRegressionSeeds{};

constexpr int kSamples = 200000;

std::vector<std::uint64_t> seeds() {
  std::vector<std::uint64_t> out(kRegressionSeeds.begin(), kRegressionSeeds.end());
  out.push_back(Catch::getSeed());
  return out;
}

/// The smallest and largest magnitudes of the format: 16^-65 and just below 16^63.
const double kMin = std::ldexp(1.0, -260);
const double kMax = std::ldexp(1.0, 252);

/// A random finite double whose magnitude the format holds.
double random_real(std::mt19937_64& rng) {
  for (;;) {
    const double d = std::bit_cast<double>(rng());
    if (std::isfinite(d) && std::abs(d) >= kMin && std::abs(d) < kMax) {
      return d;
    }
  }
}

/// A key that orders encodings as their values (sign and magnitude).
std::int64_t order_key(const std::array<std::byte, 8>& bytes) {
  std::uint64_t u = 0;
  for (const std::byte b : bytes) {
    u = (u << 8U) | std::to_integer<std::uint64_t>(b);
  }
  const auto magnitude = static_cast<std::int64_t>(u & 0x7fffffffffffffffULL);
  return (u >> 63U) != 0 ? -magnitude : magnitude;
}

std::int64_t round_trip(std::int64_t t, const char* zone, bool magic_compat) {
  const auto fields = gds::encode_date(t, zone);
  return gds::decode_date(fields, zone, magic_compat);
}

}  // namespace

TEST_CASE("eight-byte reals round-trip every representable value", "[cap:gds-write][req:Eight-byte real encoding]") {
  for (const std::uint64_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937_64 rng{seed};
    for (int i = 0; i < kSamples; ++i) {
      const double x = random_real(rng);
      INFO(x);
      REQUIRE(gds::decode_real8(gds::encode_real8(x)) == x);
    }
  }
  for (const double x : {kMin, -kMin, std::nextafter(kMax, 0.0), 1.0, 0.0625, std::nextafter(0.0625, 0.0)}) {
    INFO(x);
    CHECK(gds::decode_real8(gds::encode_real8(x)) == x);
  }
}

TEST_CASE("eight-byte real encoding is monotonic", "[cap:gds-write][req:Eight-byte real encoding]") {
  for (const std::uint64_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937_64 rng{seed};
    for (int i = 0; i < kSamples; ++i) {
      double a = random_real(rng);
      double b = i % 4 == 0 ? std::nextafter(a, std::numeric_limits<double>::infinity()) : random_real(rng);
      if (std::abs(b) >= kMax || a == b) {
        continue;
      }
      if (b < a) {
        std::swap(a, b);
      }
      INFO(a << " < " << b);
      REQUIRE(order_key(gds::encode_real8(a)) < order_key(gds::encode_real8(b)));
      const bool below_zero = order_key(gds::encode_real8(a)) <= order_key(gds::encode_real8(0.0));
      REQUIRE(below_zero == (a <= 0.0));
    }
  }
}

TEST_CASE("dates round-trip every second of the 32-bit range", "[cap:gds-write][req:Date records]") {
  const std::array<std::int64_t, 6> edges{INT32_MIN, INT32_MAX, 0, -1, 951782400, 4107542399};
  for (const char* zone : {"UTC0", "<-03>3"}) {
    for (const bool compat : {true, false}) {
      INFO(zone << (compat ? " ON" : " OFF"));
      for (const std::int64_t t : edges) {
        if (t <= INT32_MAX || !compat) {
          CHECK(round_trip(t, zone, compat) == t);
        }
      }
      for (const std::uint64_t seed : seeds()) {
        INFO("seed " << seed);
        std::mt19937_64 rng{seed};
        std::uniform_int_distribution<std::int64_t> second{INT32_MIN, INT32_MAX};
        for (int i = 0; i < kSamples / 4; ++i) {
          const std::int64_t t = second(rng);
          INFO(t);
          REQUIRE(round_trip(t, zone, compat) == t);
        }
      }
    }
  }
}

TEST_CASE("magic-compat ON keeps the low 32 bits of a date", "[cap:gds-read][req:Structure timestamps]") {
  // 2040-01-01 00:00:00 UTC is past 2^31: ON wraps it as Magic's int does, OFF keeps it.
  const auto fields = gds::encode_date(2208988800, "UTC0");
  CHECK(gds::decode_date(fields, "UTC0", false) == 2208988800);
  CHECK(gds::decode_date(fields, "UTC0", true) == 2208988800 - (std::int64_t{1} << 32));
}
