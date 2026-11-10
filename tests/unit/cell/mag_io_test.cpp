// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The .mag reader and writer (capability cell-file-format) on the tiny
// test technology.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "cell_fixture.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using cell::test::CellIo;

TEST_CASE("a minimal file reads and writes back unchanged",
          "[cap:cell-file-format][req:Overall file grammar][req:Header parsing]") {
  CellIo io;
  const std::string text = "magic\ntech tiny\ntimestamp 5\n<< end >>\n";
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& a = io.read("a", text, &status);
  REQUIRE(status.ok());
  CHECK(a.flags().available);
  CHECK_FALSE(a.flags().modified);
  CHECK(a.timestamp() == 5);
  CHECK(io.write(a) == text);
}

TEST_CASE("paint, labels and properties round-trip byte-identically",
          "[cap:cell-file-format][req:Writer paint sections][req:Writer labels and ports][req:Writer properties]") {
  CellIo io;
  const std::string text =
      "magic\ntech tiny\ntimestamp 100\n"
      "<< polysilicon >>\nrect 0 0 4 10\n"
      "<< metal1 >>\nrect -2 3 8 5\n"
      "<< labels >>\nrlabel metal1 s -2 3 8 5 0 A\nport 1 nsew signal input\n"
      "rlabel polysilicon 0 0 4 10 1 in put\n"
      "<< properties >>\nstring a2 y\nstring a10 x\nstring FIXED_BBOX 0 0 10 10\n"
      "<< end >>\n";
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& c = io.read("c", text, &status);
  REQUIRE(status.ok());
  REQUIRE(c.labels().size() == 2);
  CHECK(c.labels()[1].text == "in put");
  CHECK(io.write(c) == text);
}

TEST_CASE("a Magic-written cell without extensions saves byte-identically",
          "[cap:hefesto-mag-extensions][req:Byte-compatibility when no extensions are used]") {
  CellIo io;
  const std::string text =
      "magic\ntech tiny\ntimestamp 1700000000\n"
      "<< polysilicon >>\nrect 0 0 4 10\n"
      "<< metal1 >>\nrect -2 3 8 5\n"
      "use inv  inv_0\ntimestamp 1600000000\ntransform 1 0 10 0 1 0\nbox 0 0 8 12\n"
      "<< labels >>\nrlabel metal1 s -2 3 8 5 0 A\nport 1 nsew signal input\n"
      "<< properties >>\nstring FIXED_BBOX 0 0 10 10\nstring LEFview TRUE\n"
      "<< end >>\n";
  REQUIRE(io.ctx.fs.create_directories("/lib").ok());
  REQUIRE(io.ctx.fs.write_atomic("/lib/c.mag", text).ok());
  io.paths.path = {"/lib"};
  auto loaded = cell::load_cell(io.ctx, "c");
  REQUIRE(loaded.ok());
  REQUIRE(cell::save_cell(io.ctx, *loaded.value()).ok());  // no edits
  const auto saved = io.ctx.fs.read("/lib/c.mag");
  REQUIRE(saved.ok());
  CHECK(saved.value() == text);
}

TEST_CASE("elements are kept and written sorted by natural name order",
          "[cap:cell-file-format][req:Element records][req:Writer elements]") {
  CellIo io;
  const std::string text =
      "magic\ntech tiny\ntimestamp 1\n"
      "<< elements >>\n"
      "text note1 white 10 20 \"hello world\" NORTH\n"
      "line l1 white 0 0 10 0 halfx,arrowright\n"
      "rectangle r10 red,blue 0 0 5 5\n"
      "rectangle r2 red 1 1 2 2 persistent\n"
      "text t2 white 0 0 \"x\" small,se\n"
      "line bad white 0 0 10\n"
      "<< end >>\n";
  db::CellDef& c = io.read("c", text);
  CHECK(io.has_code(cell::codes::kRecordSkipped));
  REQUIRE(c.elements().size() == 5);
  const db::Element& note = c.elements()[0];
  CHECK(note.kind == db::Element::Kind::text);
  CHECK(note.text == "hello world");
  CHECK(note.text_position == 1);
  CHECK(note.area.x0 == 10);
  CHECK(note.area.y0 == 20);
  CHECK(c.elements()[2].styles == std::vector<std::string>{"red", "blue"});
  CHECK(io.write(c) ==
        "magic\ntech tiny\ntimestamp 1\n"
        "<< elements >>\n"
        "line l1 white 0 0 10 0 halfx,arrowright\n"
        "text note1 white 10 20 \"hello world\" NORTH\n"
        "rectangle r2 red 1 1 2 2\n"
        "rectangle r10 red,blue 0 0 5 5\n"
        "text t2 white 0 0 \"x\" SOUTHEAST,small\n"
        "<< end >>\n");
}

