// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format selection, file numbers and re-reads (capability lvs-netlist-input
// "Format selection", "Netlist database model"; tasks.md 6.1). Written from
// the spec text only (netgen clean room, Q-LVS-30).
//
// Owner: LVSREAD.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/lvs/read/codes.hpp"
#include "read_fixture.hpp"

using lvs_test::Reading;
namespace lvs = hefesto::lvs;
namespace codes = hefesto::lvs::read_codes;

namespace {

lvs::Source in_memory(std::string name, std::string text) {
  return lvs::Source{std::move(name), std::move(text)};
}

lvs::Source path(std::string name) {
  return lvs::Source{std::move(name), std::nullopt};
}

}  // namespace

TEST_CASE("two files get distinct numbers", "[cap:lvs-netlist-input][req:Netlist database model]") {
  Reading r;
  const char* inv = ".subckt inv a y\nM1 y a vss vss nfet\n.ends\n";
  REQUIRE(r.read(inv, "layout.spice").ok());
  REQUIRE(r.read(inv, "schem.spice").ok());
  const auto first = r.db.find_cell("inv", 0);
  const auto second = r.db.find_cell("inv", 1);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(*first != *second);
  REQUIRE(r.db.files.size() == 2);
  CHECK(r.db.files[0].number == 0);
  CHECK(r.db.files[1].number == 1);
  CHECK(r.db.next_file == 2);
}

TEST_CASE("suffix detection", "[cap:lvs-netlist-input][req:Format selection]") {
  CHECK(lvs::detect_format(path("top.cir")).value() == lvs::Format::spice);
  CHECK(lvs::detect_format(path("top.ckt")).value() == lvs::Format::spice);
  CHECK(lvs::detect_format(path("top.spice")).value() == lvs::Format::spice);
  CHECK(lvs::detect_format(path("top.spi")).value() == lvs::Format::spice);
  CHECK(lvs::detect_format(path("top.fspc")).value() == lvs::Format::spice);
  CHECK(lvs::detect_format(path("top.v")).value() == lvs::Format::verilog);
  CHECK(lvs::detect_format(path("top.sv")).value() == lvs::Format::verilog);
  CHECK(lvs::detect_format(path("top.sim")).value() == lvs::Format::sim);
  CHECK(lvs::detect_format(path("top.ext")).value() == lvs::Format::ext);
  const auto ntk = lvs::detect_format(path("top.ntk"));
  REQUIRE_FALSE(ntk.ok());
  CHECK(ntk.error().code == codes::kBadFormat);
}

TEST_CASE("first-character detection", "[cap:lvs-netlist-input][req:Format selection]") {
  CHECK(lvs::detect_format(in_memory("netlist", "* layout\nR1 a b 1k\n")).value() == lvs::Format::spice);
  CHECK(lvs::detect_format(in_memory("netlist", "| units: 100\n")).value() == lvs::Format::sim);
  const auto unknown = lvs::detect_format(in_memory("netlist", "R1 a b 1k\n"));
  REQUIRE_FALSE(unknown.ok());
  CHECK(unknown.error().code == codes::kUnknownType);
  CHECK(unknown.error().text == "ReadNetlist: don't know type of file 'netlist'");

  Reading r;
  r.add_file("/w/netlist", "* layout\n");
  const auto detected = lvs::resolve_source(path("/w/netlist"), r.files);
  REQUIRE(detected.ok());
  CHECK(detected.value().format == lvs::Format::spice);
  CHECK(detected.value().source.name == "/w/netlist");
}

TEST_CASE("appended suffixes are tried in order", "[cap:lvs-netlist-input][req:Format selection]") {
  Reading r;
  r.add_file("/w/top.spice", "* a\n");
  r.add_file("/w/top.v", "module top; endmodule\n");
  r.add_file("/w/top", "* plain\n");
  const auto detected = lvs::resolve_source(path("/w/top"), r.files);
  REQUIRE(detected.ok());
  CHECK(detected.value().source.name == "/w/top.spice");  // .spice comes before .v
  CHECK(detected.value().format == lvs::Format::spice);
}

