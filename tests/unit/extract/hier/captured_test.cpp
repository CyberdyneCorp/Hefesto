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

TEST_CASE("EX-047: the subcell substrate merges as captured (the array pass is not compared)",
          "[cap:extract-substrate][req:Subcell substrate connections]") {
  HierCase c("EX-047", true);
  const auto run = c.run("ex");
  const auto captured = lines_with(captured_ext("EX-047"), "merge ");
  REQUIRE(captured.size() == 6);
  const std::vector<std::string> subcell(captured.begin(), captured.begin() + 4);
  CHECK(lines_with(run.text, "merge ") == subcell);
}

TEST_CASE("EX-053: the 1-D array merge as captured", "[cap:extract-hierarchy][req:Array interactions]") {
  HierCase c("EX-053", true);
  const auto run = c.run("ex");
  const auto captured = lines_with(captured_ext("EX-053"), "merge ");
  REQUIRE(captured.size() == 1);
  const auto ours = lines_with(run.text, "merge ");
  CHECK(std::find(ours.begin(), ours.end(), captured[0]) != ours.end());
}

}  // namespace
}  // namespace hefesto::test::exthier
