// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_cell (tasks.md 8.1; capabilities extract-hierarchy,
// extract-file-format, extract-commands): the bottom-up order with "don't
// use" cells, the header and `use` records before the cell's own records,
// the flat pipeline against captured EX files, the cell properties and the
// context checks.
//
// Owner: EXTCMD (tasks.md 8.1).

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <vector>

#include "../command/run_fixture.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/mask/gen/progress.hpp"

namespace extract = hefesto::extract;
using extcmd_test::case_expected;
using extcmd_test::case_input;
using extcmd_test::mag;
using extcmd_test::Session;
using extcmd_test::use;

namespace {

const std::string kLeaf = mag("<< metal1 >>\nrect 0 0 10 4\n");

std::vector<std::string> extracting_order(const std::string& printed) {
  std::vector<std::string> out;
  for (const std::string& line : extcmd_test::lines_with(printed, "Extracting ")) {
    out.push_back(line.substr(11, line.find(' ', 11) - 11));
  }
  return out;
}

// Loads EX case `id`'s cell `cell` with the case's technology, runs
// `commands`, and returns the written `<cell>.ext`.
std::string extract_case(const std::string& id, const std::string& cell, bool compat = true,
                         const std::string& commands = "extract all") {
  Session s(compat, true, "/w", case_input(id, "exttest.tech"));
  s.load(cell, case_input(id, cell + ".mag"));
  REQUIRE(s.run(commands).ok());
  return s.read(cell + ".ext");
}

extract::ExtractContext context_of(Session& s) {
  extract::ExtractContext ctx;
  ctx.style = s.editor.extract.current;
  ctx.options = s.editor.extract.options;
  ctx.technology = s.editor.technology.get();
  ctx.compat.magic_compat = s.project.compat.magic_compat;
  return ctx;
}

}  // namespace

TEST_CASE("an empty child is not extracted and its uses get no use record",
          "[cap:extract-hierarchy][req:Bottom-up extraction order]") {
  Session s;
  s.file("empty.mag", mag("<< labels >>\nrlabel metal1 0 0 0 0 0 lab\n"));
  s.load("top", mag("<< metal1 >>\nrect 0 0 4 4\n" + use("empty", "e0", 10, 0) + use("empty", "e1", 20, 0)));
  REQUIRE(s.run("extract all").ok());
  CHECK(extracting_order(s.take_printed()) == std::vector<std::string>{"top"});
  CHECK_FALSE(s.exists("empty.ext"));
  CHECK(extcmd_test::lines_with(s.read("top.ext"), "use ").empty());
}

TEST_CASE("children are extracted before their parents, each once",
          "[cap:extract-hierarchy][req:Bottom-up extraction order]") {
  Session s;
  s.file("leaf.mag", kLeaf);
  s.file("mid.mag", mag("<< metal1 >>\nrect 0 10 4 14\n" + use("leaf", "a") + use("leaf", "b", 20, 0)));
  s.load("top", mag(use("mid", "m0") + use("leaf", "l0", 0, 40)));
  REQUIRE(s.run("extract all").ok());
  CHECK(extracting_order(s.take_printed()) == std::vector<std::string>{"leaf", "mid", "top"});
}

TEST_CASE("the use records follow the header and precede the cell's own records",
          "[cap:extract-hierarchy][req:Hierarchical section order in the file]") {
  Session s;
  s.file("leaf.mag", kLeaf);
  s.load("top", mag("<< metal1 >>\nrect 0 10 4 14\n" + use("leaf", "l0", 0, 0)));
  REQUIRE(s.run("extract all").ok());
  const std::string text = s.read("top.ext");
  const std::size_t classes = text.find("resistclasses");
  const std::size_t uses = text.find("use leaf l0 1 0 0 0 1 0\n");
  const std::size_t nodes = text.find("node ");
  REQUIRE(classes != std::string::npos);
  REQUIRE(uses != std::string::npos);
  REQUIRE(nodes != std::string::npos);
  CHECK(classes < uses);
  CHECK(uses < nodes);
}

TEST_CASE("extract_cell reproduces the captured flat EX files", "[cap:extract-file-format][req:Record order]") {
  for (const auto* id : {"EX-004", "EX-016", "EX-018", "EX-020", "EX-026", "EX-033", "EX-045", "EX-063"}) {
    INFO(id);
    CHECK(extract_case(id, "ex") == case_expected(id, "ex.ext"));
  }
}

// Proposed Q-EXTRACT-39: Magic writes the line of a `device` property
// without its newline (ExtBasic.c:519-528).
TEST_CASE("a device property's line, magic-compat ON: unterminated as Magic writes it",
          "[cap:extract-commands][req:Cell properties affecting extraction]") {
  const std::string text = extract_case("EX-042", "dev2", true);
  CHECK(text == case_expected("EX-042", "dev2.ext"));
  CHECK(text.ends_with("extra"));
}

TEST_CASE("a device property's line, magic-compat OFF: a terminated record",
          "[cap:extract-commands][req:Cell properties affecting extraction]") {
  const std::string text = extract_case("EX-042", "dev2", false);
  CHECK(text == case_expected("EX-042", "dev2.ext") + "\n");
}

TEST_CASE("an abstract view names its substrate node without a substrate record",
          "[cap:extract-commands][req:Cell properties affecting extraction]") {
  const std::string text = extract_case("EX-048", "ex");
  CHECK(text == case_expected("EX-048", "ex.ext"));
  CHECK(text.find("abstract\n") != std::string::npos);
  CHECK(text.find("substrate ") == std::string::npos);
}

TEST_CASE("extract_cell needs a style and a technology", "[cap:extract-hierarchy][req:Bottom-up extraction order]") {
  Session s;
  s.load("leaf", kLeaf);
  extract::ExtractContext ctx;
  extract::ExtWriter writer;
  const auto result = extract::extract_cell(ctx, *s.editor.cells.find("leaf"), writer, true);
  REQUIRE_FALSE(result.ok());
  CHECK(result.error().code == "EXTRACT-CMD-001");
}

TEST_CASE("extract_cell stops a cancelled pass and leaves the slots clear",
          "[cap:extract-hierarchy][req:Bottom-up extraction order]") {
  Session s;
  s.load("leaf", kLeaf);
  extract::ExtractContext ctx = context_of(s);
  std::atomic<bool> cancel{true};
  hefesto::mask::Progress progress;
  progress.cancel = &cancel;
  ctx.progress = &progress;
  extract::ExtWriter writer;
  hefesto::db::CellDef& leaf = *s.editor.cells.find("leaf");
  const auto result = extract::extract_cell(ctx, leaf, writer, true);
  REQUIRE(result.ok());
  CHECK(result.value().cancelled);
  CHECK(extract::clients_clear(leaf));
}

TEST_CASE("extract_cell on its own makes and reverts the temporary unique labels",
          "[cap:extract-hierarchy][req:Bottom-up extraction order]") {
  Session s;
  s.load("ex", case_input("EX-024", "ex.mag"));
  extract::ExtractContext ctx = context_of(s);
  extract::ExtWriter writer;
  hefesto::db::CellDef& ex = *s.editor.cells.find("ex");
  const auto result = extract::extract_cell(ctx, ex, writer, true);
  REQUIRE(result.ok());
  auto text = writer.finish();
  REQUIRE(text.ok());
  CHECK(text.value().find("\"net1_uq0\"") != std::string::npos);
  for (const auto& label : ex.labels()) {
    CHECK(label.text.find("_uq") == std::string::npos);
  }
}
