// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtWriter and format_value (capability extract-file-format;
// openspec/changes/m7-extraction tasks.md 5.5). Every file emitted here is
// read back by extflat's reader with no message. Owner: EXTNODE.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::test::extfile {
namespace {

namespace extract = hefesto::extract;

extract::ExtHeader small_header(std::vector<std::int64_t> classes = {80}) {
  extract::ExtHeader h;
  h.timestamp = 1700000000;
  h.tech = "scmos";
  h.style = "default";
  h.rscale = 1000;
  h.cscale = 1000;
  h.lscale = 100;
  h.resist_classes = std::move(classes);
  return h;
}

std::string finish(extract::ExtWriter& w) {
  auto text = w.finish();
  REQUIRE(text.ok());
  return text.value();
}

/// Reads `files` (cell name -> text) with extflat's reader from `root`;
/// returns the messages.
std::vector<diag::Diagnostic> read_back(const std::vector<std::pair<std::string, std::string>>& files,
                                        std::string_view root) {
  platform::MemoryFileSystem fs;
  for (const auto& [cell, text] : files) {
    REQUIRE(fs.write_atomic("/w/" + cell + ".ext", text).ok());
  }
  cell::SearchPaths paths{{"/w"}, {}, {}};
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{fs, paths, vars, sink};
  auto read = extflat::read(ctx, root, extflat::ReadOptions{});
  REQUIRE(read.ok());
  return sink.take();
}

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    out.push_back(line);
  }
  return out;
}

TEST_CASE("Capacitance values use %lg in the C locale", "[cap:extract-file-format][req:Lexical conventions]") {
  CHECK(extract::format_value(1760.0) == "1760");
  CHECK(extract::format_value(0.000012345) == "1.2345e-05");
  CHECK(extract::format_value(1234567.0) == "1.23457e+06");
  CHECK(extract::format_value(0.0) == "0");
  CHECK(extract::format_value(-0.52) == "-0.52");
  CHECK(extract::format_value(0.0001) == "0.0001");
  CHECK(extract::format_value(100000.0) == "100000");
  CHECK(extract::format_value(1e6) == "1e+06");
  CHECK(extract::format_value(1.0 / 3.0) == "0.333333");
}

TEST_CASE("Records are single-space separated lines with quoted names",
          "[cap:extract-file-format][req:Lexical conventions]") {
  extract::ExtWriter w;
  w.header(small_header());
  const std::array<std::int64_t, 2> pa{12, 14};
  w.node("IN", 0, 0.52, {0, -2}, "metal1", pa);
  w.attr("IN", {2, 2, 2, 2}, "metal1", "cap=10f");
  const std::string text = finish(w);
  CHECK(text.ends_with("node \"IN\" 0 0.52 0 -2 metal1 12 14\nattr IN 2 2 2 2 metal1 \"cap=10f\"\n"));
  for (const std::string& line : lines(text)) {
    CHECK_FALSE(line.starts_with(" "));
    CHECK_FALSE(line.ends_with(" "));
    CHECK(line.find("  ") == std::string::npos);
  }
  CHECK(read_back({{"c", text}}, "c").empty());
}

TEST_CASE("One emitter per record of the grammar", "[cap:extract-file-format][req:Record grammar]") {
  extract::ExtWriter w;
  w.header(small_header());
  const std::array<std::int32_t, 6> identity{1, 0, 0, 0, 1, 0};
  w.use("bit", "A[0:3:10][0:1:20]", identity);
  w.port("OUT", 1, {40, 2, 42, 4}, "metal1");
  const std::array<std::int64_t, 2> zero{0, 0};
  w.substrate("VSUB", 0, {db::kMinusInfinity + 3, db::kMinusInfinity + 3}, "space", zero);
  w.equiv("OUT", "out_int");
  const std::vector<std::string> params{"l=2", "w=4"};
  const std::vector<extract::ExtTerminal> terms{{"IN", 4, "0"}, {"VSUB", 4, "12,14"}, {"OUT", 4, "12,14"}};
  w.device("msubckt", "nfet_01v8", {0, 0, 1, 1}, params, "VSUB", terms);
  w.merge("A[0]/x", "OUT", -0.5, zero);
  w.merge_more("A[1]/x", "OUT");
  w.distance("a", "b", 3, 7);
  const std::vector<std::string> out = lines(finish(w));
  CHECK(out[6] == "use bit A[0:3:10][0:1:20] 1 0 0 0 1 0");
  CHECK(out[7] == "port \"OUT\" 1 40 2 42 4 metal1");
  CHECK(out[8] == "substrate \"VSUB\" 0 0 -1073741817 -1073741817 space 0 0");
  CHECK(out[9] == "equiv \"OUT\" \"out_int\"");
  CHECK(out[10] == "device msubckt nfet_01v8 0 0 1 1 l=2 w=4 \"VSUB\" \"IN\" 4 0 \"VSUB\" 4 12,14 \"OUT\" 4 12,14");
  CHECK(out[11] == "merge \"A[0]/x\" \"OUT\" -0.5 0 0");
  CHECK(out[12] == "merge \"A[1]/x\" \"OUT\"");
  CHECK(out[13] == "distance a b 3 7");

  extract::ExtWriter fet;
  fet.header(small_header());
  const std::vector<std::string> dims{"12", "14"};
  const std::vector<extract::ExtTerminal> fterms{{"g", 2, "0"}, {"s", 2, "0"}, {"d", 2, "0"}};
  fet.device("fet", "nfet", {0, 0, 2, 2}, dims, "GND!", fterms);
  CHECK(lines(finish(fet)).back() == "fet nfet 0 0 2 2 12 14 \"GND!\" \"g\" 2 0 \"s\" 2 0 \"d\" 2 0");
}

