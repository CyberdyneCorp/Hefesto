// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The .mag reader and writer (capability cell-file-format) on the tiny
// test technology.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

#include "cell_fixture.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using cell::test::CellIo;

TEST_CASE("a malformed bounding-box property message has no line end with magic-compat ON only",
          "[cap:cell-file-format][req:Properties section records]") {
  // Q-LEFDEF-134 (compat): Magic's TxError in DBio.c:3345 has no newline,
  // for FIXED_BBOX and OBS_BBOX alike, so this message and LEF write's
  // OBS_BBOX error share a line (LEFDEF-132).
  for (const std::string key : {"OBS_BBOX", "FIXED_BBOX"}) {
    for (const bool on : {true, false}) {
      INFO(key << ", magic-compat " << (on ? "ON" : "OFF"));
      CellIo io;
      io.ctx.magic_compat = on;
      const std::string text = "magic\ntech tiny\ntimestamp 1\n<< properties >>\nstring " + key +
                               " 1 2 3\n<< end >>\n";
      (void)io.read("c", text);
      bool found = false;
      for (const auto& d : io.sink.snapshot()) {
        if (d.code == cell::codes::kRecordValue) {
          found = true;
          CHECK(d.text == "Cannot read bounding box values in " + key + " property");
          CHECK(d.no_newline == on);
        }
      }
      CHECK(found);
    }
  }
}

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

TEST_CASE("a coarser file reports its scaling", "[cap:cell-file-format][req:Coordinate scaling on read]") {
  // Regression (golden DRC-STD-GF180-7T5V0): Magic prints step 5's message
  // at its default verbosity; the reader scaled the geometry silently.
  CellIo io;
  io.grid = hefesto::tech::GridScale{2, 1};  // internal = 2 lambda: n = 2
  (void)io.read("c", "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 3 5\n<< end >>\n");
  bool reported = false;
  for (const auto& d : io.sink.snapshot()) {
    reported = reported || d.text == "Scaled magic input cell c geometry by factor of 2";
  }
  CHECK(reported);
}

