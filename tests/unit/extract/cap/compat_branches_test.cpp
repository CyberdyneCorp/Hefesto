// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// One test per mode of each magic-compat split of extract/cap
// (openspec/changes/m7-extraction/design.md D9). Owner: EXTCAP.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

#include "hefesto/extract/cap/compat_branches.hpp"

namespace {

namespace cap = hefesto::extract::cap;

hefesto::compat::CompatSettings mode(bool on) {
  hefesto::compat::CompatSettings c;
  c.magic_compat = on;
  return c;
}

}  // namespace

TEST_CASE("Q-EXTRACT-17 with magic-compat ON: shield fractions per rule, distinct nodes, stored inside type",
          "[cap:extract-capacitance][req:Fringe-halo sideoverlap]") {
  const auto on = mode(true);
  CHECK(cap::shield_counted_per_rule(on));
  CHECK(cap::shield_needs_distinct_nodes(on));
  CHECK(cap::shield_inside_type(on, 30, 20) == 30);  // the contact, unreduced
}

TEST_CASE("Q-EXTRACT-17 with magic-compat OFF: shield fraction once per tile, residue inside type",
          "[cap:extract-capacitance][req:Fringe-halo sideoverlap]") {
  const auto off = mode(false);
  CHECK_FALSE(cap::shield_counted_per_rule(off));
  CHECK_FALSE(cap::shield_needs_distinct_nodes(off));
  CHECK(cap::shield_inside_type(off, 30, 20) == 20);
}

TEST_CASE("Q-EXTRACT-18 with magic-compat ON: whole-segment length, area summed over rules",
          "[cap:extract-capacitance][req:Legacy sideoverlap (fringe directly below the edge)]") {
  const auto on = mode(true);
  CHECK(cap::legacy_cap_length(on, 10, 4) == 10);
  const std::array<std::int64_t, 2> areas{5, 5};
  CHECK(cap::legacy_accounted_area(on, areas) == 10);
  CHECK(cap::legacy_accounted_area(on, {}) == 0);
}

TEST_CASE("Q-EXTRACT-18 with magic-compat OFF: sub-segment length, area once per tile",
          "[cap:extract-capacitance][req:Legacy sideoverlap (fringe directly below the edge)]") {
  const auto off = mode(false);
  CHECK(cap::legacy_cap_length(off, 10, 4) == 4);
  const std::array<std::int64_t, 2> areas{5, 3};
  CHECK(cap::legacy_accounted_area(off, areas) == 5);
  CHECK(cap::legacy_accounted_area(off, {}) == 0);
}

TEST_CASE("Q-EXTRACT-24 with magic-compat ON: concave by Magic's type test",
          "[cap:extract-capacitance][req:Corner fringe capacitance to substrate]") {
  const auto on = mode(true);
  CHECK(cap::substrate_corner_concave(on, {.types_connect = true, .same_node = false, .quadrant_is_space = false}));
  CHECK_FALSE(cap::substrate_corner_concave(on, {.types_connect = false, .same_node = true, .quadrant_is_space = true}));
}

TEST_CASE("Q-EXTRACT-24 with magic-compat OFF: concave by the coupling test",
          "[cap:extract-capacitance][req:Corner fringe capacitance to substrate]") {
  const auto off = mode(false);
  CHECK_FALSE(
      cap::substrate_corner_concave(off, {.types_connect = true, .same_node = false, .quadrant_is_space = true}));
  CHECK_FALSE(
      cap::substrate_corner_concave(off, {.types_connect = true, .same_node = true, .quadrant_is_space = false}));
  CHECK(cap::substrate_corner_concave(off, {.types_connect = false, .same_node = true, .quadrant_is_space = true}));
}

TEST_CASE("Q-EXTRACT-22 with magic-compat ON: shorted-ports warning suppressed by lumped resistance",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  const auto on = mode(true);
  CHECK(cap::shorted_ports_warning_suppressed(on, true, false));
  CHECK_FALSE(cap::shorted_ports_warning_suppressed(on, false, true));
}

TEST_CASE("Q-EXTRACT-22 with magic-compat OFF: shorted-ports warning suppressed only by extresist",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  const auto off = mode(false);
  CHECK_FALSE(cap::shorted_ports_warning_suppressed(off, true, false));
  CHECK(cap::shorted_ports_warning_suppressed(off, false, true));
}

TEST_CASE("class values with magic-compat ON are stored only with lumped resistance on",
          "[cap:extract-resistance-lumped][req:Per-class area and perimeter of a node]") {
  const auto on = mode(true);
  CHECK(cap::class_values_stored(on, true));
  CHECK_FALSE(cap::class_values_stored(on, false));
}

TEST_CASE("class values with magic-compat OFF are always stored",
          "[cap:extract-resistance-lumped][req:Per-class area and perimeter of a node]") {
  const auto off = mode(false);
  CHECK(cap::class_values_stored(off, true));
  CHECK(cap::class_values_stored(off, false));
}
