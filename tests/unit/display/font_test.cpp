// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Outline fonts and font-label geometry (capability font-labels).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#include "hefesto/display/font.hpp"

namespace display = hefesto::display;
namespace db = hefesto::db;

namespace {

/// A tiny Type 3 font in the FontForge layout Magic reads.
constexpr std::string_view kTestFont = R"(%!PS-AdobeFont-1.0: Test
/FontBBox {-10 -200 1000 800 }readonly def
% comment line
/CharProcs 128 dict def
CharProcs begin
  /.notdef { 500 0 0 0 400 700 setcachedevice
	} bind def
  /space { 300 0 0 0 0 0 setcachedevice
	} bind def
  /A { 600 0 0 0 600 700 setcachedevice
	0 0 moveto
	 300 700 lineto
	 600 0 lineto
	 0 0 lineto
	closepath
	} bind def
  /B { 500 0 0 -100 450 700 setcachedevice
	0 0 moveto
	 0 0 0 60 60 60 curveto
	closepath
	} bind def
currentdict end
/Test.Regular exch definefont
)";

display::OutlineFont test_font() {
  auto font = display::parse_outline_font(kTestFont, "Test.pt3");
  REQUIRE(font.ok());
  return font.value();
}

std::optional<std::string> free_sans() {
  const std::string path = std::string(HEFESTO_SOURCE_DIR) + "/build/ref/magic/graphics/FreeSans.pt3";
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

db::Label font_label(std::string text, std::int32_t size, std::int32_t rotation) {
  db::Label label;
  label.text = std::move(text);
  label.rect = db::Rect{0, 0, 0, 0};
  label.justification = db::kCenter;
  label.font = 0;
  label.size = size;
  label.rotation = rotation;
  return label;
}

}  // namespace

TEST_CASE("outline fonts load extents, glyph metrics and the name", "[cap:font-labels][req:Outline font file loading]") {
  auto font = display::parse_outline_font(kTestFont, "Test.pt3", 0.5);
  REQUIRE(font.ok());
  CHECK(font.value().name == "Test");
  CHECK(font.value().extents == db::Rect{-10, -200, 1000, 400});  // top = trunc(800 * 0.5)
  const auto& a = font.value().glyph('A');
  CHECK(a.advance == db::Point{600, 0});
  CHECK(a.bbox == db::Rect{0, 0, 600, 700});
  CHECK(font.value().glyph('\x7f').advance == db::Point{500, 0});  // .notdef
  CHECK(font.value().glyph('\x01').advance == db::Point{500, 0});
  CHECK_FALSE(display::parse_outline_font("/FontBBox {0 0 1 1 } def\n", "x.pt3").ok());
}

TEST_CASE("FreeSans loads as Magic loads it", "[cap:font-labels][req:Outline font file loading]") {
  const auto text = free_sans();
  if (!text) {
    SKIP("Magic sources not cloned into build/ref/magic");
  }
  const auto font = display::parse_outline_font(*text, "FreeSans.pt3", 0.58);
  REQUIRE(font.ok());
  CHECK(font.value().name == "FreeSans");
  CHECK(font.value().extents == db::Rect{-631, -462, 1632, 713});
  CHECK(font.value().glyph('A').advance == db::Point{666, 0});
  CHECK(font.value().glyph('A').bbox == db::Rect{17, 0, 653, 729});
}

TEST_CASE("glyph contours drop repeated points", "[cap:font-labels][req:Glyph contour construction]") {
  const auto font = test_font();
  const auto& a = font.glyph('A');
  REQUIRE(a.contours.size() == 1);
  CHECK(a.contours[0] == std::vector<db::Point>{{0, 0}, {300, 700}, {600, 0}});
  CHECK(font.glyph(' ').contours.empty());

  const auto text = free_sans();
  if (text) {
    const auto sans = display::parse_outline_font(*text, "FreeSans.pt3", 0.58);
    REQUIRE(sans.ok());
    CHECK(sans.value().glyph('!').contours.front() ==
          std::vector<db::Point>{{208, 729}, {208, 391}, {186, 168}, {147, 168}, {125, 391}, {125, 729}});
  }
}

TEST_CASE("Bezier curves are flattened at t = k/6", "[cap:font-labels][req:Bezier flattening]") {
  CHECK(display::flatten_bezier({0, 0}, {0, 0}, {60, 0}, {60, 0}) ==
        std::vector<db::Point>{{4, 0}, {15, 0}, {30, 0}, {44, 0}, {55, 0}, {60, 0}});
  const auto font = test_font();
  CHECK(font.glyph('B').contours.front().size() > 3);
}

TEST_CASE("font label geometry of a centred label", "[cap:font-labels][req:Font label geometry]") {
  const auto text = free_sans();
  if (!text) {
    SKIP("Magic sources not cloned into build/ref/magic");
  }
  const auto font = display::parse_outline_font(*text, "FreeSans.pt3", 0.58).value();
  const auto geo = display::font_label_geometry(font_label("A", 80, 0), font);
  CHECK(geo.text_box == db::Rect{-35, -40, 35, 40});
  CHECK(geo.corners == std::array<db::Point, 4>{db::Point{-35, -40}, {35, -40}, {35, 40}, {-35, 40}});
  CHECK(geo.bbox == db::Rect{-6, -6, 5, 6});

  const auto turned = display::font_label_geometry(font_label("A", 80, 90), font);
  CHECK(turned.corners == std::array<db::Point, 4>{db::Point{40, -35}, {40, 35}, {-40, 35}, {-40, -35}});
  CHECK(turned.bbox == db::Rect{-6, -6, 6, 5});
}

TEST_CASE("font label rotation is normalised once", "[cap:font-labels][req:Font label geometry]") {
  const auto font = test_font();
  CHECK(display::font_label_geometry(font_label("A", 80, -90), font).rotation == 270);
  CHECK(display::font_label_geometry(font_label("A", 80, 360), font).rotation == 0);
  // magic-compat off tests the low three bits only (Q-MISC-20): exact multiples of 8 do not grow.
  auto label = font_label("A", 64, 0);
  label.justification = db::kNorthEast;  // box (0, 0)-(48, 64) in 1/8 units
  label.offset = db::Point{0, 0};
  const auto compat = display::font_label_geometry(label, font, true);
  const auto fixed = display::font_label_geometry(label, font, false);
  CHECK(compat.bbox.y1 == fixed.bbox.y1 + 1);
}

TEST_CASE("font labels render upright", "[cap:font-labels][req:Font label rendering]") {
  const auto font = test_font();
  const auto upright = display::font_label_polygons(font_label("A", 80, 0), font);
  const auto flipped = display::font_label_polygons(font_label("A", 80, 180), font);
  REQUIRE(upright.size() == 1);
  REQUIRE(flipped.size() == 1);
  // The apex of the A (its second point) stays above the base in both.
  CHECK(upright[0][1].y > upright[0][0].y);
  CHECK(flipped[0][1].y > flipped[0][0].y);
}

TEST_CASE("fonts are found by name, case-insensitively", "[cap:font-labels][req:Font-related label commands]") {
  display::FontRegistry fonts;
  fonts.fonts.push_back(std::make_shared<const display::OutlineFont>(test_font()));
  CHECK(fonts.find("test") == 0);
  CHECK(fonts.find("Default") == -1);
  CHECK(fonts.find("FreeSans") == -2);
  db::Label label = font_label("A", 8, 0);
  CHECK(fonts.font_of(label) != nullptr);
  label.font = 5;
  CHECK(fonts.font_of(label) == nullptr);
}
