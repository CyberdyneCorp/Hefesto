// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability pcell-toolkit "Native generator equivalence", scenario "Round
// trip without scripts": a generated cell keeps its library/gencell/
// parameters properties byte for byte when it is read, moved and saved in a
// session that runs no generator script.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "framework_fixture.hpp"

using hefesto::test::PcellSession;

namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string property_lines(const std::string& mag) {
  std::string out;
  std::istringstream lines(mag);
  for (std::string line; std::getline(lines, line);) {
    if (line.rfind("string ", 0) == 0) {
      out += line + "\n";
    }
  }
  return out;
}

}  // namespace

TEST_CASE("Round trip without scripts", "[cap:pcell-toolkit][req:Native generator equivalence]") {
  const hefesto::test::TempDirectory tmp;
  const auto& dir = tmp.path();
  std::string cell;
  std::string before;
  {
    PcellSession s;
    // A list-valued parameter and a value needing quotes.
    REQUIRE(s.value("magic::gencell tpdk::res x1 w 2 l {1 2}") == "0");
    cell = s.value("instance list celldef x1");
    REQUIRE(s.run("load " + cell).ok());
    REQUIRE(s.run("save").ok());
    before = property_lines(read_file(dir / (cell + ".mag")));
  }
  CHECK(before == "string gencell res\nstring library tpdk\nstring parameters w 2 l {1 2} nf 1 compatible {res res2}\n");
  {
    PcellSession s;  // no gencell runs: the cell is only read, moved and saved
    REQUIRE(s.run("load " + cell).ok());
    REQUIRE(s.run("select top cell").ok());
    REQUIRE(s.run("box values 0 0 10 10").ok());
    REQUIRE(s.run("select area").ok());
    REQUIRE(s.run("move e 10").ok());
    REQUIRE(s.run("save").ok());
    CHECK(s.value("cellname list property " + cell + " parameters") == "w 2 l {1 2} nf 1 compatible {res res2}");
  }
  CHECK(property_lines(read_file(dir / (cell + ".mag"))) == before);
}
