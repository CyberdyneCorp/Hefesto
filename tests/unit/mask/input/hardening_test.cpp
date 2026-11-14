// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Malformed input for the cifinput reader and the read engine (design R9,
// D21): random statement soup, saturating values, extreme coordinates and
// subdivisions the internal grid cannot hold. Each case must finish without
// a crash or a sanitizer report; the values stay in Magic's int range.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "input_fixture.hpp"

namespace mask = hefesto::mask;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using mask::test::Session;

namespace {

constexpr std::int64_t kIntMax = std::numeric_limits<std::int32_t>::max();

std::string random_section(std::mt19937& rng) {
  static const std::vector<std::string> words = {
      "style",      "variants",   "variant",  "*",          "scalefactor", "nanometers", "angstroms", "gridlimit",
      "layer",      "templayer",  "fault",    "calma",      "gds",         "labels",     "port",      "cell",
      "text",       "ignore",     "options",  "and",        "and-not",     "or",         "grow",      "grow-grid",
      "shrink",     "copyup",     "boundary", "not-square", "mask-hints",  "tagged",     "m1",        "poly",
      "A",          "B,C",        ",,",       "0",          "-1",          "2.5",        "99999999999",
      "2147483647", "255",        "1,2,*",    "(p),(c)",    "",            "ndc",        "x(p)",      "ignore-unknown-layer-labels"};
  std::uniform_int_distribution<std::size_t> pick(0, words.size() - 1);
  std::uniform_int_distribution<int> count(1, 4);
  std::string section;
  for (int line = 0; line < 40; ++line) {
    const int n = count(rng);
    for (int w = 0; w < n; ++w) {
      section += words[pick(rng)] + " ";
    }
    section += "\n";
  }
  return section;
}

}  // namespace

TEST_CASE("random cifinput sections read without failing", "[cap:mask-input-styles][req:Cifinput section grammar]") {
  std::mt19937 rng(20261114);
  const auto technology = mask::test::tech_with("");
  for (int i = 0; i < 200; ++i) {
    const std::string body = random_section(rng);
    tech::RawSection section;
    section.name = "cifinput";
    std::uint32_t line_no = 1;
    std::string line;
    for (const char c : body) {
      if (c != '\n') {
        line.push_back(c);
        continue;
      }
      tech::RawLine raw;
      raw.line = line_no++;
      std::string word;
      for (const char d : line + " ") {
        if (d == ' ') {
          if (!word.empty()) {
            raw.tokens.push_back(word);
          }
          word.clear();
        } else {
          word.push_back(d);
        }
      }
      section.lines.push_back(raw);
      line.clear();
    }
    for (const bool magic : {true, false}) {
      hefesto::diag::CollectingSink sink;
      const mask::InputStyleSet set = mask::read_input_styles(*technology, section, mask::test::mode(magic), sink);
      for (const auto& style : set.styles) {
        CHECK(style->scale_factor >= 1);
        CHECK(style->scale_factor <= kIntMax);
        CHECK(style->multiplier >= 1);
        CHECK(style->cif_layers.size() <= mask::kMaxInputLayers);
        CHECK(style->labels.size() == style->cif_layers.size());
      }
    }
  }
}

TEST_CASE("saturating scale arithmetic", "[cap:mask-input-styles][req:Scale normalization]") {
  const auto r = mask::test::read("style a\nscalefactor 2147483647 angstroms\nlayer m1 A\ngrow 2147483647\n"
                                  "gridlimit 2147483647");
  mask::InputStyle s = r.style();
  (void)s.technology_scale(kIntMax, kIntMax, true);
  CHECK(s.scale_factor <= kIntMax);
  CHECK(s.multiplier <= kIntMax);
  s.read_rescale(kIntMax, 1);
  CHECK(s.scale_factor <= kIntMax);
  CHECK(s.rules[0].ops[1].distance <= kIntMax);
  CHECK(s.limit_scale(kIntMax, kIntMax));  // beyond any int grid: refused
}

TEST_CASE("extreme read coordinates and subdivisions stay bounded",
          "[cap:mask-input-styles][req:Conversion of read units to internal units]") {
  Session s("style a\nscalefactor 2147483647\nlayer m1 A\nlayer ndc B");
  auto e = s.engine();
  REQUIRE(e.begin_cell("a").ok());
  REQUIRE(e.paint(0, db::Rect{db::kPlaneMin, db::kPlaneMin, db::kPlaneMax, db::kPlaneMax}).ok());
  REQUIRE(e.paint(1, mask::Polygon{{1, 1}, {db::kPlaneMax, 7}, {3, db::kPlaneMax}}).ok());
  REQUIRE(e.paint(0, mask::Polygon{{0, 0}, {1, 1}}).ok());  // too few vertices: nothing
  CHECK(e.scale_coord(std::numeric_limits<std::int32_t>::max(), mask::Snap::exact) <= db::kPlaneMax);
  CHECK(e.scale_coord(1, mask::Snap::exact) >= 0);  // a subdivision by ~2^31 would overflow the grid: snapped
  REQUIRE(e.end_cell().ok());
  e.rescale(kIntMax, 1);
  e.rescale(3, 7);
  CHECK(e.style().scale_factor >= 1);
  REQUIRE(e.finish().ok());
  CHECK(s.editor.grid.internal <= kIntMax);
}

TEST_CASE("a huge array does not copy copyup planes", "[cap:mask-input-styles][req:Input operator copyup]") {
  Session s("style a\nscalefactor 1\ntemplayer UP NWELL\ncopyup NWELL\nlayer nwell NWELL");
  auto e = s.engine();
  REQUIRE(e.begin_cell("A").ok());
  REQUIRE(e.paint(1, db::Rect{0, 0, 10, 10}).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.begin_cell("TOP").ok());
  REQUIRE(e.call("A", db::kIdentity, db::ArrayInfo{0, 32767, 0, 32767, 20, 20}).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  bool capped = false;
  for (const auto& d : s.messages.take()) {
    capped = capped || d.code == "MASK-INPUT-017";
  }
  CHECK(capped);
}
