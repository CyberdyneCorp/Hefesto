// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Reading embedded symbol blocks (capability schematic-file-format
// "Embedded symbols"; round-2 spec answer F4; the 64-level input limit of
// Q-SCHEM-32, approved 2026-11-21).
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "file_test_util.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace schem = hefesto::schem;
using schem::test::codes;
using schem::test::read_text;
using schem::test::round_trip;

namespace {

std::size_t count(const std::string& text, std::string_view needle) {
  std::size_t n = 0;
  for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
    ++n;
  }
  return n;
}

std::size_t count_if_code(const std::vector<hefesto::diag::Diagnostic>& warnings, std::string_view code) {
  std::size_t n = 0;
  for (const auto& w : warnings) {
    n += w.code == code ? 1U : 0U;
  }
  return n;
}

// `levels` instances, each followed by `[`, then `levels` closing lines and
// a wire (scenario "Nesting deeper than 64 blocks").
std::string nested_chain(int levels) {
  std::string text;
  for (int i = 0; i < levels; ++i) {
    text += "C {s" + std::to_string(i) + ".sym} 0 0 0 0 {}\n[\n";
  }
  for (int i = 0; i < levels; ++i) {
    text += "]\n";
  }
  return text + "N 2 2 3 3 {}\n";
}

}  // namespace

TEST_CASE("block after a wire belongs to the last instance", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("C {a.sym} 0 0 0 0 {name=x1}\nN 0 0 10 0 {}\n[\nK {type=x}\n]\n");
  REQUIRE(r.doc.instances.size() == 1);
  CHECK(r.doc.instances[0].props == "name=x1\nembed=true");
  REQUIRE(r.doc.embedded.size() == 1);
  CHECK(r.doc.embedded[0].reference == "a.sym");
  CHECK(r.doc.embedded[0].symbol.globals.k == "type=x");
  CHECK(r.doc.wires.size() == 1);
  CHECK(r.warnings.empty());
}

TEST_CASE("block with no instance before it", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("N 0 0 1 1 {}\n[\nK {type=x}\nN 5 5 6 6 {}\n]\nN 2 2 3 3 {}\n");
  CHECK(r.doc.embedded.empty());
  REQUIRE(r.doc.wires.size() == 2);
  CHECK(r.doc.wires[0].p2 == schem::Point{1, 1});
  CHECK(r.doc.wires[1].p1 == schem::Point{2, 2});
  REQUIRE(r.warnings.size() == 1);
  CHECK(r.warnings[0].code == "SCHEM-FILE-004");
}

TEST_CASE("skipped block ends at the first closing line", "[cap:schematic-file-format][req:Embedded symbols]") {
  const std::string text =
      "C {a.sym} 0 0 0 0 {}\n[\nK {type=a}\n]\n"
      "C {a.sym} 10 0 0 0 {}\n[\nC {b.sym} 0 0 0 0 {}\n[\nK {type=b}\n]\nN 7 7 8 8 {}\n]\nN 2 2 3 3 {}\n";
  const auto r = read_text(text);
  REQUIRE(r.doc.embedded.size() == 1);
  CHECK(r.doc.embedded[0].symbol.globals.k == "type=a");
  REQUIRE(r.doc.instances.size() == 2);
  CHECK(r.doc.instances[1].props == "embed=true");  // set although the block is skipped
  REQUIRE(r.doc.wires.size() == 2);
  CHECK(r.doc.wires[0].p1 == schem::Point{7, 7});
  CHECK(r.doc.wires[1].p1 == schem::Point{2, 2});
  CHECK(r.warnings.empty());
}

TEST_CASE("a block of an instance with an empty reference is skipped", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("C {} 0 0 0 0 {}\n[\nK {type=x}\n]\nN 2 2 3 3 {}\n");
  CHECK(r.doc.embedded.empty());
  REQUIRE(r.doc.instances.size() == 1);
  CHECK(r.doc.instances[0].props == "embed=true");
  CHECK(r.doc.wires.size() == 1);
}

TEST_CASE("block cut by the end of the file", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("C {a.sym} 0 0 0 0 {}\n[\nK {type=x}\nL 4 0 0 1 1 {}\n");
  REQUIRE(r.doc.embedded.size() == 1);
  CHECK(r.doc.embedded[0].symbol.lines.size() == 1);
  CHECK(codes(r.warnings) == std::vector<std::string>{"SCHEM-FILE-005"});
  const std::string written = schem::write_document(r.doc);
  CHECK(written.find("C {a.sym} 0 0 0 0 {embed=true}\n[\n") != std::string::npos);
  CHECK(written.find("L 4 0 0 1 1 {}\n]\n") != std::string::npos);
}

