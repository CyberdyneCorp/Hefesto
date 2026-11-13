// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Display-style files: .cmap, .dstyle, stipples, glyphs and file lookup
// (capability display-style-files).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/diag_codes.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/platform/file_system.hpp"

namespace display = hefesto::display;
namespace diag = hefesto::diag;

namespace {

struct DstyleSpec {
  int version = 8;
  std::size_t display_lines = 52;
  std::size_t layout_lines = 3;
  std::size_t pale_lines = 3;
  std::string extra_layout;  // replaces the generated layout lines when set
};

/// A synthetic version-8 dstyle: display style i has color i and long name
/// d<i>; layout style k has num 10+k and name L<k>; pale style k name P<k>.
std::string make_dstyle(const DstyleSpec& spec) {
  std::ostringstream out;
  out << "version " << spec.version << "\n\ndisplay_styles 24\n";
  for (std::size_t i = 0; i < spec.display_lines; ++i) {
    out << i << " 127 " << i << " 0 solid 0 - d" << i << "\n";
  }
  out << "\nlayout_styles\n";
  if (spec.extra_layout.empty()) {
    for (std::size_t k = 0; k < spec.layout_lines; ++k) {
      out << 10 + k << " 64 " << 60 + k << " 0xff stipple 6 " << static_cast<char>('a' + k) << " L" << k << "\n";
    }
  } else {
    out << spec.extra_layout;
  }
  out << "\npale_styles\n";
  for (std::size_t k = 0; k < spec.pale_lines; ++k) {
    out << 10 + k << " 32 " << 60 + k << " 0 stipple 6 - P" << k << "\n";
  }
  out << "\nstipples\n6  aa 55 aa 55 aa 55 aa 55\thalf 'n half\n";
  return out.str();
}

display::ColorMap gray_map() {
  display::ColorMap map;
  map.entries.resize(256);
  for (std::size_t i = 0; i < 256; ++i) {
    const auto v = static_cast<std::uint8_t>(i);
    map.entries[i] = display::ColorEntry{v, v, v, ""};
  }
  return map;
}

diag::Outcome<display::StyleTable> parse(const std::string& text, bool compat, diag::CollectingSink& sink) {
  return display::parse_dstyle(text, "t.OpenGL.dstyle", gray_map(), compat, sink);
}

bool has_message(const diag::CollectingSink& sink, std::string_view code, std::string_view text) {
  for (const auto& d : sink.snapshot()) {
    if (d.code == code && d.text.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::string magic_file(const char* relative) {
  return std::string(HEFESTO_SOURCE_DIR) + "/build/ref/magic/" + relative;
}

std::string read_text(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

}  // namespace

// --- color maps ------------------------------------------------------------------------

TEST_CASE("cmap entries fill runs of indices", "[cap:display-style-files][req:Color map file grammar]") {
  const auto map = display::parse_color_map("255 0 0 2\n0 0 255 3\n255 255 255 255\n", "t.cmap", true);
  REQUIRE(map.ok());
  REQUIRE(map.value().entries.size() == 256);
  CHECK(map.value().entries[2] == display::ColorEntry{255, 0, 0, ""});
  CHECK(map.value().entries[3] == display::ColorEntry{0, 0, 255, ""});
  CHECK(map.value().entries[4] == display::ColorEntry{255, 255, 255, ""});
  CHECK(map.value().entries[255] == display::ColorEntry{255, 255, 255, ""});
}

TEST_CASE("cmap names are stored on the indices an entry fills", "[cap:display-style-files][req:Color map file grammar]") {
  const auto map = display::parse_color_map("200 200 200 0 background_gray\n220 95 95 1 poly_red\n", "t.cmap", true);
  REQUIRE(map.ok());
  CHECK(map.value().entries[1] == display::ColorEntry{220, 95, 95, "poly_red"});
  CHECK(map.value().find("poly_red") == 1U);
}

TEST_CASE("cmap blank lines are skipped and errors reported", "[cap:display-style-files][req:Color map file grammar]") {
  CHECK(display::parse_color_map("# c\n0 0 0 0\n\n1 1 1 1\n", "t.cmap", true).ok());
  const auto order = display::parse_color_map("0 0 0 1\n1 1 1 0\n2 2 2 3\n", "t.cmap", true);
  REQUIRE_FALSE(order.ok());
  CHECK(order.error().code == display::codes::kCmapOrder);
  CHECK(order.error().text == "Colors in map are out of order!");
  const auto syntax = display::parse_color_map("0 0 0 1\nbogus\n", "t.cmap", true);
  REQUIRE_FALSE(syntax.ok());
  CHECK(syntax.error().text == "Syntax error in color map file \"t.cmap\"\nLast color read was index 1");
}

TEST_CASE("cmap sizes above the entry limit are refused before allocation",
          "[cap:display-style-files][req:Color map file grammar][regression]") {
  // The limit is the default 2^24 entries; a caller may ask for fewer.
  const auto big = display::parse_color_map("0 0 0 16777216\n", "t.cmap", true);
  REQUIRE_FALSE(big.ok());
  CHECK(big.error().code == display::codes::kCmapSyntax);
  CHECK(big.error().text == "Color map file \"t.cmap\" has too many colors");
  CHECK_FALSE(display::parse_color_map("0 0 0 1024\n", "t.cmap", true, 1024).ok());
  const auto at_limit = display::parse_color_map("0 0 0 1023\n", "t.cmap", true, 1024);
  REQUIRE(at_limit.ok());
  CHECK(at_limit.value().entries.size() == 1024);
  // Regression (fuzz_cmap OOM, CI run 43487286969): the size comes from the
  // 127-byte fgets() chunks, so the second chunk of one long line declares
  // index 9325154 although the line's own fourth integer is 50.
  const std::string long_line = "1 2 3 50 " + std::string(118, 'x') + "0 0 0 9325154 n\n";
  REQUIRE(long_line.find("0 0 0 9325154") == 127);
  const auto split = display::parse_color_map(long_line, "t.cmap", true, std::size_t{1} << 20);
  REQUIRE_FALSE(split.ok());
  CHECK(split.error().text == "Color map file \"t.cmap\" has too many colors");
}

TEST_CASE("cmap saving merges runs and keeps the first name", "[cap:display-style-files][req:Color map file writing]") {
  display::ColorMap map;
  map.entries.assign(256, display::ColorEntry{255, 255, 255, ""});
  map.entries[0] = {0, 0, 0, "black"};
  map.entries[5].name = "dropped";
  CHECK(display::write_color_map(map) == "0 0 0 0 black\n255 255 255 255\n");
}

// --- dstyle ---------------------------------------------------------------------------

TEST_CASE("dstyle table indices follow file order, not the num column",
          "[cap:display-style-files][req:Style table model]") {
  DstyleSpec spec;
  spec.layout_lines = 0;
  spec.pale_lines = 2;
  spec.extra_layout = "10 64 1 0 solid 0 a first\n52 64 2 0 solid 0 b second\n";
  diag::CollectingSink sink;
  const auto table = parse(make_dstyle(spec), true, sink);
  REQUIRE(table.ok());
  const auto& t = table.value();
  CHECK(t.layout_count == 2);
  CHECK(t.styles[52].long_name == "first");
  CHECK(t.styles[53].long_name == "second");
  CHECK(t.styles[t.pale_index(52)].long_name == "P0");
  CHECK(t.pale_index(52) == 54);
}

TEST_CASE("the crosshair uses fixed slot 44", "[cap:display-style-files][req:Fixed internal style slots]") {
  diag::CollectingSink sink;
  auto table = parse(make_dstyle({}), true, sink);
  REQUIRE(table.ok());
  display::StyleSet set;
  set.colors = gray_map();
  set.colors.entries[47] = {1, 2, 3, ""};
  set.table = table.value();
  set.table.styles[display::kStyleCrosshair].color = 47;
  const auto style = display::resolve_style(set, display::kStyleCrosshair);
  CHECK(style.r == 1.0F / 255.0F);
  CHECK(display::kStyleCrosshair == 44);
}

TEST_CASE("dstyle field bases depend on the version", "[cap:display-style-files][req:Dstyle file grammar]") {
  diag::CollectingSink sink;
  DstyleSpec v8;
  v8.extra_layout = "6\t127\t64\t0xff\toutline\t0\t-\toutline_highlights\n30 127 070 0x00 solid 0 - gray1 (pale)\n"
                    "7 127 33 0 solid 0 - x\n";
  auto table = parse(make_dstyle(v8), true, sink);
  REQUIRE(table.ok());
  const auto& s = table.value().styles[52];
  CHECK(s.mask == 127);
  CHECK(s.color == 64);
  CHECK(s.outline == 255);
  CHECK(s.fill == display::FillKind::outline);
  CHECK(s.short_name == '-');
  CHECK(s.long_name == "outline_highlights");
  CHECK(table.value().styles[53].color == 56);  // C integer rules: leading 0 is octal
  CHECK(table.value().styles[53].long_name == "gray1");

  // No version section: version 5, octal fields.
  const std::string line = "\nlayout_styles\n";
  std::string text = "display_styles 7\n";
  for (int i = 0; i < 52; ++i) {
    text += std::to_string(i) + " 100 100 377 solid 0 - x\n";
  }
  text += line + "1 100 100 377 solid 0 - x\n\npale_styles\n1 100 100 377 solid 0 - y\n";
  auto old = parse(text, true, sink);
  REQUIRE(old.ok());
  CHECK(old.value().version == 5);
  CHECK(old.value().styles[52].mask == 64);
  CHECK(old.value().styles[52].color == 64);
  CHECK(old.value().styles[52].outline == 255);
}

TEST_CASE("dstyle colors resolve by cmap name and fills are checked",
          "[cap:display-style-files][req:Dstyle style-line field semantics]") {
  display::ColorMap map = gray_map();
  map.entries[2].name = "diff_green";
  DstyleSpec spec;
  spec.layout_lines = 1;
  spec.pale_lines = 1;
  spec.extra_layout = "20 127 diff_green 0 solid 0 d diff\n";
  diag::CollectingSink sink;
  const auto table = display::parse_dstyle(make_dstyle(spec), "t", map, true, sink);
  REQUIRE(table.ok());
  CHECK(table.value().styles[52].color == 2);

  spec.extra_layout = "20 127 3 0 dots 0 d diff\n";
  const auto bad = display::parse_dstyle(make_dstyle(spec), "t", map, true, sink);
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == display::codes::kDstyleLine);
  CHECK(bad.error().text.starts_with("Style line contained format error:"));

  // Unknown color names: -1 masked to all ones with magic-compat, an error without (Q-UI-5).
  spec.extra_layout = "20 127 nosuch 0 solid 0 d diff\n";
  const auto unknown = display::parse_dstyle(make_dstyle(spec), "t", map, true, sink);
  REQUIRE(unknown.ok());
  CHECK(unknown.value().styles[52].color == 0xFFFFFF);
  CHECK_FALSE(display::parse_dstyle(make_dstyle(spec), "t", map, false, sink).ok());
}

TEST_CASE("dstyle display-style count mismatch shifts indices with magic-compat",
          "[cap:display-style-files][req:Dstyle section count validation]") {
  DstyleSpec spec;
  spec.display_lines = 51;
  diag::CollectingSink sink;
  const auto table = parse(make_dstyle(spec), true, sink);
  REQUIRE(table.ok());
  CHECK(has_message(sink, display::codes::kDstyleCount,
                    "Error: Display style file defines only 51 of 52 required internal styles."));
  // Magic fills the table from its end: every entry moves up by one.
  CHECK(table.value().styles[52].long_name == "L0");
  CHECK(table.value().styles[1].long_name == "d0");
  CHECK_FALSE(table.value().styles[0].defined);

  diag::CollectingSink strict;
  CHECK_FALSE(parse(make_dstyle(spec), false, strict).ok());
}

TEST_CASE("dstyle pale count and missing sections fail the load",
          "[cap:display-style-files][req:Dstyle section count validation]") {
  DstyleSpec spec;
  spec.layout_lines = 120;
  spec.pale_lines = 119;
  diag::CollectingSink sink;
  const auto pale = parse(make_dstyle(spec), true, sink);
  REQUIRE_FALSE(pale.ok());
  CHECK(pale.error().text == "Error:  Number of pale styles (119) is different from the number of layout styles (120)");

  const auto missing = parse("version 8\n\ndisplay_styles 24\n0 127 0 0 solid 0 - x\n", true, sink);
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == display::codes::kDstyleMissing);
  CHECK(missing.error().text ==
        "Not all required style sections were read.  Missing sections are: layout_styles pale_styles");
}

TEST_CASE("dstyle section rules", "[cap:display-style-files][req:Dstyle file grammar]") {
  diag::CollectingSink sink;
  const std::string good = make_dstyle({});

  std::string bad_name = good;
  bad_name.replace(bad_name.find("stipples"), 8, "stippels");
  const auto unknown = parse(bad_name, true, sink);
  REQUIRE_FALSE(unknown.ok());
  CHECK(unknown.error().text == "Bad section name \"stippels\" in .dstyle file.");

  // layout_styles after pale_styles: rejected, never an exit (Q-UI-2).
  std::string reordered = good + "\nlayout_styles\n1 1 1 0 solid 0 - z\n";
  const auto late = parse(reordered, true, sink);
  REQUIRE_FALSE(late.ok());
  CHECK(late.error().code == display::codes::kDstyleOrder);

  std::string display_late = good + "\ndisplay_styles\n";
  CHECK_FALSE(parse(display_late, true, sink).ok());

  // An integer in header position: a warning, then read as data.
  std::string empty_line = good;
  empty_line.replace(empty_line.find("10 64 60"), 0, "\n");
  diag::CollectingSink warn;
  REQUIRE(parse(empty_line, true, warn).ok());
  CHECK(has_message(warn, display::codes::kDstyleEmptyLine, "Unexpected empty line in .dstyle file."));

  // Version 6 reads octal fields: a file without hexadecimal values.
  DstyleSpec octal;
  octal.extra_layout = "10 64 60 377 solid 0 a L0\n11 64 61 377 solid 0 b L1\n12 64 62 377 solid 0 c L2\n";
  std::string no_version = make_dstyle(octal);
  no_version.erase(no_version.find("\nstipples"));
  no_version.replace(0, 9, "version x");
  diag::CollectingSink v6;
  const auto t6 = parse(no_version, true, v6);
  REQUIRE(t6.ok());
  CHECK(t6.value().version == 6);
  CHECK(has_message(v6, display::codes::kDstyleVersion, "DStyle format version could not be read: assuming version 6"));
}

TEST_CASE("stipple patterns use the OpenGL orientation", "[cap:display-style-files][req:Stipple patterns]") {
  diag::CollectingSink sink;
  const auto table = parse(make_dstyle({}), true, sink);
  REQUIRE(table.ok());
  const display::Stipple s = table.value().stipple(6);
  CHECK(s == display::Stipple{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55});
  // Row 0 is the bottom pixel row; the MSB is the leftmost pixel.
  auto painted = [&s](int x, int y) { return ((s[static_cast<std::size_t>(y & 7)] >> (7 - (x & 7))) & 1) != 0; };
  CHECK(painted(0, 0));
  CHECK_FALSE(painted(1, 0));
  CHECK(table.value().stipple(5) == display::Stipple{});  // undefined: all zero
}

TEST_CASE("the type-to-style lookup uses the num ordinal of layout styles",
          "[cap:display-style-files][req:Style loading triggered by styletype]") {
  DstyleSpec spec;
  spec.layout_lines = 0;
  spec.pale_lines = 25;
  for (int k = 0; k < 25; ++k) {
    spec.extra_layout += std::to_string(k == 24 ? 20 : 100 + k) + " 127 1 0 solid 0 - s" + std::to_string(k) + "\n";
  }
  diag::CollectingSink sink;
  const auto table = parse(make_dstyle(spec), true, sink);
  REQUIRE(table.ok());
  CHECK(table.value().find_layout_style("20") == 52U + 24U);
  CHECK(table.value().find_layout_style("s3") == 55U);
  CHECK_FALSE(table.value().find_layout_style("nosuch").has_value());
}

// --- glyphs ----------------------------------------------------------------------------

TEST_CASE("glyph pixels map to style short names and record the hot spot",
          "[cap:display-style-files][req:Glyph file grammar]") {
  display::StyleTable table;
  table.by_short_name[static_cast<unsigned char>('K')] = 7;
  diag::CollectingSink sink;
  const auto glyphs = display::parse_glyphs("# cursor\nsize 1 3 3\n. K .\nK K*K\n. K .\n", "g.glyphs", table, sink);
  REQUIRE(glyphs.ok());
  const auto& g = glyphs.value().front();
  CHECK(g.width == 3);
  CHECK(g.hot_x == 1);
  CHECK(g.hot_y == 1);
  CHECK(g.pixel(1, 2) == 7U);
  CHECK(g.pixel(0, 2) == 0U);  // '.' is no style's short name: transparent
}

TEST_CASE("glyph file errors", "[cap:display-style-files][req:Glyph file grammar]") {
  display::StyleTable table;
  diag::CollectingSink sink;
  std::string rows;
  for (int i = 0; i < 16; ++i) {
    rows += ". . . . . . . . . . . . . . . .\n";
  }
  const auto missing = display::parse_glyphs("size 2 16 16\n" + rows, "g", table, sink);
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().text == "Unexpected end of file in file 'g'");

  const auto extra = display::parse_glyphs("size 1 1 1\n.\n.\n", "g", table, sink);
  CHECK(extra.ok());
  CHECK(has_message(sink, display::codes::kGlyphExtra, "Extra lines at end of glyph file 'g'"));

  CHECK(display::parse_glyphs("size 1 2 1\n.\n", "g", table, sink).error().text.ends_with("line is too short."));
  CHECK(display::parse_glyphs("size 1 1 1\n \t\n x\n", "g", table, sink).error().text.ends_with(
      "white space is not a valid glyph character"));
  CHECK(display::parse_glyphs("glyphs 1 1 1\n", "g", table, sink).error().text == "Format error in  file 'g'");
}

// Regression: "size 4096 4096 4096" used to allocate count x width x height
// bytes (about 64 GiB, more with per-glyph overhead) before reading a row.
TEST_CASE("a glyph size line beyond the pixel limit is refused without allocating",
          "[cap:display-style-files][req:Glyph file grammar]") {
  display::StyleTable table;
  diag::CollectingSink sink;
  const auto huge = display::parse_glyphs("size 4096 4096 4096\n.\n", "g", table, sink);
  REQUIRE_FALSE(huge.ok());
  CHECK(huge.error().code == display::codes::kGlyphTooLarge);
  // Within the limit, a short file still reports the end-of-file error.
  const auto truncated = display::parse_glyphs("size 4096 16 16\n", "g", table, sink);
  REQUIRE_FALSE(truncated.ok());
  CHECK(truncated.error().code == display::codes::kGlyphEof);
}

// --- file lookup -------------------------------------------------------------------------

TEST_CASE("style files are found by class name, locally first",
          "[cap:display-style-files][req:Dstyle file name and lookup][req:Style file search path]") {
  hefesto::platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("sys/mos.OpenGL.dstyle", "x").ok());
  REQUIRE(fs.write_atomic("sys/mos.OpenGL.std.cmap", "x").ok());
  REQUIRE(fs.write_atomic("sys/color.glyphs", "x").ok());
  REQUIRE(fs.write_atomic("./mos.OpenGL.dstyle", "x").ok());
  const auto found = display::find_style_files(fs, "mos", display::DisplayClass::opengl, {".", "sys"});
  REQUIRE(found.ok());
  CHECK(found.value().dstyle == hefesto::platform::Path(".") / "mos.OpenGL.dstyle");
  CHECK(found.value().cmap == hefesto::platform::Path("sys") / "mos.OpenGL.std.cmap");
}