TEST_CASE("Header of a default style", "[cap:extract-file-format][req:Header records]") {
  extract::ExtWriter w;
  extract::ExtHeader h = small_header();
  w.header(h);
  CHECK(lines(finish(w)) == std::vector<std::string>{"timestamp 1700000000", "version 8.3", "tech scmos",
                                                     "style default", "scale 1000 1000 100", "resistclasses 80"});
  // Q-EXTRACT-10: version 8.3; a fractional units-per-lambda is %g.
  h.units_per_lambda = 0.5;
  h.cell_name = "inv";
  h.cell_parameter = "w=1";
  w.header(h);
  const std::vector<std::string> out = lines(finish(w));
  CHECK(out[1] == "version 8.3");
  CHECK(out[4] == "scale 1000 1000 0.5");
  CHECK(out[6] == "parameters :inv w=1");
}

TEST_CASE("A rotated instance writes its transform coefficients", "[cap:extract-file-format][req:Use records]") {
  extract::ExtWriter w;
  w.header(small_header());
  const std::array<std::int32_t, 6> rot90{0, -1, 100, 1, 0, 50};
  w.use("buf", "I3", rot90);
  const std::string text = finish(w);
  CHECK(lines(text).back() == "use buf I3 0 -1 100 1 0 50");
  extract::ExtWriter child;
  child.header(small_header());
  CHECK(read_back({{"top", text}, {"buf", finish(child)}}, "top").empty());
}

TEST_CASE("Record classes come out in record order whatever the call order",
          "[cap:extract-file-format][req:Record order]") {
  extract::ExtWriter w;
  const std::array<std::int64_t, 2> pa{1, 2};
  w.distance("a", "b", 1, 2);
  w.merge("x/a", "n2", 0.1, pa, extract::MergeClass::array);
  w.cap("x/a", "n1", 0.5, extract::CapClass::array);
  w.merge("y/a", "n2", 0.1, pa);
  w.cap("y/a", "n1", 0.5, extract::CapClass::subcell);
  const std::vector<std::string> none;
  const std::vector<extract::ExtTerminal> terms{{"n1", 1, "0"}};
  w.device("diode", "d", {0, 0, 1, 1}, none, "", terms);
  w.cap("n2", "n1", 0.25);
  w.node("n2", 0, 0, {5, 5}, "m1", pa);
  w.port("n1", 1, {0, 0, 1, 1}, "m1");
  w.node("n1", 0, 0, {0, 0}, "m1", pa);
  w.abstract();
  w.parameters("d", none);
  w.cell_parameters("c", "primitive");
  const std::array<std::int32_t, 6> identity{1, 0, 0, 0, 1, 0};
  w.use("leaf", "u", identity);
  w.header(small_header());
  const std::vector<std::string> out = lines(finish(w));
  const std::vector<std::string> keys{"timestamp", "version",    "tech",   "style", "scale", "resistclasses",
                                      "use",       "parameters", "parameters", "abstract", "port", "node",
                                      "node",      "cap",        "device", "cap",   "merge", "cap",
                                      "merge",     "distance"};
  REQUIRE(out.size() == keys.size());
  for (std::size_t i = 0; i < keys.size(); ++i) {
    CHECK((out[i] == keys[i] || out[i].starts_with(keys[i] + " ")));
  }
  CHECK(out[7] == "parameters :c primitive");
  CHECK(out[11] == "node \"n2\" 0 0 5 5 m1 1 2");  // node records keep the call order
  CHECK(out[13] == "cap \"n1\" \"n2\" 0.25");       // Q-EXTRACT-7: the smaller name first
  CHECK(out[15] == "cap \"n1\" \"y/a\" 0.5");
  CHECK(out[17] == "cap \"n1\" \"x/a\" 0.5");
}

TEST_CASE("Q-EXTRACT-7: cap records are sorted by terminal names within their class",
          "[cap:extract-file-format][req:Record order]") {
  extract::ExtWriter w;
  w.header(small_header({}));
  w.cap("b", "a", 1.0);
  w.cap("c", "a", 2.0);
  w.cap("a", "B", 3.0);
  w.merge("z", "y", 0.0, {});
  w.merge("m", "n", 0.0, {});  // merges keep the caller's (hash) order
  const std::vector<std::string> out = lines(finish(w));
  CHECK(out[6] == "cap \"B\" \"a\" 3");
  CHECK(out[7] == "cap \"a\" \"b\" 1");
  CHECK(out[8] == "cap \"a\" \"c\" 2");
  CHECK(out[9] == "merge \"z\" \"y\" 0");
  CHECK(out[10] == "merge \"m\" \"n\" 0");
}