TEST_CASE("element flags are a comma list; an unknown flag is reported",
          "[cap:cell-file-format][req:Element records][regression]") {
  // Regression: "NORTH small" (blank, not comma) was split silently; Magic
  // takes it as one unknown flag and prints "No such text element flag".
  CellIo io;
  db::CellDef& c = io.read("c", "magic\ntech tiny\ntimestamp 1\n<< elements >>\n"
                                "text t1 red 5 5 \"hello\" NORTH small\n"
                                "rectangle r1 white 0 0 1 1 small\n"
                                "line l1 white 0 0 1 1 halfx,bogus\n<< end >>\n");
  REQUIRE(c.elements().size() == 3);
  std::vector<std::string> texts;
  for (const auto& d : io.sink.snapshot()) texts.push_back(d.text);
  CHECK(texts == std::vector<std::string>{"No such text element flag \"NORTH small\"",
                                          "No such rect element flag \"small\"",
                                          "No such line element flag \"bogus\""});
  CHECK(c.elements()[0].text_position == 0);  // unchanged
  CHECK(c.elements()[2].half_x);              // the known flag still applies
}

TEST_CASE("temporary elements are not written", "[cap:cell-file-format][req:Writer elements]") {
  CellIo io;
  db::CellDef& c = io.read("c", "magic\ntech tiny\ntimestamp 1\n<< elements >>\nrectangle r red 0 0 1 1 temporary\n"
                                "<< end >>\n");
  REQUIRE(c.elements().size() == 1);
  CHECK_FALSE(c.elements()[0].persistent);
  CHECK(io.write(c) == "magic\ntech tiny\ntimestamp 1\n<< end >>\n");
}

TEST_CASE("a bare '#' comment line in a paint section skips only that line",
          "[cap:cell-file-format][req:Line reading model][regression]") {
  // Regression: the comment's rest was read with the logical line reader,
  // which skips blank lines, so a lone "#" swallowed the next record.
  CellIo io;
  db::CellDef& c = io.read("c", "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\n#\nrect 0 0 2 2\n<< end >>\n");
  CHECK(io.write(c) == "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 2 2\n<< end >>\n");
}

TEST_CASE("a file without << end >> keeps what was read and reports a format error",
          "[cap:cell-file-format][req:Format errors and partial reads]") {
  CellIo io;
  hefesto::diag::Status status = hefesto::diag::Done{};
  db::CellDef& c = io.read("c", "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 2 2\n", &status);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == cell::codes::kFormatError);
  CHECK(c.flags().available);
  CHECK(io.write(c).find("rect 0 0 2 2\n") != std::string::npos);
}

TEST_CASE("use blocks are written in natural id order",
          "[cap:cell-file-format][req:Writer use blocks][req:Natural ordering of names]") {
  CellIo io;
  const std::string leaf = "use leaf ";
  std::string text = "magic\ntech tiny\ntimestamp 9\n";
  for (const char* id : {"a10", "B1", "a2"}) {
    text += leaf + id + "\ntimestamp 3\ntransform 1 0 0 0 1 0\nbox 0 0 4 4\n";
  }
  text += "<< end >>\n";
  db::CellDef& top = io.read("top", text);
  REQUIRE(top.uses().size() == 3);
  const std::string out = io.write(top);
  CHECK(out.find("use leaf  a2\n") < out.find("use leaf  a10\n"));
  CHECK(out.find("use leaf  a10\n") < out.find("use leaf  B1\n"));
}

