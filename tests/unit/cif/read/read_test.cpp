// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cif::read_text over the read engine (capability cif-format-io "cif read,
// write and flat options", "Read diagnostics", "Calls and
// transformations", "Label user extensions"): the adapter from
// CifReadTarget to mask::ReadEngine.
// Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "../cif_fixture.hpp"
#include "hefesto/cif/read/read.hpp"

namespace {

using namespace cif_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

cmd::CommandResult read(Fixture& f, const std::string& text) {
  cif::CifReadOptions options;
  options.settings = f.ed.mask.cif;
  options.warnings = f.ed.mask.warnings;
  options.files = &f.fs;
  return cif::read_text(f.ed, text, options);
}

std::vector<std::string> texts(const cmd::CommandResult& r) {
  std::vector<std::string> out;
  for (const auto& d : r.diagnostics) {
    out.push_back(d.text);
  }
  return out;
}

bool has(const std::vector<std::string>& lines, const std::string& text) {
  for (const std::string& l : lines) {
    if (l == text) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("a read without an input style fails", "[cap:cif-format-io][req:cif read, write and flat options]") {
  Fixture f;
  f.ed.mask.input_styles = mask::InputStyleSet{};
  const cmd::CommandResult r = read(f, "E\n");
  CHECK_FALSE(r.ok());
  CHECK(texts(r) == std::vector<std::string>{"Don't know how to read CIF:  nothing in tech file."});
}

TEST_CASE("definitions, calls with ids and top-level geometry", "[cap:cif-format-io][req:Calls and transformations]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.ed.edit_cell = &top;
  const cmd::CommandResult r =
      read(f, "DS 1 1 1;\n9 leaf;\nL CMF;\nB 40 20 20 10;\nDF;\n91 u1;\nC 1 R 0 1 T 100 0;\nL CMS;\nB 20 20 10 10;\nE\n");
  REQUIRE(r.ok());
  CHECK(texts(r).front() == "Warning: CIF reading is not undoable!  I hope that's OK.");
  const db::CellDef* leaf = f.ed.cells.find("leaf");
  REQUIRE(leaf != nullptr);
  CHECK(leaf->bbox() == db::Rect{0, 0, 4, 2});
  CHECK(leaf->flags().modified);
  REQUIRE(top.uses().size() == 1);
  const db::CellUse& use = *top.uses().front();
  CHECK(use.id == "u1");
  CHECK(use.transform == db::Transform{0, -1, 10, 1, 0, 0});
  // The top-level box went into the edit cell.
  CHECK(top.bbox().x0 == 0);
  CHECK(top.flags().modified);
  CHECK(f.ed.drc.pending.contains(*leaf));
}

TEST_CASE("labels keep the parser's type and stickiness", "[cap:cif-format-io][req:Label user extensions]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.ed.edit_cell = &top;
  REQUIRE(read(f, "DS 1 1 1;\n9 labs;\nL CMF;\nB 40 20 20 10;\n94 out 20 10 CMF;\n94 free 0 0;\nDF;\nE\n").ok());
  const db::CellDef* labs = f.ed.cells.find("labs");
  REQUIRE(labs != nullptr);
  REQUIRE(labs->labels().size() == 2);
  CHECK(labs->labels()[0].text == "out");
  CHECK(labs->labels()[0].type == f.type("metal1"));
  CHECK(labs->labels()[0].rect == db::Rect{2, 1, 2, 1});
  CHECK(labs->labels()[1].text == "free");
}

TEST_CASE("the engine's grid messages carry the CIF line prefix of the DF that paints them", "[cap:cif-format-io][req:Read diagnostics]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.ed.edit_cell = &top;
  const cmd::CommandResult r = read(f, "DS 1 1 1;\n9 odd;\nL CMF;\n\nB 40 20 25 10;\nDF;\nE\n");
  REQUIRE(r.ok());
  const std::vector<std::string> lines = texts(r);
  for (const std::string& l : lines) {
    UNSCOPED_INFO(l);
  }
  CHECK(has(lines, "Warning at line 6 of CIF file: Input off lambda grid by 1/2; grid redefined."));
  for (const std::string& l : lines) {
    CHECK_FALSE(l.starts_with("Input off lambda grid"));
  }
}

TEST_CASE("a forward call reports the cell as used but not defined", "[cap:cif-format-io][req:Read diagnostics]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.ed.edit_cell = &top;
  const cmd::CommandResult r = read(f, "C 7;\nE\n");
  REQUIRE(r.ok());
  for (const std::string& l : texts(r)) {
    UNSCOPED_INFO(l);
  }
  CHECK(has(texts(r), "Error at line 3 of CIF file: cell 7 was used but not defined."));
  const db::CellDef* seven = f.ed.cells.find("7");
  REQUIRE(seven != nullptr);
  CHECK_FALSE(seven->flags().available);
}

TEST_CASE("cif drccheck no leaves the read cells unmarked", "[cap:cif-format-io][req:cif drccheck option]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.ed.edit_cell = &top;
  f.ed.mask.cif.drccheck = false;
  REQUIRE(read(f, "DS 1 1 1;\n9 leaf;\nL CMF;\nB 40 20 20 10;\nDF;\nE\n").ok());
  const db::CellDef* leaf = f.ed.cells.find("leaf");
  REQUIRE(leaf != nullptr);
  CHECK_FALSE(f.ed.drc.pending.contains(*leaf));
}
