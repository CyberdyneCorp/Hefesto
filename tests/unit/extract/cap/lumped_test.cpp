// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Lumped resistance (tasks.md 6.7), with the values Magic printed for
// EX-017 and EX-018, the pinned L/W formula (ExtBasic.c:585-645) and
// Q-EXTRACT-22 per mode. Owner: EXTCAP.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "cap_fixture.hpp"

namespace {

using hefesto::test::cap::Layout;
using hefesto::test::cap::style;
namespace extract = hefesto::extract;

extract::NodeRegion with_class(std::int64_t area, std::int64_t perimeter) {
  extract::NodeRegion n;
  n.classes.push_back(extract::ClassArea{area, perimeter});
  return n;
}

extract::ExtStyle one_class(double sheet) {
  extract::ExtStyle s;
  extract::ResistClass rc;
  rc.sheet_resistance = sheet;
  s.resist_classes.push_back(rc);
  return s;
}

}  // namespace

TEST_CASE("EX-017: a five-square wire and a square",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  Layout l("style exttest\n" + hefesto::test::cap::kBase + " resist metal1 80\nstyle exttest1000\n" +
           hefesto::test::cap::kBase + " resist metal1 80\n rscale 1000\n");
  l.paint("metal1", {0, 0, 10, 2});
  l.paint("metal1", {20, 0, 24, 4});
  auto r = l.extract_all();
  const std::size_t a = l.node_at("metal1", 5, 1);
  const std::size_t b = l.node_at("metal1", 22, 2);
  CHECK(r.nodes[a].classes == std::vector<extract::ClassArea>{{20, 24}});
  CHECK(r.nodes[b].classes == std::vector<extract::ClassArea>{{16, 16}});
  CHECK(extract::lumped_resistance(r.nodes[a], l.style()) == 400);
  CHECK(extract::lumped_resistance(r.nodes[b], l.style()) == 80);
  CHECK(extract::resistance_output(400, l.style().rscale) == 400);
  l.select("exttest1000");
  r = l.extract_all();
  CHECK(extract::resistance_output(extract::lumped_resistance(r.nodes[a], l.style()), l.style().rscale) == 0);
  CHECK(extract::resistance_output(extract::lumped_resistance(r.nodes[b], l.style()), l.style().rscale) == 0);
}

TEST_CASE("resistance output rounds (r + rscale/2) / rscale in integers",
          "[cap:extract-resistance-lumped][req:Resistance output rounding]") {
  CHECK(extract::resistance_output(400, 1000) == 0);
  CHECK(extract::resistance_output(500, 1000) == 1);
  CHECK(extract::resistance_output(1499, 1000) == 1);
  CHECK(extract::resistance_output(30480, 1) == 30480);
  CHECK(extract::resistance_output(7, 3) == 2);  // (7 + 1) / 3
}

TEST_CASE("EX-018: per-class area and perimeter, the contact counted in both classes",
          "[cap:extract-resistance-lumped][req:Per-class area and perimeter of a node]") {
  Layout l(style(" resist poly 5000\n resist metal1 80\n"));
  l.paint("poly", {0, 0, 10, 2});
  l.paint("m1c", {10, 0, 12, 2});
  l.paint("metal1", {12, 0, 22, 2});
  const auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  CHECK(r.nodes[0].classes == std::vector<extract::ClassArea>{{24, 28}, {24, 28}});
  CHECK(extract::lumped_resistance(r.nodes[0], l.style()) == 30480);
}

TEST_CASE("node records list each class's area and perimeter in class order",
          "[cap:extract-resistance-lumped][req:Area and perimeter values in records]") {
  Layout l(style(" resist poly 7000\n resist metal1 80\n"));
  l.paint("poly", {0, 0, 6, 2});
  l.paint("metal1", {0, 10, 10, 12});
  const auto r = l.extract_all();
  const std::size_t p = l.node_at("active", 1, 1);
  const std::size_t m = l.node_at("metal1", 1, 11);
  CHECK(r.nodes[p].classes == std::vector<extract::ClassArea>{{12, 16}, {0, 0}});
  CHECK(r.nodes[m].classes == std::vector<extract::ClassArea>{{0, 0}, {20, 24}});
}

