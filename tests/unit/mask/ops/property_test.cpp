// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Randomized property tests of the operators (hefesto-testing-strategy
// "Geometry property tests"; tasks.md 4.8), seeded and reproducible: each
// case runs a fixed list of seeds, and a failing seed is reported so that it
// can be added to the regression list below.
//   - grow d then shrink d contains the original (square and Euclidean);
//   - mask-plane contents do not depend on the paint order;
//   - or and and are commutative;
//   - and-not of a layer with itself is empty;
//   - the bridge-lim snapshot mode (magic-compat OFF) does not depend on the
//     order of the input rectangles.

#include <algorithm>
#include <random>

#include "ops_fixture.hpp"

using namespace hefesto;
using namespace hefesto::mask;
using namespace hefesto::mask::test;

#define PROPS "[cap:hefesto-testing-strategy][req:Geometry property tests]"
#define CAP "[cap:mask-output-operators]"

namespace {

constexpr int kSeeds = 60;
// Seeds that once failed are kept here (none so far); they run first.
constexpr std::array<std::uint32_t, 0> kRegressionSeeds{};

std::vector<std::uint32_t> seeds() {
  std::vector<std::uint32_t> out(kRegressionSeeds.begin(), kRegressionSeeds.end());
  for (int i = 1; i <= kSeeds; ++i) {
    out.push_back(static_cast<std::uint32_t>(i * 7919));
  }
  return out;
}

Rects random_rects(std::mt19937& rng, int count, db::Coord span) {
  std::uniform_int_distribution<db::Coord> pos(-span, span);
  std::uniform_int_distribution<db::Coord> size(1, span / 2);
  Rects out;
  for (int i = 0; i < count; ++i) {
    const db::Coord x = pos(rng);
    const db::Coord y = pos(rng);
    out.push_back(Rect{x, y, x + size(rng), y + size(rng)});
  }
  return out;
}

MaskPlane plane_of(const Rects& rects) {
  MaskPlane p;
  for (const Rect& r : rects) {
    REQUIRE(p.paint(r).ok());
  }
  return p;
}

/// True when every point of `inner` is in `outer`.
bool contains(const Rects& outer, const Rects& inner) {
  MaskPlane rest = plane_of(inner);
  const MaskPlane o = plane_of(outer);
  REQUIRE(rest.subtract(o).ok());
  return rest.empty();
}

}  // namespace

TEST_CASE("grow then shrink by the same distance contains the original",
          PROPS CAP "[req:grow and shrink (default, square metric)]") {
  for (const std::uint32_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937 rng{seed};
    OpsFixture f;
    const Rects cur = random_rects(rng, 6, 60);
    const db::Coord d = std::uniform_int_distribution<db::Coord>{1, 8}(rng);
    const Rects closed = f.apply(cur, {op(OpKind::grow, {d}), op(OpKind::shrink, {d})});
    CHECK(contains(closed, strips(cur)));
  }
}

TEST_CASE("Euclidean grow then shrink contains the original", PROPS CAP "[req:Euclidean grow and shrink]") {
  for (const std::uint32_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937 rng{seed};
    OpsFixture f;
    f.style.options = 0x08;
    const Rects cur = random_rects(rng, 6, 60);
    const db::Coord d = std::uniform_int_distribution<db::Coord>{1, 8}(rng);
    const Rects closed = f.apply(cur, {op(OpKind::grow, {d}), op(OpKind::shrink, {d})});
    CHECK(contains(closed, strips(cur)));
  }
}

TEST_CASE("mask-plane contents do not depend on the paint order",
          PROPS "[cap:mask-output-operators][req:Geometric model of mask planes]") {
  for (const std::uint32_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937 rng{seed};
    Rects rects = random_rects(rng, 12, 80);
    const Rects first = plane_of(rects).solid_rects();
    std::shuffle(rects.begin(), rects.end(), rng);
    CHECK(plane_of(rects).solid_rects() == first);
    std::reverse(rects.begin(), rects.end());
    CHECK(plane_of(rects).solid_rects() == first);
  }
}

TEST_CASE("or and and are commutative", PROPS CAP "[req:or, and, and-not]") {
  for (const std::uint32_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937 rng{seed};
    OpsFixture f;
    const std::size_t a = f.mask_layer(random_rects(rng, 5, 50));
    const std::size_t b = f.mask_layer(random_rects(rng, 5, 50));
    const LayerOp or_a = with_layers(op(OpKind::op_or), {a});
    const LayerOp or_b = with_layers(op(OpKind::op_or), {b});
    CHECK(f.run({or_a, or_b}) == f.run({or_b, or_a}));
    CHECK(f.run({or_a, with_layers(op(OpKind::op_and), {b})}) == f.run({or_b, with_layers(op(OpKind::op_and), {a})}));
  }
}

TEST_CASE("and-not of a layer with itself is empty", PROPS CAP "[req:or, and, and-not]") {
  for (const std::uint32_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937 rng{seed};
    OpsFixture f;
    const std::size_t a = f.mask_layer(random_rects(rng, 8, 50));
    CHECK(f.run({with_layers(op(OpKind::op_or), {a}), with_layers(op(OpKind::and_not), {a})}).empty());
    f.paint(f.m1, random_rects(rng, 1, 50).front());
    CHECK(f.run({with_types(op(OpKind::op_or), {f.m1}), with_types(op(OpKind::and_not), {f.m1})}).empty());
  }
}

TEST_CASE("bridge-lim with magic-compat OFF depends only on the geometry", PROPS CAP "[req:bridge-lim]") {
  for (const std::uint32_t seed : seeds()) {
    INFO("seed " << seed);
    std::mt19937 rng{seed};
    Rects cur = random_rects(rng, 6, 60);
    const Rects limits = random_rects(rng, 2, 60);
    OpsFixture f1;
    f1.compat(false);
    const std::size_t l1 = f1.mask_layer(limits);
    const Rects first = f1.apply(cur, {with_layers(op(OpKind::bridge_lim, {5, 4}), {l1})});
    std::shuffle(cur.begin(), cur.end(), rng);
    OpsFixture f2;
    f2.compat(false);
    const std::size_t l2 = f2.mask_layer(limits);
    CHECK(f2.apply(cur, {with_layers(op(OpKind::bridge_lim, {5, 4}), {l2})}) == first);
  }
}
