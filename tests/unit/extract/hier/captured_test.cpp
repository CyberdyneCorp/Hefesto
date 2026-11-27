// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_subtree against the captured `.ext` files of the hierarchical EX
// cases (openspec/changes/m7-extraction tasks.md 17.1). Owner: EXTHIER.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "hier_fixture.hpp"

namespace hefesto::test::exthier {
namespace {

std::vector<std::string> records(const std::string& text) {
  std::vector<std::string> out = lines_with(text, "cap ");
  for (const std::string& m : lines_with(text, "merge ")) {
    out.push_back(m);
  }
  return out;
}

TEST_CASE("The captured hierarchical records: EX-043, 049, 050, 052, 054, 055, 056",
          "[cap:extract-hierarchy][req:Merge records and pass-2 adjustments]") {
  for (const std::string id : {"EX-043", "EX-049", "EX-050", "EX-052", "EX-054", "EX-055", "EX-056"}) {
    HierCase c(id, true);
    c.options.labelcheck = true;
    c.options.fringehalo = id != "EX-055";
    const auto run = c.run("ex");
    INFO(id);
    CHECK(records(run.text) == records(captured_ext(id)));
  }
}

TEST_CASE("EX-051: the illegal overlap is the one error Magic counts",
          "[cap:extract-hierarchy][req:Interaction processing and connections (pass 1)]") {
  HierCase c("EX-051", true);
  const auto run = c.run("ex");
  CHECK(read_file(case_root("EX-051") + "expected/stdout.txt").find("ex: 1 error") != std::string::npos);
  CHECK(run.result.errors == 1);
  CHECK(records(run.text) == records(captured_ext("EX-051")));
}

TEST_CASE("EX-047: the subcell and array-pass substrate merges as captured",
          "[cap:extract-substrate][req:Subcell substrate connections]") {
  // The array pass's group (x[2]/VSUB, x[1]/VSUB, VSUB) comes from the use's
  // box line, which Magic keeps as sc's bounding box: it spans the whole
  // array, so extArrayFunc's area B reaches the neighbouring elements.
  HierCase c("EX-047", true);
  const auto run = c.run("ex");
  const auto captured = lines_with(captured_ext("EX-047"), "merge ");
  REQUIRE(captured.size() == 6);
  CHECK(lines_with(run.text, "merge ") == captured);
}

TEST_CASE("EX-053: the array merges as captured", "[cap:extract-hierarchy][req:Array interactions]") {
  // B's box line (0 40 22 62) is bit2's bounding box for Magic, so
  // extArrayFunc's areas miss B's real overlaps: no merge for B.
  HierCase c("EX-053", true);
  const auto run = c.run("ex");
  const auto captured = lines_with(captured_ext("EX-053"), "merge ");
  REQUIRE(captured.size() == 1);
  CHECK(lines_with(run.text, "merge ") == captured);
}

TEST_CASE("EX-P04: the array merges follow the child's bounding box from its first use's box line",
          "[cap:extract-hierarchy][req:Array interactions]") {
  // The task 8.5 probe (tests/data/extract/hier/array_probe): arr and sub
  // read bit, bit2 and sc first, so each child's bounding box is the box
  // line of its first use (DBio.c:2544-2568), here written in parent
  // coordinates and spanning the array; arrfix and subfix reuse the cells
  // already read and capture the same merges. extArrayFunc's areas
  // (ExtArray.c:286-326) are then as read: B merges every element, and x's
  // separated elements merge through the box's reach.
  const std::string probe =
      std::string{HEFESTO_SOURCE_DIR} + "/tests/data/extract/hier/array_probe/EXTRACT/cases/EX-P04/";
  for (const std::string cell : {"arr", "sub"}) {
    HierCase c(HierCase::InputDir{probe + "input/"}, true);
    const auto run = c.run(cell);
    const auto captured = lines_with(read_file(probe + "expected/collected/input/" + cell + ".ext"), "merge ");
    INFO(cell);
    REQUIRE_FALSE(captured.empty());
    CHECK(lines_with(run.text, "merge ") == captured);

    HierCase painted(HierCase::InputDir{probe + "input/"}, true);
    painted.use_boxes = false;  // the children's own paint: not what Magic extracts here
    CHECK(lines_with(painted.run(cell).text, "merge ") != captured);
  }
}

}  // namespace
}  // namespace hefesto::test::exthier