TEST_CASE("nested block merged into the outer symbol", "[cap:schematic-file-format][req:Embedded symbols]") {
  const std::string text =
      "C {a.sym} 0 0 0 0 {}\n[\nK {type=a}\nC {b.sym} 100 0 0 0 {}\n[\nK {type=b}\nL 4 0 0 10 0 {}\n"
      "B 5 -2.5 -2.5 2.5 2.5 {name=P}\n]\nL 4 0 0 0 10 {}\n]\n";
  for (const bool compat : {true, false}) {
    const auto r = read_text(text, schem::FileKind::schematic, compat);
    REQUIRE(r.doc.embedded.size() == 1);
    const schem::Document& a = r.doc.embedded[0].symbol;
    CHECK(a.globals.k == "type=a");
    REQUIRE(a.lines.size() == 2);
    CHECK(a.lines[0] == schem::LineRecord{4, {100, 0}, {110, 0}, ""});
    CHECK(a.lines[1] == schem::LineRecord{4, {0, 0}, {0, 10}, ""});
    REQUIRE(a.boxes.size() == 1);
    CHECK(a.boxes[0] == schem::BoxRecord{7, {97.5, -2.5}, {102.5, 2.5}, "name=P"});
    CHECK(a.instances.empty());
    CHECK(r.warnings.empty());
    const std::string written = schem::write_document(r.doc);
    CHECK(count(written, "[\n") == 1);
    CHECK(count(written, "\nC ") == 1);  // only the top-level instance
    CHECK(written.find("B 7 97.5 -2.5 102.5 2.5 {name=P}\n") != std::string::npos);
    CHECK(round_trip(written) == written);
  }
}

TEST_CASE("a nested block is placed with the instance's orientation", "[cap:schematic-file-format][req:Embedded symbols]") {
  const std::string text =
      "C {a.sym} 0 0 0 0 {}\n[\nC {b.sym} 10 20 1 0 {}\n[\nL 4 0 0 5 0 {}\nT {x} 0 0 0 0 0.2 0.2 {}\n"
      "A 4 0 0 3 0 90 {}\nP 4 2 1 0 2 0 {}\n]\n]\n";
  const auto r = read_text(text);
  REQUIRE(r.doc.embedded.size() == 1);
  const schem::Document& a = r.doc.embedded[0].symbol;
  REQUIRE(a.lines.size() == 1);
  CHECK(a.lines[0] == schem::LineRecord{4, {10, 20}, {10, 25}, ""});
  REQUIRE(a.texts.size() == 1);
  CHECK(a.texts[0].origin == schem::Point{10, 20});
  CHECK(a.texts[0].rotation == 1);
  REQUIRE(a.arcs.size() == 1);
  CHECK(a.arcs[0].start_angle == 270);
  REQUIRE(a.polygons.size() == 1);
  CHECK(a.polygons[0].points == std::vector<schem::Point>{{10, 21}, {10, 22}});
}

TEST_CASE("a block drops floaters, graph boxes, ignored objects and other globals",
          "[cap:schematic-file-format][req:Embedded symbols]") {
  const std::string text =
      "C {a.sym} 0 0 0 0 {}\n[\nG {type=g}\nK {type=k}\nG {type=late}\nS {spice}\nV {v}\nE {e}\nF {f}\n"
      "T {@name} 0 0 0 0 0.2 0.2 {name=x1}\nT {f} 0 0 0 0 0.2 0.2 {floater=1}\nT {keep} 0 0 0 0 0.2 0.2 {}\n"
      "B 2 0 0 100 100 {flags=graph,unlocked}\nB 2 0 0 1 1 {}\nL 4 0 0 1 1 {symbol_ignore=true}\n"
      "N 0 0 5 0 {symbol_ignore=1}\nC {c.sym} 0 0 0 0 {symbol_ignore=true}\n]\n";
  const auto r = read_text(text);
  REQUIRE(r.doc.embedded.size() == 1);
  const schem::Document& a = r.doc.embedded[0].symbol;
  CHECK(a.globals == schem::GlobalProperties{"type=k", "", "", "", "", ""});
  REQUIRE(a.texts.size() == 1);
  CHECK(a.texts[0].text == "keep");
  REQUIRE(a.boxes.size() == 1);
  CHECK(a.boxes[0].props.empty());
  CHECK(a.lines.empty());
  CHECK(a.wires.empty());
  CHECK(a.instances.empty());
}

TEST_CASE("G is the block's symbol property when no K comes before it",
          "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("C {a.sym} 0 0 0 0 {}\n[\nG {type=g}\n]\n");
  REQUIRE(r.doc.embedded.size() == 1);
  CHECK(r.doc.embedded[0].symbol.globals.k == "type=g");
  CHECK(schem::write_document(r.doc).find("K {type=g}\nG {}\n") != std::string::npos);
}