TEST_CASE("class values with magic-compat ON are 0 when lumped resistance is off",
          "[cap:extract-resistance-lumped][req:Area and perimeter values in records]") {
  Layout l(style(" resist metal1 80\n"), true);
  l.paint("metal1", {0, 0, 10, 2});
  l.options().lumped = false;
  const auto r = l.extract_all();
  CHECK(r.nodes[0].classes == std::vector<extract::ClassArea>{{0, 0}});
}

TEST_CASE("class values with magic-compat OFF are kept when lumped resistance is off",
          "[cap:extract-resistance-lumped][req:Area and perimeter values in records]") {
  Layout l(style(" resist metal1 80\n"), false);
  l.paint("metal1", {0, 0, 10, 2});
  l.options().lumped = false;
  const auto r = l.extract_all();
  CHECK(r.nodes[0].classes == std::vector<extract::ClassArea>{{20, 24}});
}

TEST_CASE("the L/W estimate in Magic's single precision (pinned)",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  CHECK(extract::lumped_resistance(with_class(20, 24), one_class(80)) == 400);
  CHECK(extract::lumped_resistance(with_class(16, 16), one_class(80)) == 80);  // v = 0: one square
  CHECK(extract::lumped_resistance(with_class(12345, 1000), one_class(125)) == 2274);
  CHECK(extract::lumped_resistance(with_class(7, 11), one_class(1000)) == 1750);
  CHECK(extract::lumped_resistance(with_class(0, 24), one_class(80)) == 0);  // no area: no term
  CHECK(extract::lumped_resistance(with_class(10, 4), one_class(80)) == 80);  // v < 0: S = 0
}

TEST_CASE("a perimeter whose S rounds to P saturates instead of dividing by zero",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  // P = 2^24 + 1 rounds to 2^24 in single precision, and so does S: Magic
  // converts +inf to int (undefined behaviour); fixed in both modes.
  CHECK(extract::lumped_resistance(with_class(100000, 16777217), one_class(3)) ==
        std::numeric_limits<std::int32_t>::max());
}

TEST_CASE("a total past the int range keeps the previous value",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  extract::NodeRegion n;
  n.classes = {{16, 16}, {16, 16}};
  extract::ExtStyle s = one_class(2000000000);
  s.resist_classes.push_back(s.resist_classes.front());
  CHECK(extract::lumped_resistance(n, s) == 2000000000);
}

TEST_CASE("the resistclasses header lists each class's sheet resistance",
          "[cap:extract-resistance-lumped][req:Resistance class header]") {
  Layout l(style(" resist poly 7000\n resist metal1 80\n"));
  CHECK(extract::resistance_class_header(l.style()) == std::vector<std::int64_t>{7000, 80});
  Layout none(style(""));
  CHECK(extract::resistance_class_header(none.style()).empty());
}

TEST_CASE("Q-EXTRACT-22 with magic-compat ON: lumped resistance silences shorted ports",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  hefesto::compat::CompatSettings on;
  extract::ExtractOptions o;  // lumped on, extresist off
  CHECK_FALSE(extract::shorted_ports_warning("a", "b", o, on).has_value());
  o.lumped = false;
  const auto w = extract::shorted_ports_warning("a", "b", o, on);
  REQUIRE(w.has_value());
  CHECK(w->code == "EXTRACT-CAP-001");
  CHECK(w->text == "Warning:  Ports \"a\" and \"b\" are electrically shorted.");
}

TEST_CASE("Q-EXTRACT-22 with magic-compat OFF: only extresist silences shorted ports",
          "[cap:extract-resistance-lumped][req:Lumped resistance formula]") {
  hefesto::compat::CompatSettings off;
  off.magic_compat = false;
  extract::ExtractOptions o;
  const auto w = extract::shorted_ports_warning("a_uq3", "b", o, off);
  REQUIRE(w.has_value());
  CHECK(w->text == "Warning:  Ports \"a\" and \"b\" are electrically shorted.");
  CHECK_FALSE(extract::shorted_ports_warning("a_uq3", "a_uq7", o, off).has_value());  // same name
  o.extresist = true;
  CHECK_FALSE(extract::shorted_ports_warning("a", "b", o, off).has_value());
}
