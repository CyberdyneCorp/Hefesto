// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include "hefesto/render/synthetic_layout.hpp"

using hefesto::render::fingerprint;
using hefesto::render::generate_synthetic_layout;
using hefesto::render::SplitMix64;
using hefesto::render::SyntheticLayoutParams;

TEST_CASE("splitmix64 matches its reference sequence", "[cap:hefesto-testing-strategy][req:Cross-platform determinism checks]") {
  // Reference values of splitmix64 seeded with 1234567 (Vigna's C code).
  SplitMix64 rng(1234567);
  CHECK(rng.next() == 6457827717110365317ULL);
  CHECK(rng.next() == 3203168211198807973ULL);
  CHECK(rng.next() == 9817491932198370423ULL);
}

TEST_CASE("synthetic layout is deterministic and platform independent",
          "[cap:hefesto-testing-strategy][req:Cross-platform determinism checks]") {
  SyntheticLayoutParams p;
  p.count = 1000;
  const auto a = generate_synthetic_layout(p);
  const auto b = generate_synthetic_layout(p);
  REQUIRE(a.size() == 1000);
  CHECK(fingerprint(a) == fingerprint(b));
  // Pinned value: any platform or compiler producing another layout fails here,
  // so every benchmark machine draws the same rectangles.
  CHECK(fingerprint(a) == 12003618579180040715ULL);
  p.seed += 1;
  CHECK(fingerprint(generate_synthetic_layout(p)) != fingerprint(a));
}

TEST_CASE("synthetic rectangles stay on the die and use 8 layers",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  SyntheticLayoutParams p;
  p.count = 20'000;
  bool seen[8] = {};
  for (const auto& r : generate_synthetic_layout(p)) {
    REQUIRE(r.layer < 8);
    seen[r.layer] = true;
    REQUIRE(r.rect.x0 >= 0);
    REQUIRE(r.rect.y0 >= 0);
    REQUIRE(r.rect.x1 <= p.die_size);
    REQUIRE(r.rect.y1 <= p.die_size);
    REQUIRE(r.rect.x0 < r.rect.x1);
    REQUIRE(r.rect.y0 < r.rect.y1);
  }
  for (bool s : seen) {
    CHECK(s);
  }
}