TEST_CASE("a nested block of a symbol found as a file is skipped", "[cap:schematic-file-format][req:Embedded symbols]") {
  schem::ReadOptions options;
  options.symbol_file_exists = [](std::string_view reference) { return reference == "b.sym"; };
  const auto read = schem::read_document(
      "C {a.sym} 0 0 0 0 {}\n[\nC {b.sym} 100 0 0 0 {}\n[\nL 4 0 0 10 0 {}\n]\nL 4 0 0 0 10 {}\n]\n", options);
  REQUIRE(read.ok());
  const schem::Document& doc = read.value();
  REQUIRE(doc.embedded.size() == 1);
  const schem::Document& a = doc.embedded[0].symbol;
  REQUIRE(a.instances.size() == 1);  // drawn from the file by the symbol library
  CHECK(a.instances[0].symbol == "b.sym");
  REQUIRE(a.lines.size() == 1);
  CHECK(a.lines[0].p2 == schem::Point{0, 10});
}

TEST_CASE("an instance kept in a nested block is placed through both levels",
          "[cap:schematic-file-format][req:Embedded symbols]") {
  // c.sym is a file: its instance stays, placed as seen from a.sym.
  schem::ReadOptions options;
  options.symbol_file_exists = [](std::string_view reference) { return reference == "c.sym"; };
  const auto read = schem::read_document(
      "C {a.sym} 0 0 0 0 {}\n[\nC {b.sym} 100 0 1 0 {}\n[\nC {c.sym} 10 0 0 1 {}\n]\n]\n", options);
  REQUIRE(read.ok());
  const schem::Document& a = read.value().embedded.at(0).symbol;
  REQUIRE(a.instances.size() == 1);
  CHECK(a.instances[0].origin == schem::Point{100, 10});
  CHECK(a.instances[0].rotation == 1);
  CHECK(a.instances[0].flip == 1);
}

TEST_CASE("nesting deeper than 64 blocks", "[cap:schematic-file-format][req:Embedded symbols]") {
  const std::string text = nested_chain(70);
  std::string written[2];
  for (const bool compat : {true, false}) {
    const auto r = read_text(text, schem::FileKind::schematic, compat);
    CHECK(count_if_code(r.warnings, "SCHEM-FILE-007") == 1);
    CHECK(count_if_code(r.warnings, "SCHEM-FILE-008") == 1);
    REQUIRE(r.doc.embedded.size() == 1);
    // Levels 0 to 39 are read; s39 is the last symbol placed (level 39).
    REQUIRE(r.doc.wires.size() == 1);
    CHECK(r.doc.wires[0].p1 == schem::Point{2, 2});
    written[compat ? 0 : 1] = schem::write_document(r.doc);
  }
  CHECK(written[0] == written[1]);
}

TEST_CASE("the 64-level warning text", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text(nested_chain(70));
  bool found = false;
  for (const auto& w : r.warnings) {
    if (w.code == "SCHEM-FILE-007") {
      CHECK(w.text == "embedded symbol blocks nested deeper than 64 levels; block skipped");
      found = true;
    }
  }
  CHECK(found);
}

TEST_CASE("symbols nested 40 levels in a block stop reading the block",
          "[cap:schematic-file-format][req:Embedded symbols]") {
  // Each level holds one line before its nested instance and one after it.
  std::string text;
  for (int i = 0; i < 45; ++i) {
    text += "C {s" + std::to_string(i) + ".sym} 0 0 0 0 {}\n[\nL 4 0 0 1 0 {}\n";
  }
  for (int i = 0; i < 45; ++i) {
    text += "L 4 0 0 0 1 {}\n]\n";
  }
  const auto r = read_text(text);
  REQUIRE(r.doc.embedded.size() == 1);
  const schem::Document& s0 = r.doc.embedded[0].symbol;
  // 40 lines read before the instance that would open level 40, none after it.
  CHECK(s0.lines.size() == 40);
  REQUIRE(r.warnings.size() == 1);
  CHECK(r.warnings[0].code == "SCHEM-FILE-008");
  CHECK(r.warnings[0].text == "Symbol recursively instantiating symbol: max depth reached, skipping");
}

TEST_CASE("hostile nesting of blocks is read in bounded work", "[cap:schematic-file-format][req:Malformed input]") {
  // Regression for unbounded block nesting: 20000 nested blocks keep at most
  // 64 open; the rest of the file is skipped, not recursed into.
  const auto r = read_text(nested_chain(20000));
  CHECK(r.doc.instances.size() == 1);
  CHECK(r.doc.embedded.size() == 1);
  CHECK(r.doc.wires.size() == 1);
}