TEST_CASE("writer reduces the scale and writes magscale", "[cap:cell-file-format][req:Writer header and scale reduction]") {
  CellIo io;
  db::CellDef& c = io.read("c", "magic\ntech tiny\nmagscale 1 2\ntimestamp 1\n<< metal1 >>\nrect 0 0 3 5\n<< end >>\n");
  io.grid = hefesto::tech::GridScale{2, 1};  // what the rescale step set
  CHECK(io.write(c) == "magic\ntech tiny\nmagscale 1 2\ntimestamp 1\n<< metal1 >>\nrect 0 0 3 5\n<< end >>\n");
}

TEST_CASE("names with whitespace cannot be written", "[cap:cell-file-format][req:Writer use blocks]") {
  CHECK(cell::check_writable_name("cell name", "inv").ok());
  const auto bad = cell::check_writable_name("cell name", "in v");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == cell::codes::kInvalidName);
}

TEST_CASE("the command script holds the prologue, paint and epilogue",
          "[cap:cell-file-format][req:Command-script content]") {
  CellIo io;
  db::CellDef& a = io.read("a", "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  const std::string script = cell::write_command_script(io.ctx, a);
  CHECK(script.starts_with("# Command script for generating cell a\n\nsuspendall\n"));
  CHECK(script.find("box values 0 0 4 4\npaint metal1\n") != std::string::npos);
  CHECK(script.ends_with("units {*}$curunits\nresumeall\n"));
}

TEST_CASE("HEFESTO_ properties of a newer schema are kept and reported",
          "[cap:hefesto-mag-extensions][req:Reserved property namespace][req:Extension values are versioned]") {
  CellIo io;
  const std::string text = "magic\ntech tiny\ntimestamp 1\n<< properties >>\n"
                           "string HEFESTO_annotations v3:future data\nstring HEFESTO_view3d v1:x\n<< end >>\n";
  db::CellDef& c = io.read("c", text);
  CHECK(io.has_code(cell::codes::kNewerExtension));
  CHECK(io.write(c) == text);
}

TEST_CASE("a HEFESTO_ schema version too long for an int is still a newer version",
          "[cap:hefesto-mag-extensions][req:Extension values are versioned]") {
  // Regression (libFuzzer fuzz_mag_reader): the version digits overflowed an int.
  CellIo io;
  const std::string text = "magic\ntech tiny\ntimestamp 1\n<< properties >>\n"
                           "string HEFESTO_note v577777777777777777777777777:x\n<< end >>\n";
  db::CellDef& c = io.read("c", text);
  CHECK(io.has_code(cell::codes::kNewerExtension));
  CHECK(io.write(c) == text);
}

TEST_CASE("gzip cell data is decompressed; plain data passes through",
          "[cap:cell-file-format][req:Compressed cell input]") {
  constexpr unsigned char kGzip[] = {
      0x1f, 0x8b, 0x8,  0x0,  0x0,  0x0,  0x0,  0x0,  0x2,  0xff, 0xcb, 0x4d, 0x4c, 0xcf, 0x4c,
      0xe6, 0x2a, 0x49, 0x4d, 0xce, 0x50, 0x28, 0xc9, 0xcc, 0xab, 0xe4, 0x2a, 0xc9, 0xcc, 0x4d,
      0x2d, 0x2e, 0x49, 0xcc, 0x2d, 0x50, 0x30, 0xe7, 0xb2, 0xb1, 0x51, 0x48, 0xcd, 0x4b, 0x51,
      0xb0, 0xb3, 0xe3, 0x2,  0x0,  0xe3, 0x59, 0x6e, 0xc5, 0x26, 0x0,  0x0,  0x0};
  std::string data;
  for (const unsigned char b : kGzip) {
    data.push_back(static_cast<char>(b));
  }
  const auto plain = cell::maybe_decompress(data);
  REQUIRE(plain.ok());
  CHECK(plain.value() == "magic\ntech tiny\ntimestamp 7\n<< end >>\n");
  CHECK(cell::maybe_decompress("magic\n").value() == "magic\n");
  CHECK_FALSE(cell::maybe_decompress(data.substr(0, 20)).ok());
}