namespace {

constexpr const char* kUseHead = "magic\ntech tiny\ntimestamp 1\nuse child child_0\ntimestamp 0\n";
constexpr const char* kUseTail = "use child child_1\ntimestamp 0\ntransform 1 0 0 0 1 0\nbox 0 0 4 4\n<< end >>\n";

bool use_ignored_off_plane(const CellIo& io) {
  for (const auto& d : io.sink.snapshot()) {
    if (d.code == db::codes::kGeometryOutsidePlane &&
        d.text == "Ignoring subcell child of top: its transform or box lies outside the usable plane "
                  "(-1073741818..1073741818)") {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("a use whose transform or box leaves the usable plane is ignored",
          "[cap:cell-file-format][req:Use block parsing][regression]") {
  // Regression (fuzz_mag_reader, CI run 43487286969): a translation and a box
  // corner of -1908874356 overflowed int32 when the use's bounding box was
  // transformed (Q-MODEL-24: geometry beyond the plane is rejected).
  for (const char* block : {"transform 1 0 -1908874356 0 1 0\nbox -1908874356 0 1 0\n",
                            "transform 1 0 1073741819 0 1 0\nbox 0 0 4 4\n",
                            "transform 0 1 0 -1 0 -1073741819\nbox 0 0 4 4\n",
                            "transform 1 0 0 0 1 0\nbox 0 0 4 1073741819\n"}) {
    CellIo io;
    hefesto::diag::Status status = hefesto::diag::Done{};
    db::CellDef& top = io.read("top", std::string{kUseHead} + block + kUseTail, &status);
    CHECK(status.ok());
    CHECK(use_ignored_off_plane(io));
    REQUIRE(top.uses().size() == 1);
    CHECK(top.uses()[0]->id == "child_1");
  }
}

TEST_CASE("a use at the edge of the usable plane is kept", "[cap:cell-file-format][req:Use block parsing]") {
  CellIo io;
  db::CellDef& top = io.read(
      "top", std::string{kUseHead} + "transform 1 0 1073741818 0 1 -1073741818\nbox -1073741818 0 0 4\n" + kUseTail);
  CHECK_FALSE(use_ignored_off_plane(io));
  CHECK(top.uses().size() == 2);
}

TEST_CASE("the usable-plane limit of a use applies after scaling", "[cap:cell-file-format][req:Use block parsing]") {
  CellIo io;
  io.grid = hefesto::tech::GridScale{2, 1};  // n = 2: 600000000 scales to 1200000000
  db::CellDef& top = io.read("top", std::string{kUseHead} + "transform 1 0 600000000 0 1 0\nbox 0 0 4 4\n" + kUseTail);
  CHECK(use_ignored_off_plane(io));
  CHECK(top.uses().size() == 1);
}

TEST_CASE("a huge magscale over crossed triangles reads with bounded work",
          "[cap:layout-paint-semantics][req:Non-Manhattan painting][regression]") {
  // Regression (fuzz_mag_reader timeout, CI run 43648522848): with
  // `magscale 8888882 2` every paint of the crossed triangles below
  // descended into the full chain of sub-areas allowed per paint, each
  // working over the tiles the chain had already left, and the contact
  // images were then repainted one strip at a time; the read took about
  // 10 s in a debug build (Q-MODEL-35). plane_test bounds the work of one
  // such paint; this bounds the time of the whole read.
  CellIo io;
  const std::string text =
      "magic\n"
      "tech tiny\n"
      "magscale 8888882 2\n"
      "timestamp 1000\n"
      "<< po<< ~etal1 t 0 0 10 20\n"
      "rect 10 0 20 4\n"
      "<< ~etal1 >>\n"
      "rec@t -2 -8\n"
      "rect 0 0 10 20 -4 -4 24 24\n"
      "tri\xb4 0 30 10 eeSans 16 4 24 24\n"
      "tri\xb4 0 30 17 eeSanmagirec@t -2 -8 0\n"
      "rect 10 0 20 4\n"
      "<< ~etal1 t 0 0 1* 20\n"
      "rect 10 0 20 4\n"
      "<< ~et2al1 >>\n"
      "rect -2 -8 24 24\x11tr2lysilicon >>\n"
      "rect 0 0ilicon >>\n"
      "rect\xac\xac\xac\xacmm";
  const auto start = std::chrono::steady_clock::now();
  db::CellDef& c = io.read("c", text);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(c.flags().available);
  // About 0.3 s in a debug build now; the bound leaves room for sanitizers.
  CHECK(elapsed < std::chrono::seconds{5});
}

namespace {

std::size_t total_tiles(const db::CellDef& def) {
  std::size_t n = 0;
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    n += def.plane(static_cast<hefesto::tech::PlaneId>(p)).tile_count();
  }
  return n;
}

// The triangles of the fuzz_mag_reader seeds, scaled by `magscale 8888882
// 2`: `/` on (0,0)-(30k,10k) and `\` on (0,0)-(30k,17k).
constexpr int kHuge = 4444441;
constexpr db::Rect kSlash{0, 0, 30 * kHuge, 10 * kHuge};
constexpr db::Rect kBack{0, 0, 30 * kHuge, 17 * kHuge};

}  // namespace

TEST_CASE("a paint shares its non-Manhattan budget with the contact images it repaints",
          "[cap:layout-paint-semantics][req:Non-Manhattan painting][regression]") {
  // fuzz_mag_reader timeout: painting a residue over the staircase of
  // contact images a huge triangle left repainted every image strip as a
  // triangle paint of its own, each with a whole budget of pieces over the
  // crossed triangles, so one paint left tens of thousands of tiles
  // (Q-MODEL-36). Every type of the tiny technology paints both triangles,
  // as a `<< ~metal1 >>` section does.
  CellIo io;
  const auto tech = cell::test::tiny_technology();
  auto created = io.library.create("c", tech->plane_count());
  REQUIRE(created.ok());
  db::CellDef& def = *created.value();
  std::size_t most = 0;
  for (const db::Rect& area : {kSlash, kBack}) {
    for (std::size_t t = 1; t < tech->user_type_end(); ++t) {
      const std::size_t before = total_tiles(def);
      const auto type = static_cast<hefesto::tech::TileType>(t);
      REQUIRE(db::paint_type(def, *tech, area, type, db::TriangleSpec{db::Diagonal::nw_se, true}).ok());
      const std::size_t after = total_tiles(def);
      most = std::max(most, after > before ? after - before : 0);
    }
  }
  // Every piece leaves at most about four tiles: 1276 at most now; 2905
  // when each image repaint had a budget of its own.
  INFO("most tiles added by one paint: " << most);
  CHECK(most < 4 * db::NMBudget::kPieces);
}

TEST_CASE("the paints of a .mag read share one non-Manhattan budget",
          "[cap:layout-paint-semantics][req:Non-Manhattan painting][regression]") {
  // fuzz_mag_reader variants: every paint of a huge triangle over crossed
  // triangles may leave a few thousand tiles, and one record of a type
  // list section paints every type, so a few kilobytes of such records
  // left hundreds of thousands of tiles for every later paint and image
  // repaint to walk (Q-MODEL-36). The read now grants its paints 16 pieces
  // per record on top of one NMBudget.
  std::string text = "magic\ntech tiny\nmagscale 8888882 2\ntimestamp 1000\n";
  const char* const dirs[] = {"s", "se", "ne", "nw", "sw", "n", "e", "w"};
  std::size_t records = 0;
  for (const char* layer : {"~metal1", "~metal2", "~poly", "~pc", "~m2c", "~ndc", "~nwell", "~pwell"}) {
    text += std::string{"<< "} + layer + " >>\n";
    for (int i = 0; i < 6; ++i) {
      text += "tri 0 0 " + std::to_string(30 + i) + " " + std::to_string(17 + i % 5) + " " + dirs[i] + "\n";
      ++records;
    }
  }
  CellIo io;
  const auto start = std::chrono::steady_clock::now();
  db::CellDef& c = io.read("c", text);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  // 190 tiles and 22 ms in a debug build now; without the pool 37856 tiles
  // and 144 s.
  INFO("tiles: " << total_tiles(c));
  CHECK(total_tiles(c) < 4 * (db::NMBudget::kPieces + 16 * records));
  CHECK(elapsed < std::chrono::seconds{5});
}

TEST_CASE("crossed triangles under a huge magscale read with bounded work (image repaints)",
          "[cap:layout-paint-semantics][req:Non-Manhattan painting][regression]") {
  // Regression (fuzz_mag_reader timeout, CI fuzz job, after Q-MODEL-35):
  // the paints of this file repainted the staircase of contact images left
  // by the crossed triangles strip by strip, each strip with a budget of
  // its own, and a later rectangle walked the 150000 tiles that left; the
  // read took about 12 s in a debug build (Q-MODEL-36). The input is the
  // fuzz_mag_reader seed regression-3b720185126474ca4033c22ea793e51f2711551e.
  using namespace std::string_view_literals;
  CellIo io;
  const std::string_view text =
      "magic\n"
      "tech tiny\n"
      "magscale 8888882 2\n"
      "timestamp 1000\n"
      "<< po<< ~etal1 t 0 0 10 20\n"
      "rect 10 0 20 4\n"
      "<< ~etal1 >>\n"
      "rec@t -2 -8\n"
      "rect 0 0 10 20 -4 -4 24 24\n"
      "tri\xb4 0 20 10 eeSans 16 4 24 24\n"
      "tri\xb4 0 30 17 eeSanmagirec@t -2 -\x04\x00\x00\x00\x00\x00\x00\x00" "8 \x00\x00\x00\x00\x00\x00\x00\x00" "0\n"
      "rect 05 0 20 4\n"
      "<< ~etal1 t 0 0 1* 10\n"
      "rect 10 0 20 4\n"
      "<< ~et2al1 >>\n"
      "rect -2 -8 24 24\x11tr2lysilicon >>\n"
      "rect 0 0ilicon >>\n"
      "rect\xac\xac\xac\xacmm"sv;
  REQUIRE(text.size() == 354);
  const auto start = std::chrono::steady_clock::now();
  db::CellDef& c = io.read("c", text);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(c.flags().available);
  // About 0.03 s in a debug build now; the bound leaves room for sanitizers.
  CHECK(elapsed < std::chrono::seconds{5});
}
