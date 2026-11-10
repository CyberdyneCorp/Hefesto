// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// wiring-tool "wire segment": the coordinate-list and file forms, end caps,
// the usage and error texts, the centerline-path polygons and the file
// reader.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/diag/codes.hpp"
#include "hefesto/wiring/diag_codes.hpp"
#include "hefesto/wiring/wiring.hpp"
#include "wire_fixture.hpp"

namespace wiring = hefesto::wiring;
namespace db = hefesto::db;
namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
using db::Point;
using db::Rect;
using wiring::test::last_text;
using wiring::test::Session;

#define SEGMENT "[cap:wiring-tool][req:wire segment]"

namespace {

cmd::CoordinateUnits internal_units() {
  cmd::CoordinateUnits u;
  u.default_unit = cmd::DefaultUnit::internal;
  return u;
}

}  // namespace

TEST_CASE("horizontal segment with end caps", SEGMENT) {
  Session s;
  s.ok("wire segment metal1 4 0 0 20 0");
  CHECK(s.rects("metal1") == std::vector<Rect>{{-2, -2, 22, 2}});
  CHECK(s.top->flags().modified);
  CHECK(s.top->bbox() == Rect{-2, -2, 22, 2});
  CHECK_FALSE(s.editor.wire.type.has_value());  // the wire state is not used
}

TEST_CASE("segment without end caps", SEGMENT) {
  Session s;
  s.ok("wire segment metal1 4 0 0 20 0 -noendcap");
  CHECK(s.rects("metal1") == std::vector<Rect>{{0, -2, 20, 2}});
}

TEST_CASE("segment centerlines on half-grid points are exact", SEGMENT) {
  Session s;
  // Edges at x = -1.5 and 1.5 before rounding; Magic's 1.5708 right angle
  // puts them a hair inside, so they round to -1 and 1.
  s.ok("wire segment metal1 3 0 0 0 20 -noendcap");
  CHECK(s.rects("metal1") == std::vector<Rect>{{-1, 0, 1, 20}});
  // A centerline at x = 0.5 (half unit 1): edges at -1.5 and 2.5 before rounding.
  const std::vector<Point> half{{1, 40}, {1, 0}};
  const auto polys = wiring::wire_path_polygons(half, 4, false);
  REQUIRE(polys.size() == 1);
  CHECK(polys[0] == std::vector<Point>{{2, 0}, {2, 20}, {-1, 20}, {-1, 0}});
}

TEST_CASE("usage lists the coordinates in order", SEGMENT) {
  Session s;
  const auto r = s.run("wire segment metal1 4 0 0");
  CHECK_FALSE(r.ok());
  CHECK(last_text(r) == "Usage: wire segment layer width x1 y1 x2 y2\n (or)  wire segment layer width filename");
  CHECK(s.top->plane(hefesto::tech::kFirstUserPlane + 2).empty());
  CHECK(last_text(s.run("wire segment metal1 4 0 0 20")).starts_with("Usage: wire segment"));
  CHECK(last_text(s.run("wire segment metal1 4 0 0 20 0 5")).starts_with("Usage: wire segment"));
  for (const bool compat : {true, false}) {
    Session t;
    t.project.compat.magic_compat = compat;
    CHECK(last_text(t.run("wire segment")).starts_with("Usage: wire segment layer width x1 y1 x2 y2\n"));
  }
}

TEST_CASE("segment argument errors", SEGMENT) {
  Session s;
  auto r = s.run("wire segment nosuch 4 0 0 20 0");
  CHECK(last_text(r) == "Unrecognized layer (type) name \"nosuch\"");
  CHECK(r.diagnostics.back().code == wiring::codes::kBadLayer);
  CHECK(last_text(s.run("wire segment met 4 0 0 20 0")) == "Ambiguous layer (type) name \"met\"");
  CHECK(last_text(s.run("wire segment metal1 wide 0 0 20 0")) == "Route segment width must be a numeric value");
  s.editor.edit_cell = nullptr;
  CHECK(last_text(s.run("wire segment metal1 4 0 0 20 0")) == "No cell being edited");
}

TEST_CASE("segment file form", SEGMENT) {
  Session s;
  auto& fs = s.services.view().files;
  REQUIRE(fs.write_atomic("/data/path.txt", "0 0\n  20\t0   trailing words\n").ok());
  s.ok("wire segment metal1 4 /data/path.txt");
  CHECK(s.rects("metal1") == std::vector<Rect>{{-2, -2, 22, 2}});
  Session t;
  REQUIRE(t.services.view().files.write_atomic("/data/path.txt", "0 0\n20 0\n").ok());
  t.ok("wire segment metal1 4 /data/path.txt -noendcap");
  CHECK(t.rects("metal1") == std::vector<Rect>{{0, -2, 20, 2}});
  REQUIRE(t.services.view().files.write_atomic("/data/bad.txt", "0 0\n20\n").ok());
  auto r = t.run("wire segment metal1 4 /data/bad.txt");
  CHECK(last_text(r) == "Bad coordinate pair at /data/bad.txt line 2");
  CHECK(r.diagnostics.back().code == wiring::codes::kFileCoordinate);
  r = t.run("wire segment metal1 4 /data/none.txt");
  CHECK(last_text(r) == "No such file or error opening /data/none.txt");
  CHECK(r.diagnostics.back().code == wiring::codes::kFileOpen);
}

