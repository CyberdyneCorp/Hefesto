// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The non-Manhattan merge option, `cif unfracture` (mask-hierarchical-
// generation "Non-manhattan merge option"): DBMergeNMTiles on each layer
// CIFGen generates (cif/CIFgen.c:5784-5786).
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "gen_fixture.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/mask/ops/evaluate.hpp"

namespace hefesto::mask::test {

namespace {

#define MERGE "[cap:mask-hierarchical-generation][req:Non-manhattan merge option]"

void tri_unmerged(MaskPlane& plane, const Rect& r) {
  // The upper-left side of a '/' split, painted as Magic's plane paint does
  // (no split-tile merge afterwards).
  REQUIRE(plane.plane()
              .paint_triangle(r, db::Diagonal::ne_sw, false, cif_paint_table(), true, nullptr, db::PaintMerge::none)
              .ok());
}

void rect_unmerged(MaskPlane& plane, const Rect& r) {
  REQUIRE(plane.plane().paint(r, cif_paint_table(), db::PaintMerge::none).ok());
}

/// Two collinear '/' halves of the square (x, y)-(x + 2h, y + 2h) and the
/// filled gap above the lower one: one triangle in two split tiles.
void fractured_pair(MaskPlane& plane, db::Coord x, db::Coord y, db::Coord h) {
  tri_unmerged(plane, {x, y, x + h, y + h});
  tri_unmerged(plane, {x + h, y + h, x + 2 * h, y + 2 * h});
  rect_unmerged(plane, {x, y + h, x + h, y + 2 * h});
}

std::size_t split_count(const MaskPlane& plane) {
  std::size_t n = 0;
  for (const db::TileView& t : plane.solid_tiles()) {
    n += t.split ? 1 : 0;
  }
  return n;
}

std::vector<tech::TileType> raster(const MaskPlane& plane, const Rect& box) {
  std::vector<tech::TileType> out;
  for (db::Coord y = box.y0; y < box.y1; ++y) {
    for (db::Coord x = box.x0; x < box.x1; ++x) {
      out.push_back(plane.plane().type_at({x, y}));
    }
  }
  return out;
}

/// A 45-degree m1 wire from (0,0) to (40,40), two units wide, built from
/// triangles and rectangles in internal units.
void diagonal_wire(GenFixture& f, db::CellDef& cell) {
  for (db::Coord i = 0; i < 4; ++i) {
    const db::Coord o = 10 * i;
    REQUIRE(db::paint_type(cell, *f.tech, Rect{o, o, o + 10, o + 10}, f.m1, db::TriangleSpec{db::Diagonal::ne_sw, false})
                .ok());
    REQUIRE(db::paint_type(cell, *f.tech, Rect{o + 2, o, o + 12, o + 10}, f.m1,
                           db::TriangleSpec{db::Diagonal::ne_sw, true})
                .ok());
  }
  db::recompute_bbox(cell);
}

}  // namespace

TEST_CASE("Unfracture changes polygon count, not area", MERGE) {
  for (const bool on : {true, false}) {
    GenFixture f(on);
    const std::size_t layer = f.layer("M1", {types_op(OpKind::op_or, {f.m1})});
    db::CellDef& cell = f.cell("wire");
    diagonal_wire(f, cell);
    HierarchyOptions off;
    HierarchyOptions merged;
    merged.unfracture = true;
    const LayerPlanes a = f.hierarchical(cell, off);
    const LayerPlanes b = f.hierarchical(cell, merged);
    REQUIRE(a[layer] != nullptr);
    REQUIRE(b[layer] != nullptr);
    const Rect box{-10, -10, 530, 530};
    CHECK(raster(*b[layer], box) == raster(*a[layer], box));
    CHECK(b[layer]->solid_tiles().size() <= a[layer]->solid_tiles().size());
    CHECK(f.codes().empty());
  }
}

TEST_CASE("Unfracture merges over Magic's area with magic-compat ON", MERGE) {
  // CIFGen passes its search area, in internal units, to DBMergeNMTiles on a
  // plane in mask units: with scale factor 10 a pair at mask (100,100) lies
  // outside the internal-unit area (-10,-10)-(30,30) and stays fractured.
  const GenerationClip clip = generation_clip(Rect{0, 0, 20, 20}, 10, 10);
  REQUIRE(clip.expanded == Rect{-10, -10, 30, 30});
  const Rect box{90, 90, 210, 210};
  SECTION("magic-compat ON: only the internal-unit area") {
    MaskPlane far;
    fractured_pair(far, 100, 100, 50);
    const std::vector<tech::TileType> before = raster(far, box);
    unfracture_layer(far, clip, 10, compat::CompatSettings{true, true});
    CHECK(split_count(far) == 2);
    CHECK(raster(far, box) == before);
    MaskPlane near;
    fractured_pair(near, 0, 0, 10);
    unfracture_layer(near, clip, 10, compat::CompatSettings{true, true});
    CHECK(split_count(near) == 1);
  }
  SECTION("magic-compat OFF: the whole generated area in mask units") {
    MaskPlane far;
    fractured_pair(far, 100, 100, 50);
    const std::vector<tech::TileType> before = raster(far, box);
    unfracture_layer(far, clip, 10, compat::CompatSettings{false, true});
    REQUIRE(split_count(far) == 1);
    CHECK(far.solid_tiles().front().area == Rect{100, 100, 200, 200});
    CHECK(raster(far, box) == before);
  }
}

TEST_CASE("Unfracture over an unbounded generation area", MERGE) {
  // A side at the plane's edge stays there in both modes (no overflow when
  // scaled).
  const GenerationClip clip = generation_clip(db::whole_plane(), 10, 10);
  for (const bool on : {true, false}) {
    MaskPlane plane;
    fractured_pair(plane, -500, 300, 25);
    unfracture_layer(plane, clip, 10, compat::CompatSettings{on, true});
    CHECK(split_count(plane) == 1);
  }
}

TEST_CASE("The editor's cif unfracture switch reaches generation", MERGE) {
  GenFixture f;
  CHECK_FALSE(f.source().unfracture);
  f.ed.mask.cif.unfracture = true;
  CHECK(f.source().unfracture);
}

}  // namespace hefesto::mask::test
