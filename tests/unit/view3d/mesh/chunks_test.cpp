// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Chunking, instance encoding and deterministic emission of the mesh
// (M10 task 6.3; openspec/changes/m10-3d-view/design.md D6 "Encoding",
// "Chunks", "Determinism").

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

#include "mesh_fixture.hpp"
#include "view3d/mesh/internal.hpp"

namespace db = hefesto::db;
namespace view3d = hefesto::view3d;
namespace detail = hefesto::view3d::detail;
using db::Rect;

using view3d::test::grid_stack;
using view3d::test::models_byte_equal;
using view3d::test::paint_grid;

TEST_CASE("Morton codes interleave x in the even bits",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  CHECK(detail::morton(0, 0) == 0U);
  CHECK(detail::morton(1, 0) == 1U);
  CHECK(detail::morton(0, 1) == 2U);
  CHECK(detail::morton(3, 3) == 15U);
  CHECK(detail::morton(0xFFFF, 0xFFFF) == 0xFFFFFFFFU);
}

TEST_CASE("the chunk plan is a quadtree in Morton order covering every tile",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  paint_grid(root, t, false);
  const auto stack = grid_stack(t);
  const Rect clip{-1, -1, 161, 161};
  detail::FlatLayout flat;
  REQUIRE(detail::flatten_layout(stack, root, clip, {}, flat));
  const auto plan = detail::plan_chunks(flat, clip, 16);
  REQUIRE(plan.size() > 4);
  for (std::size_t i = 0; i < plan.size(); ++i) {
    CHECK_FALSE(plan[i].area.is_null());
    CHECK(db::surround(clip, plan[i].area));
    if (i > 0) {
      CHECK(plan[i - 1].id < plan[i].id);
    }
    for (std::size_t j = 0; j < i; ++j) {
      CHECK_FALSE(db::overlap(plan[i].area, plan[j].area));
    }
  }
  // Empty areas are dropped: one chunk at most for an empty plane.
  detail::FlatLayout none;
  CHECK(detail::plan_chunks(none, clip, 16).empty());
  // A small cell is one chunk with the default size.
  const auto one = detail::plan_chunks(flat, clip);
  REQUIRE(one.size() == 1);
  CHECK(one[0].area == clip);
}

TEST_CASE("a chunked build has the same geometry as a single chunk and stays closed",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  paint_grid(root, t, false);
  // The left triangle: its left type is ndiff itself, so it has side faces
  // (a right triangle over space would be drawn flat, as in Magic).
  REQUIRE(db::paint_triangle(root, *t.tech, Rect{0, 100, 160, 140}, db::Diagonal::ne_sw, false, t.ndiff).ok());
  db::recompute_bbox(root);
  const auto stack = grid_stack(t);
  const view3d::ClipArea clip{Rect{-1, -1, 161, 161}, false, 0};
  view3d::BuildContext ctx;
  std::vector<std::uint32_t> seen;
  ctx.on_chunk = [&](const view3d::Chunk& c) { seen.push_back(c.id); };
  double last_fraction = 0.0;
  ctx.progress.report = [&](double f) { last_fraction = f; };
  const auto chunked = detail::build_layout(stack, root, clip, ctx, 16);
  REQUIRE(chunked.has_value());
  REQUIRE(chunked->chunks.size() > 4);
  CHECK(seen.size() == chunked->chunks.size());
  CHECK(last_fraction == 1.0);
  view3d::BuildContext plain;
  const auto single = view3d::build_from_layout(stack, root, clip, plain);
  REQUIRE(single.has_value());
  REQUIRE(single->chunks.size() == 1);
  for (view3d::LayerId l = 0; l < 2; ++l) {
    INFO("layer " << l);
    CHECK(view3d::test::face_area(*chunked, l) == view3d::test::face_area(*single, l));
    // Diagonal pieces have irrational lengths: the sums differ in order.
    CHECK(std::abs(view3d::test::side_length(*chunked, l) - view3d::test::side_length(*single, l)) < 1e-9);
    CHECK(view3d::test::open_edge(*chunked, l).empty());
  }
  for (const view3d::Chunk& c : chunked->chunks) {
    CHECK(c.version == 1U);
    CHECK(c.origin_x == c.area.x0);
    CHECK(c.origin_y == c.area.y0);
    for (const view3d::RectInstance& r : c.rects) {
      CHECK((r.x0 >= 0.0F && r.y0 >= 0.0F && static_cast<double>(r.x1) <= static_cast<double>(c.area.width()) &&
             static_cast<double>(r.y1) <= static_cast<double>(c.area.height())));
    }
  }
}

