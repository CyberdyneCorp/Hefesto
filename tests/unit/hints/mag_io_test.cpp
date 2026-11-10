// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-hints "Mask hints in .mag files": writing, reading with any type
// keyword (Q-MISC-10), `plane` per magic-compat (Q-MISC-12), read scaling,
// value errors and the scale reducer (MISC-MH-01/02/09 at unit level).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "cell_fixture.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/hints/hints.hpp"

namespace db = hefesto::db;
namespace hints = hefesto::hints;
namespace ct = hefesto::cell::test;
using db::Rect;
using Rects = std::vector<Rect>;

namespace {

#define MAG "[cap:mask-hints][req:Mask hints in .mag files]"

std::string cell_text(std::string_view properties, std::string_view header = "") {
  return "magic\ntech tiny\n" + std::string{header} + "timestamp 1\n<< metal1 >>\nrect 0 0 4 4\n<< properties >>\n" +
         std::string{properties} + "<< end >>\n";
}

std::vector<std::string> messages(const ct::CellIo& io, const hefesto::diag::Status& status) {
  std::vector<std::string> out;
  for (const auto& d : io.sink.snapshot()) {
    out.push_back(d.text);
  }
  if (!status.ok()) {
    out.push_back(status.error().text);
  }
  return out;
}

bool has(const std::vector<std::string>& texts, std::string_view text) {
  for (const auto& t : texts) {
    if (t == text) {
      return true;
    }
  }
  return false;
}

bool has_paint(const db::CellDef& c) {
  for (std::size_t p = 1; p < c.plane_count(); ++p) {
    if (!c.plane(static_cast<hefesto::tech::PlaneId>(p)).empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE(".mag hints round trip as strips in enumeration order", MAG) {
  ct::CellIo io;
  db::CellDef& c = io.read("c", cell_text(""));
  db::set_property(c, "MASKHINTS_SRAM", db::PropertyValue{hints::HintRegion{Rect{0, 0, 10, 10}, Rect{5, 5, 15, 15}}});
  const std::string text = io.write(c);
  CHECK(text.find("<< properties >>\nstring MASKHINTS_SRAM 5 10 15 15 0 5 15 10 0 0 10 5\n") != std::string::npos);
  db::CellDef& back = io.read("back", text);
  REQUIRE(hints::find_hint(back, "SRAM") != nullptr);
  CHECK(*hints::find_hint(back, "SRAM") == *hints::find_hint(c, "SRAM"));
}

TEST_CASE(".mag hints are written as dimension with property compat off", MAG) {
  ct::CellIo io;
  io.ctx.property_compat = false;
  db::CellDef& c = io.read("c", cell_text(""));
  db::set_property(c, "MASKHINTS_A", db::PropertyValue{hints::HintRegion{Rect{0, 0, 4, 4}}});
  const std::string text = io.write(c);
  CHECK(text.find("\ndimension MASKHINTS_A 0 0 4 4\n") != std::string::npos);
  // Q-MISC-10: read back as a hint, not as a dimension list.
  db::CellDef& back = io.read("back", text);
  REQUIRE(hints::find_hint(back, "A") != nullptr);
  CHECK(hints::find_hint(back, "A")->strips() == Rects{{0, 0, 4, 4}});
}

TEST_CASE(".mag hints read with any type keyword, matched by prefix", MAG) {
  ct::CellIo io;
  db::CellDef& c = io.read("c", cell_text("STR MASKHINTS_A 0 0 10 10\ninteger MASKHINTS_B 0 0 2 2\n"
                                          "dim MASKHINTS_C 1 1 3 3\ndoub MASKHINTS_D 0 0 1 1\n"));
  CHECK(hints::find_hint(c, "A")->strips() == Rects{{0, 0, 10, 10}});
  CHECK(hints::find_hint(c, "B")->strips() == Rects{{0, 0, 2, 2}});
  CHECK(hints::find_hint(c, "C")->strips() == Rects{{1, 1, 3, 3}});
  CHECK(hints::find_hint(c, "D")->strips() == Rects{{0, 0, 1, 1}});
}

TEST_CASE(".mag hints scale up and down on read", MAG) {
  SECTION("scale-down truncates") {
    ct::CellIo io;
    io.ctx.rescale_database = [](std::int64_t, std::int64_t) { return false; };
    db::CellDef& c = io.read("c", cell_text("string MASKHINTS_A 0 0 9 9\n", "magscale 1 2\n"));
    CHECK(hints::find_hint(c, "A")->strips() == Rects{{0, 0, 4, 4}});
  }
  SECTION("scale-up multiplies") {
    ct::CellIo io;
    io.ctx.grid = hefesto::tech::GridScale{2, 1};
    db::CellDef& c = io.read("c", cell_text("string MASKHINTS_A 0 0 9 9\n"));
    CHECK(hints::find_hint(c, "A")->strips() == Rects{{0, 0, 18, 18}});
  }
}

TEST_CASE(".mag hint values that are not integers drop the property", MAG) {
  ct::CellIo io;
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& c = io.read("c", cell_text("string MASKHINTS_A 0 0 4 4\nstring MASKHINTS_A 0 0 abc 9\n"), &status);
  CHECK(has(messages(io, status), "Mask-hint \"abc 9\" has non-integer values!"));
  CHECK(c.properties().find("MASKHINTS_A") == nullptr);  // the earlier value goes too (DBPropPut NULL)
}

TEST_CASE(".mag hint values not divisible by four are truncated", MAG) {
  ct::CellIo io;
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& c = io.read("c", cell_text("string MASKHINTS_B 0 0 4 4 9 9\n"), &status);
  CHECK(has(messages(io, status), "Mask-hint property number of values is not divisible by four.  Truncated."));
  CHECK(hints::find_hint(c, "B")->strips() == Rects{{0, 0, 4, 4}});
}

TEST_CASE(".mag plane keyword with magic-compat ON is a format error", MAG) {
  ct::CellIo io;
  REQUIRE(io.ctx.magic_compat);
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& c =
      io.read("c", cell_text("string FOO bar\nplane MASKHINTS_X 0 0 10 10\nstring BAZ qux\n"), &status);
  const auto texts = messages(io, status);
  CHECK(has(texts, "Expected \"use\" line but saw: plane MASKHINTS_X 0 0 10 10"));
  CHECK(has(texts, "File c contained format error"));
  CHECK(c.properties().find("FOO") != nullptr);
  CHECK(c.properties().find("MASKHINTS_X") == nullptr);
  CHECK(c.properties().find("BAZ") == nullptr);
  CHECK(has_paint(c));  // what was read before the line is kept
}

TEST_CASE(".mag plane keyword with magic-compat OFF reads a hint", MAG) {
  ct::CellIo io;
  io.ctx.magic_compat = false;
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& c = io.read("c",
                           cell_text("string FOO bar\nplane MASKHINTS_X 0 0 10 10\nPLA MASKHINTS_Y 0 0 2 2\n"
                                     "string BAZ qux\n"),
                           &status);
  CHECK(status.ok());
  CHECK(messages(io, status).empty());
  CHECK(hints::find_hint(c, "X")->strips() == Rects{{0, 0, 10, 10}});
  CHECK(hints::find_hint(c, "Y")->strips() == Rects{{0, 0, 2, 2}});
  CHECK(c.properties().find("FOO") != nullptr);
  CHECK(c.properties().find("BAZ") != nullptr);
  // Hefesto never writes plane; property compat on writes string.
  io.ctx.magic_compat = true;
  CHECK(io.write(c).find("\nstring MASKHINTS_X 0 0 10 10\n") != std::string::npos);
}

TEST_CASE(".mag plane keyword for a key that is not a hint is reported and skipped", MAG) {
  ct::CellIo io;
  io.ctx.magic_compat = false;
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& c = io.read("c", cell_text("plane FOO 0 0 1 1\nstring BAZ qux\n"), &status);
  CHECK(has(messages(io, status), "Plane type specified for property \"FOO\" but property is not a mask hint!"));
  CHECK(c.properties().find("FOO") == nullptr);
  CHECK(c.properties().find("BAZ") != nullptr);
}

TEST_CASE(".mag hint coordinates take part in the scale reducer", MAG) {
  ct::CellIo io;
  io.ctx.grid = hefesto::tech::GridScale{2, 1};
  db::CellDef& c = io.read("c", "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 2 2\n<< end >>\n");
  REQUIRE(has_paint(c));
  db::set_property(c, "MASKHINTS_A", db::PropertyValue{hints::HintRegion{Rect{0, 0, 4, 4}}});
  CHECK(io.write(c).find("magscale") == std::string::npos);  // everything even: reducer 2
  CHECK(io.write(c).find("\nstring MASKHINTS_A 0 0 2 2\n") != std::string::npos);
  db::set_property(c, "MASKHINTS_A", db::PropertyValue{hints::HintRegion{Rect{0, 0, 3, 4}}});
  const std::string odd = io.write(c);
  CHECK(odd.find("magscale 1 2\n") != std::string::npos);
  CHECK(odd.find("\nstring MASKHINTS_A 0 0 3 4\n") != std::string::npos);
}
