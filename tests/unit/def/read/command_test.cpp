// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read: the `def read` command, its context and the DEF file lookup
// (tasks.md 7.1; Q-LEFDEF-86, 88). The command is reached as `defr` here;
// the shared `lef`/`def` dispatcher calls the same def::read_command once
// INTEGRATION wires it in.
//
// Owner: DEFR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include "def_fixture.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/view/window.hpp"

namespace def_read_test {
namespace {

def::DefReadOptions parse(std::vector<std::string> args) {
  return def::parse_read_arguments(args);
}

// --- def read command ---------------------------------------------------------------

TEST_CASE("option prefixes", "[cap:def-read][req:def read command]") {
  const def::DefReadOptions o = parse({"-lab", "-noblockageXYZ", "-foo", "extra", "-import"});
  CHECK_FALSE(o.labels);
  CHECK(o.no_blockage);
  CHECK_FALSE(o.annotate);
  CHECK(parse({"-labels"}).labels);
}

TEST_CASE("annotate implies labels", "[cap:def-read][req:def read command]") {
  const def::DefReadOptions o = parse({"-annotate"});
  CHECK(o.annotate);
  CHECK(o.labels);
  CHECK(parse({"-anno"}).annotate);
}

TEST_CASE("without a file name the options are printed", "[cap:def-read][req:def read command]") {
  Session s;
  REQUIRE(s.run("defr read").ok());
  const auto printed = s.printed.take();
  REQUIRE(printed.size() == 1);
  CHECK(printed.front().text.starts_with("The \"defr\" options are:\n    defr read [filename]\t\tread a DEF file"));
  CHECK(printed.front().text.ends_with("    defr help                   \tprint this help information"));
}

TEST_CASE("the command passes its options and sets no result", "[cap:def-read][req:def read command]") {
  // Regression (round-3 golden capture, LEFDEF-211): Magic's CmdLef sets no
  // Tcl result for `def read`; the command returned the design cell's name.
  Session s;
  s.file("/d/top.def", "DESIGN top ;\nNETS 1 ;\n- n1 + ROUTED met1 ( 0 0 ) ( 100 0 ) ;\nEND NETS\nEND DESIGN\n");
  const auto result = s.run("defr read /d/top -labels");
  REQUIRE(result.ok());
  CHECK(result.value == cmd::Value{});
  CHECK(s.label("top", "n1") != nullptr);
}

TEST_CASE("regression: the read's first line comes before the masters' messages",
          "[cap:def-read][req:def read command]") {
  // LEFDEF-211/214 (round-3 capture): Magic prints "Reading DEF data from
  // file ..." before "Cell blk read from path ..." of a COMPONENTS master
  // read from disk; Hefesto printed it after, with the read's other lines.
  Session s;
  s.file("/d/blk.mag", "magic\ntech def\ntimestamp 0\n<< end >>\n");
  s.file("/d/top.def", "DESIGN top ;\nCOMPONENTS 1 ;\n- u1 blk + PLACED ( 0 0 ) N ;\nEND COMPONENTS\nEND DESIGN\n");
  s.editor.search_paths.path = {"/d"};
  REQUIRE(s.run("defr read /d/top").ok());
  std::vector<std::string> texts;
  for (const auto& d : s.printed.take()) {
    texts.push_back(d.text);
  }
  const auto reading = std::find(texts.begin(), texts.end(), "Reading DEF data from file /d/top.def.");
  const auto master = std::find_if(texts.begin(), texts.end(),
                                   [](const std::string& t) { return t.starts_with("Cell blk read from path"); });
  REQUIRE(reading != texts.end());
  REQUIRE(master != texts.end());
  CHECK(reading < master);
}

// --- Command context (Q-LEFDEF-86, 88) ----------------------------------------------

TEST_CASE("no window context", "[cap:def-read][req:Command context]") {
  Session s;
  REQUIRE(s.editor.windows.size() == 0);
  s.file("/d/top.def", "DESIGN top ;\nDIEAREA ( 0 0 ) ( 10 10 ) ;\nEND DESIGN\n");
  REQUIRE(s.run("defr read /d/top").ok());
  CHECK(s.editor.cells.find("top") != nullptr);
}

TEST_CASE("the design is loaded into the only window", "[cap:def-read][req:Command context]") {
  Session s;
  REQUIRE(s.run("load scratch").ok());
  db::CellDef* scratch = s.editor.cells.find("scratch");
  REQUIRE(scratch != nullptr);
  REQUIRE(s.editor.windows.open(scratch, hefesto::view::ScreenRect{0, 0, 499, 499}).ok());
  s.file("/d/top.def", "DESIGN top ;\nDIEAREA ( 0 0 ) ( 10 10 ) ;\nEND DESIGN\n");
  REQUIRE(s.run("defr read /d/top").ok());
  CHECK(s.editor.windows.current()->root() == &s.cell("top"));
}

TEST_CASE("the read reports progress through the host and can be cancelled", "[cap:def-read][req:Command context]") {
  Session s;
  s.file("/d/top.def", "DESIGN top ;\nDIEAREA ( 0 0 ) ( 10 10 ) ;\nEND DESIGN\n");
  std::vector<std::string> labels;
  int reports = 0;
  std::atomic<bool> cancel{false};
  s.editor.mask.read_host = [&](std::string_view label, const hefesto::mask::ProgressWork& work) {
    labels.emplace_back(label);
    hefesto::mask::Progress progress;
    progress.report = [&](double) { ++reports; };
    progress.cancel = &cancel;
    return work(&progress);
  };
  REQUIRE(s.run("defr read /d/top").ok());
  CHECK(labels == std::vector<std::string>{"top.def"});
  CHECK(reports > 0);
  CHECK(s.editor.cells.find("top") != nullptr);
  cancel = true;
  s.file("/d/other.def", "DESIGN other ;\nEND DESIGN\n");
  REQUIRE(s.run("defr read /d/other").ok());
  CHECK(s.editor.cells.find("other") == nullptr);
}

// --- DEF file lookup ----------------------------------------------------------------

TEST_CASE("unusual extension", "[cap:def-read][req:DEF file lookup]") {
  Session s;
  s.file("/d/chip.txt", "DESIGN fromtxt ;\nEND DESIGN\n");
  s.file("/d/chip.def", "DESIGN fromdef ;\nEND DESIGN\n");
  REQUIRE(s.run("defr read /d/chip.txt").ok());
  CHECK(s.editor.cells.find("fromtxt") != nullptr);
  CHECK(s.editor.cells.find("fromdef") == nullptr);
  s.file("/d/only.def", "DESIGN fromonly ;\nEND DESIGN\n");
  REQUIRE(s.run("defr read /d/only.lst").ok());
  CHECK(s.editor.cells.find("fromonly") != nullptr);
}

TEST_CASE("no extension", "[cap:def-read][req:DEF file lookup]") {
  Session s;
  s.file("/d/chip", "DESIGN bare ;\nEND DESIGN\n");
  const auto result = s.run("defr read /d/chip");
  REQUIRE_FALSE(result.ok());
  REQUIRE(!result.diagnostics.empty());
  CHECK(result.diagnostics.front().text == "Cannot open input file /d/chip.def (No such file or directory).");
  CHECK(s.editor.cells.find("bare") == nullptr);
}

TEST_CASE("a .def name and the messages of the read", "[cap:def-read][req:DEF file lookup]") {
  Session s;
  s.file("/d/top.def", "DESIGN top ;\nEND DESIGN\n");
  const auto result = s.run("defr read /d/top.def");
  REQUIRE(result.ok());
  const auto printed = s.printed.take();
  REQUIRE(!printed.empty());
  CHECK(printed.front().text == "Reading DEF data from file /d/top.def.");
  for (const auto& d : printed) {
    CHECK(d.text != "This action cannot be undone.");
  }
}

}  // namespace
}  // namespace def_read_test