TEST_CASE("a .dstyle5 file is used as a fallback, else the load fails",
          "[cap:display-style-files][req:Dstyle file name and lookup]") {
  hefesto::platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("sys/mos.OpenGL.std.cmap", "x").ok());
  REQUIRE(fs.write_atomic("sys/color.glyphs", "x").ok());
  const auto none = display::find_style_files(fs, "mos", display::DisplayClass::opengl, {"sys"});
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().text == "Couldn't open display styles file \"mos.OpenGL.dstyle5\"");
  REQUIRE(fs.write_atomic("sys/mos.OpenGL.dstyle5", "x").ok());
  const auto v5 = display::find_style_files(fs, "mos", display::DisplayClass::opengl, {"sys"});
  REQUIRE(v5.ok());
  CHECK(v5.value().dstyle_is_v5);
}

TEST_CASE("a missing color map is reported with the .cmap name", "[cap:display-style-files][req:Color map file grammar]") {
  hefesto::platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("sys/mos.7bit.std.cmap1", "x").ok());
  CHECK(display::find_style_files(fs, "mos", display::DisplayClass::opengl, {"sys"}).error().text ==
        "Couldn't open color map file \"mos.OpenGL.std.cmap\"");
  REQUIRE(fs.write_atomic("sys/mos.7bit.dstyle", "x").ok());
  REQUIRE(fs.write_atomic("sys/color.glyphs", "x").ok());
  const auto found = display::find_style_files(fs, "mos", display::DisplayClass::bit7, {"sys"});
  REQUIRE(found.ok());
  CHECK(found.value().cmap == hefesto::platform::Path("sys") / "mos.7bit.std.cmap1");
}

