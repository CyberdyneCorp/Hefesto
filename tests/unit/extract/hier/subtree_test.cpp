// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_subtree on the hierarchical EX cases (openspec/changes/m7-extraction
// tasks.md 8.2-8.5): the `merge` and `cap` records against the spec
// scenarios and the captured `.ext` files. Owner: EXTHIER.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/extract/hier/interact.hpp"
#include "hefesto/extract/hier/node_table.hpp"
#include "hefesto/extract/hier/yank.hpp"
#include "hier_fixture.hpp"

namespace hefesto::test::exthier {
namespace {

double cap_sum(const std::string& text, std::string_view a, std::string_view b) {
  double sum = 0.0;
  for (const std::string& line : lines_with(text, "cap ")) {
    const auto w = words(line);
    const bool match = (w[1] == "\"" + std::string{a} + "\"" && w[2] == "\"" + std::string{b} + "\"") ||
                       (w[1] == "\"" + std::string{b} + "\"" && w[2] == "\"" + std::string{a} + "\"");
    if (match) {
      sum += std::stod(w[3]);
    }
  }
  return sum;
}

TEST_CASE("Halo without coupling is 1, with coupling and adjust sidehalo + 1",
          "[cap:extract-hierarchy][req:Interaction halo and step squares]") {
  extract::ExtStyle style;
  style.side_halo = 4;
  extract::ExtractOptions o;
  CHECK(extract::hier_halo(style, o, {}) == 5);
  o.coupling = false;
  CHECK(extract::hier_halo(style, o, {}) == 1);
  o.coupling = true;
  o.adjust = false;
  CHECK(extract::hier_halo(style, o, {}) == 1);
}

TEST_CASE("EX-055: a cell spanning three step squares has an interaction in each",
          "[cap:extract-hierarchy][req:Interaction halo and step squares]") {
  HierCase c("EX-055", true);
  db::CellDef& top = c.cell("ex");
  const auto squares = extract::interaction_squares(top, 20, 5, true);
  std::size_t interactions = 0;
  for (const auto& s : squares) {
    interactions += s.kind == extract::SquareKind::interaction ? 1U : 0U;
    CHECK(s.square.x1 - s.square.x0 == 20);
  }
  CHECK(interactions == 3);
}

TEST_CASE("EX-050: a parent wire crossing a child is an interaction area with the halo",
          "[cap:extract-hierarchy][req:Interaction area]") {
  HierCase c("EX-050", true);
  db::CellDef& top = c.cell("ex");
  const db::Rect square{-1, -7, 11, 11};
  // Q-MASK-400 with magic-compat ON: the lone use's empty sibling box
  // (GeoNullRect expanded by the halo) joins the area.
  const auto on = extract::find_interaction(top, square, 1, true);
  REQUIRE(on.has_value());
  REQUIRE(on->has_value());
  CHECK(**on == db::Rect{-1, -1, 7, 5});
  const auto off = extract::find_interaction(top, square, 1, false);
  REQUIRE(off.has_value());
  REQUIRE(off->has_value());
  CHECK(**off == db::Rect{3, -1, 7, 5});
  CHECK_FALSE(extract::find_interaction(top, db::Rect{100, 100, 110, 110}, 1, true).has_value());
}

TEST_CASE("Name unification: the child name comes first", "[cap:extract-hierarchy][req:Name unification order]") {
  extract::HierNodeTable t(1);
  const std::size_t cum = t.node_of("b");
  const std::size_t added = t.node_of("c1/a");
  const std::size_t kept = t.unify(cum, added, false);
  REQUIRE(t.node(kept).names.size() == 2);
  CHECK(t.name(t.node(kept).names[0]) == "c1/a");
  CHECK(t.name(t.node(kept).names[1]) == "b");
  // A larger cumulative node keeps its first name; the smaller one's names go after it.
  const std::size_t three = t.node_of("x");
  const std::size_t joined = t.unify(three, kept, false);
  CHECK(t.name(t.node(joined).names[0]) == "c1/a");
  CHECK(t.name(t.node(joined).names[1]) == "x");
  CHECK(t.name(t.node(joined).names[2]) == "b");
}

TEST_CASE("EX-049: identical overlapping wires merge with the pass-2 adjustments",
          "[cap:extract-hierarchy][req:Merge records and pass-2 adjustments]") {
  HierCase c("EX-049", true);
  const auto run = c.run("ex");
  const auto merges = lines_with(run.text, "merge ");
  REQUIRE(merges.size() == 1);
  CHECK(merges[0] == "merge \"c1/a\" \"b\" -1760 -40 -28");
}

TEST_CASE("EX-050: a parent wire over a child wire of another net is a hierarchical cap",
          "[cap:extract-hierarchy][req:Hierarchical coupling records]") {
  HierCase c("EX-050", true);
  const auto run = c.run("ex");
  CHECK(cap_sum(run.text, "c1/a", "p") == 160.0);
  CHECK(lines_with(run.text, "merge ").empty());
}

TEST_CASE("EX-051: overlapping types that do not connect are an error",
          "[cap:extract-hierarchy][req:Interaction processing and connections (pass 1)]") {
  HierCase c("EX-051", true);
  const auto run = c.run("ex");
  CHECK(run.result.errors == 1);
  REQUIRE(run.result.feedback.size() == 1);
  CHECK(run.result.feedback[0].text == "Illegal overlap between ndiff and poly (types do not connect)");
  CHECK(run.result.feedback[0].area == db::Rect{4, 0, 6, 4});
}

TEST_CASE("EX-052: an unlabelled child node is named the hard way", "[cap:extract-hierarchy][req:Hard-way node names]") {
  HierCase c("EX-052", true);
  const auto run = c.run("ex");
  const auto merges = lines_with(run.text, "merge ");
  REQUIRE(merges.size() == 1);
  CHECK(merges[0].find("\"I1/metal1_10_4#\"") != std::string::npos);
  CHECK(merges[0].find("\"metal1_20_4#\"") != std::string::npos);
}

TEST_CASE("EX-053: abutting array elements merge with index ranges", "[cap:extract-hierarchy][req:Array interactions]") {
  HierCase c("EX-053", true);
  const auto run = c.run("ex");
  const auto merges = lines_with(run.text, "merge ");
  CHECK(std::find_if(merges.begin(), merges.end(), [](const std::string& m) {
          return m.rfind("merge \"A[1:3]/n\" \"A[0:2]/n\"", 0) == 0;
        }) != merges.end());
}

TEST_CASE("EX-054: a sticky parent label over child metal joins the child's node",
          "[cap:extract-hierarchy][req:Sticky labels across the hierarchy]") {
  HierCase c("EX-054", true);
  c.options.labelcheck = true;
  const auto run = c.run("ex");
  const auto merges = lines_with(run.text, "merge ");
  REQUIRE(merges.size() == 1);
  CHECK(merges[0].find("\"VDD\"") != std::string::npos);
  CHECK(merges[0].find("\"c/vdd\"") != std::string::npos);
}

TEST_CASE("EX-055: the hierarchical caps over the step squares sum to the sidewall coupling",
          "[cap:extract-hierarchy][req:Interaction halo and step squares]") {
  HierCase c("EX-055", true);
  c.options.fringehalo = false;
  const auto run = c.run("ex");
  CHECK(cap_sum(run.text, "c1/c", "p") == 1200.0);
}

TEST_CASE("A label two levels down is yanked with the instance path", "[cap:extract-hierarchy][req:Hierarchical label names]") {
  HierCase c("EX-049", true);
  db::CellLibrary lib;
  const auto metal1 = c.tech().find_type("metal1");
  REQUIRE(metal1.ok());
  db::CellDef& bit = *lib.create("bit", c.tech().plane_count()).value();
  REQUIRE(db::paint(bit, c.tech(), db::Rect{0, 0, 4, 4}, *metal1.id).ok());
  db::Label q;
  q.type = *metal1.id;
  q.rect = {2, 2, 2, 2};
  q.text = "q";
  bit.labels().push_back(q);
  db::recompute_bbox(bit);
  db::CellDef& mid = *lib.create("mid", c.tech().plane_count()).value();
  db::PlaceOptions array;
  array.id = "R";
  array.array = db::ArrayInfo{0, 0, 0, 3, 0, 10};
  REQUIRE(db::place(mid, bit, array).ok());
  db::recompute_bbox(mid);
  db::CellDef& top = *lib.create("top", c.tech().plane_count()).value();
  db::PlaceOptions one;
  one.id = "X1";
  auto use = db::place(top, mid, one);
  REQUIRE(use.ok());
  db::recompute_bbox(top);

  auto target = extract::new_yank_cell("__T__", c.tech().plane_count());
  extract::yank_use(*use.value(), db::Rect{0, 20, 4, 24}, *target, c.tech(), true);
  REQUIRE(target->labels().size() == 1);
  CHECK(target->labels()[0].text == "X1/R[2]/q");
  CHECK(target->labels()[0].rect == db::Rect{2, 22, 2, 22});
  CHECK(target->plane(c.tech().home_plane(*metal1.id).value()).type_at({1, 21}) == *metal1.id);
}

TEST_CASE("Hierarchical caps come before the merges in the file",
          "[cap:extract-hierarchy][req:Hierarchical section order in the file]") {
  HierCase c("EX-050", true);
  db::CellDef& top = c.cell("ex");
  const auto metal1 = c.tech().find_type("metal1");
  REQUIRE(metal1.ok());
  REQUIRE(db::paint(top, c.tech(), db::Rect{8, 0, 10, 4}, *metal1.id).ok());
  const auto run = c.run("ex");
  const std::size_t cap = run.text.find("\ncap ");
  const std::size_t merge = run.text.find("\nmerge ");
  REQUIRE(cap != std::string::npos);
  REQUIRE(merge != std::string::npos);
  CHECK(cap < merge);
}

}  // namespace
}  // namespace hefesto::test::exthier