TEST_CASE("Area and perimeter lists hold one pair per resistance class",
          "[cap:extract-file-format][req:Reader constraints honored by the writer]") {
  extract::ExtWriter w;
  w.header(small_header({80, 120}));
  const std::array<std::int64_t, 2> one_pair{1, 2};
  w.node("n", 0, 0, {0, 0}, "m1", one_pair);
  auto text = w.finish();
  REQUIRE_FALSE(text.ok());
  CHECK(text.error().code == "EXTRACT-FILE-001");

  extract::ExtWriter empty;
  auto none = empty.finish();
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().code == "EXTRACT-FILE-002");

  // A `Short` device is still an ordinary device record.
  extract::ExtWriter shorts;
  shorts.header(small_header({}));
  shorts.node("a", 0, 0, {0, 0}, "m1", {});
  shorts.node("b", 0, 0, {5, 0}, "m1", {});
  const std::vector<std::string> none_params{"0", "0"};
  const std::vector<extract::ExtTerminal> terms{{"a", 1, "0"}, {"b", 1, "0"}};
  shorts.device("devres", "Short", {0, 0, 1, 1}, none_params, "", terms);
  const std::string out = finish(shorts);
  CHECK(lines(out).back() == "device devres Short 0 0 1 1 0 0 \"a\" 1 0 \"b\" 1 0");
  CHECK(read_back({{"s", out}}, "s").empty());
}

TEST_CASE("The annotated example round-trips through the writer and the reader",
          "[cap:extract-file-format][req:Annotated example]") {
  std::ifstream in(std::string{HEFESTO_TEST_DATA_DIR} + "/annotated_example.ext", std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream fixture;
  fixture << in.rdbuf();

  extract::ExtWriter w;
  extract::ExtHeader h;
  h.timestamp = 1730800000;
  h.tech = "scmos";
  h.style = "default";
  h.rscale = 1000;
  h.cscale = 1000;
  h.lscale = 100;
  h.resist_classes = {80};
  w.header(h);
  const std::array<std::int32_t, 6> shift{1, 0, 20, 0, 1, 0};
  w.use("nand2", "x1", shift);
  w.parameters("nfet_01v8", std::vector<std::string>{"l=l", "w=w"});
  w.port("OUT", 1, {40, 2, 42, 4}, "metal1");
  w.node("OUT", 0, 1.76, {40, 2}, "metal1", std::array<std::int64_t, 2>{40, 28});
  w.attr("OUT", {2, 2, 2, 2}, "metal1", "cap=10f");
  w.equiv("OUT", "out_int");
  w.node("IN", 0, 0.52, {0, -2}, "metal1", std::array<std::int64_t, 2>{12, 14});
  w.substrate("VSUB", 0, {db::kMinusInfinity + 3, db::kMinusInfinity + 3}, "space",
              std::array<std::int64_t, 2>{0, 0});
  w.cap("OUT", "IN", 0.2);
  const std::vector<extract::ExtTerminal> terms{{"IN", 4, "0"}, {"VSUB", 4, "12,14"}, {"OUT", 4, "12,14"}};
  w.device("msubckt", "nfet_01v8", {0, 0, 1, 1}, std::vector<std::string>{"l=2", "w=4"}, "VSUB", terms);
  w.cap("x1/A", "OUT", 0.015, extract::CapClass::subcell);
  w.merge("x1/A", "IN", -0.52, std::array<std::int64_t, 2>{-12, -14});
  w.merge("x1/VSUB", "VSUB", 0, std::array<std::int64_t, 2>{0, 0});
  const std::string written = finish(w);

  // The writer differs from the example only in Q-EXTRACT-7's cap order.
  std::string canonical = fixture.str();
  const auto swap_line = [&](const std::string& from, const std::string& to) {
    const std::size_t at = canonical.find(from);
    REQUIRE(at != std::string::npos);
    canonical.replace(at, from.size(), to);
  };
  swap_line("cap \"OUT\" \"IN\" 0.2", "cap \"IN\" \"OUT\" 0.2");
  swap_line("cap \"x1/A\" \"OUT\" 0.015", "cap \"OUT\" \"x1/A\" 0.015");
  CHECK(written == canonical);

  extract::ExtWriter child;
  child.header(h);
  child.node("A", 0, 0, {0, 0}, "metal1", std::array<std::int64_t, 2>{0, 0});
  child.substrate("VSUB", 0, {db::kMinusInfinity + 3, db::kMinusInfinity + 3}, "space",
                  std::array<std::int64_t, 2>{0, 0});
  const std::string leaf = finish(child);
  CHECK(read_back({{"top", fixture.str()}, {"nand2", leaf}}, "top").empty());
  CHECK(read_back({{"top", written}, {"nand2", leaf}}, "top").empty());
}

}  // namespace
}  // namespace hefesto::test::extfile
