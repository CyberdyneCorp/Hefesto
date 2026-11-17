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

#include "hefesto/cif/read/read.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/platform/file_system.hpp"
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
  Session s("style a\nscalefactor 1073741818\nlayer m1 A\nlayer ndc B");  // the largest scale accepted
  auto e = s.engine();
  REQUIRE(e.begin_cell("a").ok());
  REQUIRE(e.paint(0, db::Rect{db::kPlaneMin, db::kPlaneMin, db::kPlaneMax, db::kPlaneMax}).ok());
  REQUIRE(e.paint(1, mask::Polygon{{1, 1}, {db::kPlaneMax, 7}, {3, db::kPlaneMax}}).ok());
  REQUIRE(e.paint(0, mask::Polygon{{0, 0}, {1, 1}}).ok());  // too few vertices: nothing
  CHECK(e.scale_coord(std::numeric_limits<std::int32_t>::max(), mask::Snap::exact) <= db::kPlaneMax);
  CHECK(e.scale_coord(1, mask::Snap::exact) >= 0);
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

namespace {

bool in_plane(const db::Rect& r) {
  return r.x0 >= db::kPlaneMin && r.y0 >= db::kPlaneMin && r.x1 <= db::kPlaneMax && r.y1 <= db::kPlaneMax;
}

}  // namespace

// Fuzz regression (fuzz_cifinput_section): a subdivision of the grid by a
// scale factor near the plane limit multiplied the layout past it, and the
// end-of-read DRC marking transformed a bounding box at INT32_MAX.
TEST_CASE("a grid subdivision near the plane limit keeps the layout in the plane (Q-MASK-34)",
          "[cap:mask-input-styles][req:Conversion of read units to internal units]") {
  Session s("style a\nscalefactor 1073741818\nlayer m1 A");
  s.editor.mask.input_styles = s.inputs;
  auto top = s.editor.cells.create("top", s.editor.technology->plane_count());
  REQUIRE(top.ok());
  top.value()->flags().available = true;
  REQUIRE(db::paint_type(*top.value(), *s.editor.technology, db::Rect{0, 0, 3, 3}, s.type("m1")).ok());
  db::recompute_bbox(*top.value());
  s.editor.edit_cell = top.value();
  hefesto::platform::MemoryFileSystem files;
  hefesto::cif::CifReadOptions options;
  options.files = &files;
  const auto r = hefesto::cif::read_text(
      s.editor, "DS 1 1 1;\n9 child;\nL A; B 200 200 0 100;\nDF;\nC 1 T 0 1000;\nC 1 MX R 0 1 T 3000 0;\nE\n",
      options);
  CHECK(r.ok());
  CHECK(s.editor.grid.internal > 1);  // the read subdivided the grid
  s.editor.cells.for_each([](db::CellDef& def) {
    CHECK(in_plane(def.bbox()));
    for (const auto& use : def.uses()) {
      CHECK(in_plane(use->bbox));
    }
  });
  CHECK(top.value()->bbox().x1 == db::kPlaneMax);  // 3 * 536870909 clamped
}