TEST_CASE("a missing file is reported", "[cap:lvs-netlist-input][req:Format selection]") {
  Reading r;
  for (const char* name : {"/w/none", "/w/none.spice"}) {
    const auto detected = lvs::resolve_source(path(name), r.files);
    REQUIRE_FALSE(detected.ok());
    CHECK(detected.error().code == codes::kFileNotFound);
    CHECK(detected.error().text == std::string{"ReadNetlist: unable to find file '"} + name + "'");
  }
  const auto read = r.read_file("/w/none.spice");
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().code == codes::kFileNotFound);
}

TEST_CASE("re-read returns same number", "[cap:lvs-netlist-input][req:Format selection]") {
  Reading r;
  r.add_file("a.spice", "R1 x y 1k\n");
  const auto first = r.read_file("a.spice");
  REQUIRE(first.ok());
  const auto cells = r.db.cells.size();
  REQUIRE(r.files.write_atomic("a.spice", "R1 x y 1k\nR2 x y 2k\n").ok());
  const auto again = r.read_file("a.spice");
  REQUIRE(again.ok());
  CHECK(again.value() == first.value());
  CHECK(r.db.cells.at(again.value()).file == 0);
  CHECK(r.db.cells.size() == cells);  // nothing re-parsed
  CHECK(r.db.cells.at(first.value()).instances.size() == 1);
}

TEST_CASE("explicit file numbers", "[cap:lvs-netlist-input][req:Format selection]") {
  Reading r;
  r.options.file_number = 5;
  const auto top = r.read("R1 x y 1k\n", "a.spice");
  REQUIRE(top.ok());
  CHECK(r.db.cells.at(top.value()).file == 5);
  CHECK(r.db.next_file == 6);

  r.options.file_number = 6;
  const auto other = r.read("R1 x y 1k\n", "a.spice");
  REQUIRE_FALSE(other.ok());
  CHECK(other.error().code == codes::kFileNumber);
  CHECK(other.error().text == "File is already loaded as a different file number.");

  r.options.file_number = -1;
  const auto negative = r.read("R1 x y 1k\n", "b.spice");
  REQUIRE_FALSE(negative.ok());
  CHECK(negative.error().code == codes::kFileNumber);
}

TEST_CASE("format keywords by unique prefix", "[cap:lvs-netlist-input][req:Format selection]") {
  CHECK(lvs::parse_format_keyword("spice").value() == lvs::Format::spice);
  CHECK(lvs::parse_format_keyword("sp").value() == lvs::Format::spice);
  CHECK(lvs::parse_format_keyword("ver").value() == lvs::Format::verilog);
  CHECK(lvs::parse_format_keyword("ext").value() == lvs::Format::ext);  // exact beats extflat
  CHECK(lvs::parse_format_keyword("extf").value() == lvs::Format::ext);
  CHECK_FALSE(lvs::parse_format_keyword("au").value().has_value());  // automatic
  for (const char* bad : {"s", "a", "e", "", "spicey", "foo"}) {
    const auto parsed = lvs::parse_format_keyword(bad);
    REQUIRE_FALSE(parsed.ok());
    CHECK(parsed.error().code == codes::kBadFormat);
  }
  const auto ntk = lvs::parse_format_keyword("ntk");
  REQUIRE_FALSE(ntk.ok());
  CHECK(ntk.error().code == codes::kBadFormat);
}

TEST_CASE("Verilog, .sim and .ext reads are not implemented yet", "[cap:lvs-netlist-input][req:Format selection]") {
  // tasks.md 6.3 (a later round): the formats are selected but not read.
  Reading r;
  for (const auto format : {lvs::Format::verilog, lvs::Format::sim, lvs::Format::ext}) {
    const auto read = lvs::read_netlist(r.db, in_memory("top.v", "module top; endmodule\n"), format, r.options);
    REQUIRE_FALSE(read.ok());
    CHECK(read.error().code == "CORE-NOTIMPL-009");
  }
  CHECK(r.db.cells.empty());
}