TEST_CASE("segment file lines are read as fgets reads them", SEGMENT) {
  const auto u = internal_units();
  auto points = wiring::read_segment_points("1 2\n3 4", "f", u);
  REQUIRE(points.ok());
  CHECK(points.value() == std::vector<Point>{{2, 4}, {6, 8}});  // half units
  // A line longer than 127 bytes continues as the next line.
  const std::string longline = std::string(125, ' ') + "5 6\n";
  points = wiring::read_segment_points(longline, "f", u);
  REQUIRE_FALSE(points.ok());
  CHECK(points.error().text == "Bad coordinate pair at f line 1");  // "  ...  5 " then "6\n"
  CHECK_FALSE(wiring::read_segment_points("   \n", "f", u).ok());
  CHECK_FALSE(wiring::read_segment_points("7", "f", u).ok());
  CHECK(wiring::read_segment_points("", "f", u).value().empty());
  // A word that is not a number is taken as 0 with Magic's warning.
  diag::CollectingSink sink;
  points = wiring::read_segment_points("x 1\n", "f", u, &sink);
  REQUIRE(points.ok());
  CHECK(points.value() == std::vector<Point>{{0, 2}});
  const auto notes = sink.take();
  REQUIRE_FALSE(notes.empty());
  CHECK(notes.back().text == "Coordinate value cannot be parsed:  assuming 0");
}

TEST_CASE("the centerline path polygon of a bend", SEGMENT) {
  // PaintWireList's order: the points of `0 0 20 0 20 20` reversed, in half units.
  const std::vector<Point> path{{40, 40}, {40, 0}, {0, 0}};
  const auto polys = wiring::wire_path_polygons(path, 4, true);
  REQUIRE(polys.size() == 1);
  CHECK(polys[0] == std::vector<Point>{{-2, -2}, {22, -2}, {22, 22}, {18, 22}, {18, 2}, {-2, 2}});
}

TEST_CASE("diagonal and mitred path polygons", SEGMENT) {
  // Reference values from an independent transcription of CIFPaintWirePath.
  const std::vector<Point> diagonal{{40, 40}, {0, 0}};  // `0 0 20 20`
  auto polys = wiring::wire_path_polygons(diagonal, 4, true);
  REQUIRE(polys.size() == 1);
  CHECK(polys[0] == std::vector<Point>{{0, -3}, {23, 20}, {20, 23}, {-3, 0}});
  const std::vector<Point> zigzag{{80, 40}, {40, 0}, {0, 0}};  // `0 0 20 0 40 20`
  polys = wiring::wire_path_polygons(zigzag, 4, true);
  REQUIRE(polys.size() == 1);
  CHECK(polys[0] == std::vector<Point>{{-2, -2}, {21, -2}, {43, 20}, {40, 23}, {19, 2}, {-2, 2}});
}

TEST_CASE("a single-point path is a square; repeated points are dropped", SEGMENT) {
  const std::vector<Point> one{{20, 20}};
  auto polys = wiring::wire_path_polygons(one, 4, true);
  REQUIRE(polys.size() == 1);
  CHECK(polys[0] == std::vector<Point>{{12, 12}, {8, 12}, {8, 8}, {12, 8}});
  Session s;
  s.ok("wire segment metal1 4 10 10 10 10");
  CHECK(s.rects("metal1") == std::vector<Rect>{{8, 8, 12, 12}});
  const std::vector<Point> repeated{{0, 0}, {40, 0}, {40, 0}};
  polys = wiring::wire_path_polygons(repeated, 4, false);
  REQUIRE(polys.size() == 1);
  CHECK(polys[0].size() == 4);
}

TEST_CASE("a path that reverses direction is split there with a warning", SEGMENT) {
  diag::CollectingSink sink;
  const std::vector<Point> path{{20, 0}, {40, 0}, {0, 0}};  // `0 0 20 0 10 0` reversed
  const auto polys = wiring::wire_path_polygons(path, 4, false, &sink);
  CHECK(polys.size() == 2);
  const auto notes = sink.take();
  REQUIRE(notes.size() == 1);
  CHECK(notes[0].code == wiring::codes::kPathReversal);
  CHECK(notes[0].text == "Warning: direction reversal in path at (20, 0).");
}

TEST_CASE("a bend sharper than the mitre limit truncates the route", SEGMENT) {
  diag::CollectingSink sink;
  // A hairpin of about 166 degrees: |cos(alpha)| < 0.25.
  const std::vector<Point> path{{0, 0}, {400, 0}, {0, 100}};
  const auto polys = wiring::wire_path_polygons(path, 4, true, &sink);
  CHECK(polys.empty());  // the open polygon is dropped, as in Magic
  const auto notes = sink.take();
  REQUIRE(notes.size() == 1);
  CHECK(notes[0].code == wiring::codes::kPathMitre);
  CHECK(notes[0].text == "Error: mitre limit exceeded at wire junction at (400, 0).\nRoute has been truncated.");
}

TEST_CASE("a bent segment is painted through the polygon decomposer", SEGMENT) {
  Session s;
  const auto r = s.run("wire segment metal1 4 0 0 20 0 20 20");
  REQUIRE(r.ok());
  const auto& plane = s.top->plane(*s.editor.technology->home_plane(s.type("metal1")));
  CHECK(plane.type_at({-2, -2}) == s.type("metal1"));
  CHECK(plane.type_at({21, 21}) == s.type("metal1"));
  CHECK(plane.type_at({17, 3}) == hefesto::tech::kSpace);
  CHECK(plane.type_at({22, 0}) == hefesto::tech::kSpace);
  CHECK(s.top->bbox() == Rect{-2, -2, 22, 22});
}

