// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `load <cell> -silent` and `-quiet` (scripting-pdk-integration "Command
// surface used by device generators"; design D14): Magic's CmdLoad sets
// DBVerbose for the read, so sg13g2's on-demand builders (`load <c>
// -silent` of a cell not in memory) print nothing. Regression: Hefesto
// ignored both options and printed "Creating new cell". Expected texts
// verified against Magic 8.3.694 (fc5daa9e) with the arm64 capture image,
// 2026-11-11:
//   load top -silent   ->  (nothing)
//   load top2 -quiet   ->  Cell top2 couldn't be read / No such file or directory
//   load top3          ->  ... / Creating new cell
//   load have -silent  ->  only the zero-timestamp warning (not gated)
// Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "pdk_fixture.hpp"

using hefesto::script::test::PdkSession;
using hefesto::script::test::TempDir;
using hefesto::script::test::write_file;

namespace {

std::vector<std::string> load(PdkSession& s, std::vector<std::string> args) {
  const auto run = s.dispatcher->execute(hefesto::cmd::Invocation{"load", std::move(args), hefesto::cmd::Origin::script, std::nullopt});
  std::vector<std::string> out;
  for (const auto& d : run.diagnostics) {
    out.push_back(d.text);
  }
  return out;
}

}  // namespace

TEST_CASE("load -silent prints nothing for a new cell", "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  PdkSession s;
  TempDir dir;
  const std::string top = (dir.path() / "top").string();
  CHECK(load(s, {top, "-silent"}).empty());
  REQUIRE(s.editor.cells.find("top") != nullptr);
  CHECK(s.editor.cells.find("top")->flags().available);
  CHECK(s.eval("cellname list window") == "top");
}

TEST_CASE("load -quiet prints only errors", "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  PdkSession s;
  TempDir dir;
  // A name with a directory part is reported by its file path (DBio.c:1589).
  const std::string top2 = (dir.path() / "top2").string();
  CHECK(load(s, {top2, "-quiet"}) == std::vector<std::string>{"File " + top2 + " couldn't be read", "No such file or directory"});
  // Without an option the new cell is announced.
  const std::string top3 = (dir.path() / "top3").string();
  CHECK(load(s, {top3}) ==
        std::vector<std::string>{"File " + top3 + " couldn't be read", "No such file or directory", "Creating new cell"});
}

TEST_CASE("load -silent and -quiet of an existing file", "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  PdkSession s;
  TempDir dir;
  const std::string text = "magic\ntech minimum\ntimestamp 0\n<< end >>\n";
  write_file(dir.path() / "have.mag", text);
  write_file(dir.path() / "have2.mag", text);
  write_file(dir.path() / "have3.mag", text);
  const std::vector<std::string> stamp{"\"have\" has a zero timestamp; it should be written out",
                                       "    to establish a correct timestamp."};
  CHECK(load(s, {(dir.path() / "have").string(), "-silent"}) == stamp);  // not gated by DBVerbose
  const auto quiet = load(s, {(dir.path() / "have2").string(), "-quiet"});
  CHECK(std::find(quiet.begin(), quiet.end(), "Cell have2 read from path " + dir.path().string()) == quiet.end());
  const auto plain = load(s, {(dir.path() / "have3").string()});
  CHECK(std::find(plain.begin(), plain.end(), "Cell have3 read from path " + dir.path().string()) != plain.end());
}

TEST_CASE("the last of several verbosity options wins as in Magic", "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  // CmdLoad reads the options from the end of the line, so the first one
  // written is applied last.
  PdkSession s;
  TempDir dir;
  const std::string a = (dir.path() / "a").string();
  CHECK(load(s, {a, "-quiet", "-silent"}) == std::vector<std::string>{"File " + a + " couldn't be read", "No such file or directory"});
  CHECK(load(s, {(dir.path() / "b").string(), "-silent", "-quiet"}).empty());
}
