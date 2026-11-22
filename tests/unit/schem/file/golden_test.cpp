// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SCHEM-RT golden cases replayed through the reader and writer
// (tests/golden/SCHEM/cases/SCHEM-RT-*; capability schematic-file-format;
// openspec/changes/m8-schematic tasks.md 6.1-6.4). Each case's expected
// files were written by xschem 3.4.8RC in CI; Hefesto runs with
// xschem-compat ON as the golden harness does.
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "file_test_util.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "reference_data.hpp"

namespace schem = hefesto::schem;
using schem::test::golden;
using schem::test::read_text;
using schem::test::round_trip;
using schem::test::slurp;

namespace {

// xschem's second save of each case equals its first: the saved file is a
// fixpoint of Hefesto's read and write as well.
void check_fixpoint(std::string_view id, std::string_view first, std::string_view second, schem::FileKind kind) {
  const std::string saved = slurp(golden(id, first));
  CHECK(round_trip(saved, kind) == saved);
  CHECK(slurp(golden(id, second)) == saved);
}

}  // namespace

TEST_CASE("SCHEM-RT-01 not.sch is a fixpoint",
          "[cap:schematic-file-format][req:Round-trip guarantee][req:Canonical write order]") {
  check_fixpoint("SCHEM-RT-01", "expected/not.sch", "expected/2-not.sch", schem::FileKind::schematic);
  const auto tree = hefesto::test::pinned_xschem(hefesto::test::kSky130A);
  if (!tree) {
    SKIP("sky130-xschem not in .cache/pdks/xschem");
  }
  CHECK(round_trip(slurp(*tree / "sky130_tests" / "not.sch")) == slurp(golden("SCHEM-RT-01", "expected/not.sch")));
}

TEST_CASE("SCHEM-RT-03 CR-LF file with unordered endpoints",
          "[cap:schematic-file-format][req:File kinds and character handling]"
          "[req:Line, box, arc and polygon records][req:Numeric fields]") {
  const auto r = read_text(slurp(golden("SCHEM-RT-03", "input/crlf.sch")));
  CHECK(r.warnings.empty());
  CHECK(schem::write_document(r.doc) == slurp(golden("SCHEM-RT-03", "expected/crlf.sch")));
  check_fixpoint("SCHEM-RT-03", "expected/crlf.sch", "expected/2-crlf.sch", schem::FileKind::schematic);
}

TEST_CASE("SCHEM-RT-04 pre-1.2 symbol", "[cap:schematic-file-format][req:Global property records]") {
  CHECK(round_trip(slurp(golden("SCHEM-RT-04", "input/old.sym")), schem::FileKind::symbol) ==
        slurp(golden("SCHEM-RT-04", "expected/old.sym")));
  check_fixpoint("SCHEM-RT-04", "expected/old.sym", "expected/2-old.sym", schem::FileKind::symbol);
}

TEST_CASE("SCHEM-RT-05 embedded symbols", "[cap:schematic-file-format][req:Embedded symbols]") {
  check_fixpoint("SCHEM-RT-05", "expected/embed.sch", "expected/2-embed.sch", schem::FileKind::schematic);
  const std::string expected = slurp(golden("SCHEM-RT-05", "expected/embed.sch"));

  // The input carries the block of shape.sym; the block of block.sch (a
  // schematic used as a symbol) is built by the symbol library, so the
  // reader and writer alone give the expected file without it.
  const std::string x3 = "C {block.sch} 0 200 0 0 {name=x3 embed=true}\n";
  const auto block_begin = expected.find(x3) + x3.size();
  const auto block_end = expected.find("]\n", block_begin) + 2;
  std::string without_block = expected;
  without_block.erase(block_begin, block_end - block_begin);
  const auto input = read_text(slurp(golden("SCHEM-RT-05", "input/embed.sch")));
  CHECK(schem::write_document(input.doc) == without_block);

  // The wires of block.sch, given to the block as wires, come out as the
  // expected `L 1` records.
  auto doc = input.doc;
  schem::EmbeddedSymbol block;
  block.reference = "block.sch";
  block.symbol = read_text(slurp(golden("SCHEM-RT-05", "input/block.sch"))).doc;
  block.symbol.instances.clear();
  doc.embedded.push_back(block);
  const std::string written = schem::write_document(doc);
  for (const char* line : {"L 1 0 0 100 0 {lab=IN}\n", "L 1 100 -30 100 0 {}\n", "L 1 100 30 100 60 {lab=OUT}\n",
                           "L 1 100 60 200 60 {}\n", "L 1 200 0 200 60 {lab=OUT}\n"}) {
    CHECK(written.find(line) != std::string::npos);
    CHECK(expected.find(line) != std::string::npos);
  }
}

TEST_CASE("SCHEM-RT-06 malformed file", "[cap:schematic-file-format][req:Record grammar][req:Malformed input][req:Layers]") {
  const auto r = read_text(slurp(golden("SCHEM-RT-06", "input/bad.sch")));
  CHECK(r.doc.wires.size() == 1);  // stdout.txt: "wires 1"
  CHECK(r.doc.instances.size() == 1);
  CHECK(schem::write_document(r.doc) == slurp(golden("SCHEM-RT-06", "expected/bad.sch")));
  // stderr.txt, in order: the truncated wire, the layer, the brace.
  REQUIRE(r.warnings.size() == 3);
  CHECK(r.warnings[0].text == "missing fields for WIRE object, ignoring");
  CHECK(r.warnings[1].text == "wrong or missing layer number for LINE object, ignoring");
  CHECK(r.warnings[2].text == "EOF reached, malformed {...} string input, missing close brace");
}

TEST_CASE("SCHEM-RT-07 symbol with a license header", "[cap:schematic-file-format][req:Version record and header text]") {
  check_fixpoint("SCHEM-RT-07", "expected/nfet_01v8.sym", "expected/2-nfet_01v8.sym", schem::FileKind::symbol);
  const auto tree = hefesto::test::pinned_xschem(hefesto::test::kSky130A);
  if (!tree) {
    SKIP("sky130-xschem not in .cache/pdks/xschem");
  }
  CHECK(round_trip(slurp(*tree / "sky130_fd_pr" / "nfet_01v8.sym"), schem::FileKind::symbol) ==
        slurp(golden("SCHEM-RT-07", "expected/nfet_01v8.sym")));
}

TEST_CASE("SCHEM-RT-08 files with no version record", "[cap:schematic-file-format][req:Version record and header text]") {
  CHECK(round_trip(slurp(golden("SCHEM-RT-08", "input/a.sch"))) == slurp(golden("SCHEM-RT-08", "expected/a.sch")));
  check_fixpoint("SCHEM-RT-08", "expected/a.sch", "expected/2-a.sch", schem::FileKind::schematic);
  check_fixpoint("SCHEM-RT-08", "expected/b.sch", "expected/2-b.sch", schem::FileKind::schematic);
  // (b) starts with the instance: both modes write `C {res.sym}`, since a
  // file version 1.0 document writes references with `.sym` (round-2 answer
  // F1); only the reference stored in the model differs.
  for (const bool compat : {true, false}) {
    CHECK(round_trip(slurp(golden("SCHEM-RT-08", "input/b.sch")), schem::FileKind::schematic, compat) ==
          slurp(golden("SCHEM-RT-08", "expected/b.sch")));
    const auto second = round_trip(slurp(golden("SCHEM-RT-08", "expected/b.sch")), schem::FileKind::schematic, compat);
    CHECK(second == slurp(golden("SCHEM-RT-08", "expected/2-b.sch")));
  }
}