TEST_CASE("two builds of the same cell are byte-equal",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef a("a", t.tech->plane_count());
  db::CellDef b("b", t.tech->plane_count());
  paint_grid(a, t, false);
  paint_grid(b, t, true);  // the same geometry painted in the other order
  const auto stack = grid_stack(t);
  const view3d::ClipArea clip{Rect{-1, -1, 161, 161}, false, 0};
  for (const std::size_t size : {std::size_t{16}, detail::kMaxChunkTiles}) {
    INFO("chunk size " << size);
    view3d::BuildContext c1;
    view3d::BuildContext c2;
    view3d::BuildContext c3;
    const auto first = detail::build_layout(stack, a, clip, c1, size);
    const auto second = detail::build_layout(stack, a, clip, c2, size);
    const auto other = detail::build_layout(stack, b, clip, c3, size);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(other.has_value());
    CHECK(models_byte_equal(*first, *second));
    CHECK(models_byte_equal(*first, *other));
  }
}

TEST_CASE("instances keep sub-unit precision near 2^30",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  const db::Coord far = db::kPlaneMax - 1000;  // about 2^30
  const db::Coord near_min = db::kPlaneMin + 1000;
  view3d::test::paint(root, t, Rect{far, far, far + 7, far + 3}, t.metal1);
  view3d::test::paint(root, t, Rect{near_min, near_min, near_min + 5, near_min + 9}, t.metal1);
  // Clear of the rectangle's horizontal strips, so the paint keeps one
  // split tile (an off-grid triangle would be cut into steps).
  const Rect tri{far + 100, far - 100, far + 103, far - 93};
  REQUIRE(db::paint_triangle(root, *t.tech, tri, db::Diagonal::nw_se, false, t.metal1).ok());
  db::recompute_bbox(root);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  // Almost the whole usable plane, 2^31 units wide.
  const Rect clip{db::kPlaneMin, db::kPlaneMin, db::kPlaneMax, db::kPlaneMax};
  const auto mesh = view3d::test::build(stack, root, clip, false);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->chunks.size() >= 2);
  for (const view3d::Chunk& c : mesh->chunks) {
    CHECK(c.area.width() <= detail::kMaxChunkSide);
    CHECK(c.area.height() <= detail::kMaxChunkSide);
  }
  using view3d::test::Seg;
  const auto rects = view3d::test::rects_of(*mesh, 0);
  CHECK(rects == std::vector<Seg>{{static_cast<double>(near_min), static_cast<double>(near_min),
                                   static_cast<double>(near_min) + 5, static_cast<double>(near_min) + 9, 0},
                                  {static_cast<double>(far), static_cast<double>(far), static_cast<double>(far) + 7,
                                   static_cast<double>(far) + 3, 0}});
  CHECK(view3d::test::face_area(*mesh, 0) == 21.0 + 45.0 + 10.5);
  CHECK(view3d::test::open_edge(*mesh, 0).empty());
  // The triangle's diagonal is exact to well under a unit.
  bool found = false;
  for (const Seg& s : view3d::test::sides_of(*mesh, 0)) {
    if ((s.flags & view3d::side_flag::diagonal) != 0) {
      found = true;
      CHECK(s.x0 == static_cast<double>(far) + 100);
      CHECK(s.y0 == static_cast<double>(far) - 93);
      CHECK(s.x1 == static_cast<double>(far) + 103);
      CHECK(s.y1 == static_cast<double>(far) - 100);
    }
  }
  CHECK(found);
}
