// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Substrate capacitance and option gating (tasks.md 6.1), with the values
// Magic printed for EX-002, EX-003, EX-014 and EX-015
// (tests/golden/EXTRACT/cases/EX-0NN/expected). Owner: EXTCAP.

#include <catch2/catch_test_macros.hpp>

#include "cap_fixture.hpp"

namespace {

using hefesto::test::cap::Layout;
using hefesto::test::cap::lg;
using hefesto::test::cap::style;
namespace extract = hefesto::extract;
namespace db = hefesto::db;

const std::string kEx002Rules = " areacap metal1 30\n perimc metal1 space 20\n";

}  // namespace

TEST_CASE("EX-002: a lone metal1 rectangle has area plus perimeter capacitance",
          "[cap:extract-capacitance][req:Area capacitance to substrate]") {
  Layout l(style(kEx002Rules) + "style exttest1000\n" + hefesto::test::cap::kBase + kEx002Rules + " cscale 1000\n");
  l.paint("metal1", {0, 0, 10, 4});
  auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  CHECK(r.nodes[0].substrate_cap == 1760.0);  // 40*30 + 28*20
  CHECK(lg(extract::node_cap_output(r.nodes[0].substrate_cap, l.style().cscale)) == "1760");
  l.select("exttest1000");
  r = l.extract_all();
  CHECK(lg(extract::node_cap_output(r.nodes[0].substrate_cap, l.style().cscale)) == "1.76");
}

TEST_CASE("EX-003: a split-tile triangle walks two sides and its diagonal",
          "[cap:extract-capacitance][req:Perimeter capacitance to substrate]") {
  Layout l(style(kEx002Rules));
  l.triangle("metal1", {0, 0, 4, 4}, db::TriangleCorner::sw);
  const auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  // (16/2)*30 + (4 + 4 + (int)sqrt(32))*20 = 240 + 13*20
  CHECK(r.nodes[0].substrate_cap == 500.0);
}

TEST_CASE("the split-tile diagonal length is (int)sqrt(w*w + h*h), pinned",
          "[cap:extract-capacitance][req:Perimeter capacitance to substrate]") {
  Layout l(style(" perimc metal1 space 1\n resist metal1 10\n"));
  l.triangle("metal1", {0, 0, 3, 5}, db::TriangleCorner::ne);
  const auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  // sqrt(34) = 5.83 -> 5, plus the two orthogonal sides 3 and 5.
  CHECK(r.nodes[0].substrate_cap == 13.0);
  REQUIRE(r.nodes[0].classes.size() == 1);
  CHECK(r.nodes[0].classes[0].perimeter == 13);
  CHECK(r.nodes[0].classes[0].area == 7);  // 15 / 2 in integer division
}

TEST_CASE("a split tile past 46340 units forms its diagonal in 64 bits",
          "[cap:extract-capacitance][req:Perimeter capacitance to substrate]") {
  // Magic squares the sides in int and overflows (undefined behaviour);
  // fixed in both modes.
  Layout l(style(" perimc metal1 space 1\n"));
  l.triangle("metal1", {0, 0, 60000, 80000}, db::TriangleCorner::sw);
  const auto r = l.extract_all();
  REQUIRE(r.nodes.size() == 1);
  CHECK(r.nodes[0].substrate_cap == 60000.0 + 80000.0 + 100000.0);
}

TEST_CASE("EX-014: extract no capacitance zeroes node capacitance but keeps coupling",
          "[cap:extract-capacitance][req:Capacitance option gating]") {
  Layout l(style(" areacap metal2 10\n overlap metal2 metal1 20\n"));
  l.paint("metal1", {0, 0, 10, 10});
  l.paint("metal2", {0, 0, 10, 10});
  l.options().capacitance = false;
  const auto r = l.extract_all();
  const std::size_t a = l.node_at("metal1", 5, 5);
  const std::size_t b = l.node_at("metal2", 5, 5);
  CHECK(extract::node_cap_output(r.nodes[b].substrate_cap, 1) == 0.0);  // 0 - 1000, clamped
  CHECK(r.coupling.value(a, b) == 2000.0);
}

TEST_CASE("EX-015: extract no coupling writes no cap record and keeps the area term",
          "[cap:extract-capacitance][req:Capacitance option gating]") {
  Layout l(style(" areacap metal2 10\n overlap metal2 metal1 20\n"));
  l.paint("metal1", {0, 0, 10, 10});
  l.paint("metal2", {0, 0, 10, 10});
  l.options().coupling = false;
  const auto r = l.extract_all();
  const std::size_t b = l.node_at("metal2", 5, 5);
  CHECK(r.nodes[b].substrate_cap == 1000.0);
  CHECK(r.coupling.pairs().empty());
}

TEST_CASE("a clip area clips tile areas and side lengths",
          "[cap:extract-capacitance][req:Area capacitance to substrate]") {
  Layout l(style(kEx002Rules));
  l.paint("metal1", {0, 0, 10, 4});
  auto nodes = l.find_nodes();
  auto in = l.inputs();
  in.clip = db::Rect{0, 0, 5, 4};
  REQUIRE(extract::substrate_caps(nodes, in).ok());
  // Area 20*30; the left side 4, bottom and top 5 each; the right side lies
  // outside the clip area (x = 10).
  CHECK(nodes[0].substrate_cap == 600.0 + 14 * 20.0);
}

TEST_CASE("substrate_caps without a technology reports EXTRACT-CAP-002",
          "[cap:extract-capacitance][req:Area capacitance to substrate]") {
  Layout l(style(kEx002Rules));
  auto nodes = l.find_nodes();
  auto in = l.inputs();
  in.technology = nullptr;
  const auto status = extract::substrate_caps(nodes, in);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == "EXTRACT-CAP-002");
}