// --- Magic's distributed files (skipped when build/ref/magic is absent) --------------------

TEST_CASE("Magic's distributed OpenGL styles, color map and glyphs load",
          "[cap:display-style-files][req:Dstyle file grammar][req:Cursor glyph indices]") {
  const std::string dstyle = magic_file("scmos/mos.OpenGL.dstyle");
  if (!std::filesystem::exists(dstyle)) {
    SKIP("Magic sources not cloned into build/ref/magic");
  }
  const auto map = display::parse_color_map(read_text(magic_file("scmos/mos.OpenGL.std.cmap")), "mos.OpenGL.std.cmap", true);
  REQUIRE(map.ok());
  CHECK(map.value().entries.size() >= 256);
  diag::CollectingSink sink;
  const auto table = display::parse_dstyle(read_text(dstyle), "mos.OpenGL.dstyle", map.value(), true, sink);
  REQUIRE(table.ok());
  CHECK(sink.snapshot().empty());
  CHECK(table.value().layout_count > 0);
  CHECK(table.value().styles.size() == 52 + 2 * table.value().layout_count);
  const auto glyphs = display::parse_glyphs(read_text(magic_file("graphics/color.glyphs")), "color.glyphs", table.value(), sink);
  REQUIRE(glyphs.ok());
  CHECK(glyphs.value().size() == 24);
  CHECK(glyphs.value()[static_cast<std::size_t>(display::Cursor::wiring)].width == 16);
}
