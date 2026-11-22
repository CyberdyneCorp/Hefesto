// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Incremental rebuild, progressive order and cancellation of the mesh
// build (M10 task 6.4; openspec/changes/m10-3d-view/design.md D4 "Change
// notification", D6 "Chunks").

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

#include "mesh_fixture.hpp"
#include "view3d/mesh/internal.hpp"

namespace db = hefesto::db;
namespace view3d = hefesto::view3d;
namespace detail = hefesto::view3d::detail;
using db::Rect;

namespace {

const view3d::Chunk* find_chunk(const view3d::MeshModel& m, std::uint32_t id) {
  for (const view3d::Chunk& c : m.chunks) {
    if (c.id == id) {
      return &c;
    }
  }
  return nullptr;
}

bool meets(const Rect& a, const Rect& b) {
  return !detail::intersect(a, b).is_null();
}

// The rebuilt model has the geometry of a fresh build of the same cell.
void check_same_geometry(const view3d::MeshModel& rebuilt, const view3d::MeshModel& fresh, view3d::LayerId layers) {
  for (view3d::LayerId l = 0; l < layers; ++l) {
    INFO("layer " << l);
    CHECK(view3d::test::face_area(rebuilt, l) == view3d::test::face_area(fresh, l));
    CHECK(std::abs(view3d::test::side_length(rebuilt, l) - view3d::test::side_length(fresh, l)) < 1e-9);
    CHECK(view3d::test::open_edge(rebuilt, l).empty());
  }
}

std::optional<view3d::MeshModel> build_small(const view3d::LayerStack& stack, const db::CellDef& cell,
                                             const view3d::ClipArea& clip, std::size_t max_tiles) {
  view3d::BuildContext ctx;
  return detail::build_layout(stack, cell, clip, ctx, max_tiles);
}

std::optional<view3d::MeshModel> rebuild_small(const view3d::MeshModel& previous, const view3d::LayerStack& stack,
                                               const db::CellDef& cell, const Rect& dirty, std::size_t max_tiles) {
  view3d::BuildContext ctx;
  const std::vector<Rect> areas{dirty};
  return detail::rebuild_layout(previous, stack, cell, areas, ctx, max_tiles);
}

}  // namespace

TEST_CASE("a local paint rebuilds only the affected chunks",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint_grid(root, t, false);
  const auto stack = view3d::test::grid_stack(t);
  const view3d::ClipArea clip{Rect{-1, -1, 161, 161}, false, 0};
  const auto before = build_small(stack, root, clip, 16);
  REQUIRE(before.has_value());
  REQUIRE(before->chunks.size() > 4);

  // A tab on the right side of the square (10,10)-(16,16).
  const Rect dirty{16, 11, 18, 13};
  view3d::test::paint(root, t, dirty, t.metal1);
  const auto after = rebuild_small(*before, stack, root, dirty, 16);
  REQUIRE(after.has_value());
  CHECK(after->version > before->version);
  std::size_t rebuilt = 0;
  for (const view3d::Chunk& c : after->chunks) {
    INFO("chunk " << c.id);
    if (meets(c.area, db::expand(dirty, 1))) {
      ++rebuilt;
      CHECK(c.version == after->version);
      continue;
    }
    const view3d::Chunk* old = find_chunk(*before, c.id);
    REQUIRE(old != nullptr);
    CHECK(view3d::test::chunks_byte_equal(c, *old));
    CHECK(c.version == old->version);  // only rebuilt chunks take the new version
  }
  CHECK(rebuilt >= 1);
  CHECK(rebuilt < after->chunks.size());
  CHECK(after->chunks.size() == before->chunks.size());
  for (std::size_t i = 1; i < after->chunks.size(); ++i) {
    CHECK(after->chunks[i - 1].id < after->chunks[i].id);
  }
  const auto fresh = build_small(stack, root, clip, 16);
  REQUIRE(fresh.has_value());
  check_same_geometry(*after, *fresh, 2);
}

TEST_CASE("paint outside every chunk adds a chunk and erasing a chunk's paint drops it",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint(root, t, Rect{0, 0, 10, 10}, t.metal1);
  view3d::test::paint(root, t, Rect{900, 900, 910, 910}, t.metal1);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  const view3d::ClipArea clip{Rect{-1, -1, 1001, 1001}, false, 0};
  const auto first = build_small(stack, root, clip, 1);
  REQUIRE(first.has_value());
  const std::size_t first_count = first->chunks.size();

  SECTION("new paint in an empty node") {
    // The upper-left quadrant held no paint, so the first build has no
    // chunk there.
    const Rect added{100, 900, 110, 910};
    view3d::test::paint(root, t, added, t.metal1);
    const auto after = rebuild_small(*first, stack, root, added, 1);
    REQUIRE(after.has_value());
    CHECK(after->chunks.size() > first_count);
    for (const view3d::Chunk& c : first->chunks) {
      const view3d::Chunk* kept = find_chunk(*after, c.id);
      REQUIRE(kept != nullptr);
      CHECK(view3d::test::chunks_byte_equal(c, *kept));
      CHECK(kept->version == c.version);
    }
    const auto fresh = build_small(stack, root, clip, 1);
    REQUIRE(fresh.has_value());
    check_same_geometry(*after, *fresh, 1);
    CHECK(view3d::test::face_area(*after, 0) == 300.0);
  }
  SECTION("erasing all the paint of a chunk") {
    const Rect gone{0, 0, 10, 10};
    REQUIRE(db::erase_type(root, *t.tech, gone, t.metal1).ok());
    const auto after = rebuild_small(*first, stack, root, gone, 1);
    REQUIRE(after.has_value());
    CHECK(after->chunks.size() < first_count);
    for (const view3d::Chunk& c : after->chunks) {
      CHECK_FALSE(meets(c.area, gone));
    }
    CHECK(view3d::test::face_area(*after, 0) == 100.0);
    CHECK(view3d::test::open_edge(*after, 0).empty());
  }
}

TEST_CASE("a change outside the region keeps the model; a cancelled rebuild returns none",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint(root, t, Rect{0, 0, 10, 10}, t.metal1);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  const auto first = view3d::test::build(stack, root, Rect{0, 0, 20, 20});
  REQUIRE(first.has_value());
  const std::vector<Rect> outside{Rect{100, 100, 110, 110}};
  view3d::BuildContext ctx;
  const auto same = view3d::rebuild_chunks(*first, stack, root, outside, ctx);
  REQUIRE(same.has_value());
  CHECK(view3d::test::models_byte_equal(*same, *first));
  CHECK(same->version == first->version);

  std::atomic<bool> cancel{true};
  view3d::BuildContext cancelled;
  cancelled.progress.cancel = &cancel;
  const std::vector<Rect> inside{Rect{2, 2, 4, 4}};
  CHECK_FALSE(view3d::rebuild_chunks(*first, stack, root, inside, cancelled).has_value());
}

TEST_CASE("chunks are shown nearest the view centre first and kept in Morton order",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint_grid(root, t, false);
  const auto stack = view3d::test::grid_stack(t);
  const view3d::ClipArea clip{Rect{-1, -1, 161, 161}, false, 0};
  view3d::BuildContext ctx;
  ctx.centre = db::Point{150, 150};
  std::vector<Rect> shown;
  ctx.on_chunk = [&](const view3d::Chunk& c) { shown.push_back(c.area); };
  const auto mesh = detail::build_layout(stack, root, clip, ctx, 16);
  REQUIRE(mesh.has_value());
  REQUIRE(shown.size() == mesh->chunks.size());
  const auto d2 = [](const Rect& r) {
    const double dx = static_cast<double>(r.x0 + r.x1) - 300.0;
    const double dy = static_cast<double>(r.y0 + r.y1) - 300.0;
    return (dx * dx) + (dy * dy);
  };
  for (std::size_t i = 1; i < shown.size(); ++i) {
    CHECK(d2(shown[i - 1]) <= d2(shown[i]));
  }
  for (std::size_t i = 1; i < mesh->chunks.size(); ++i) {
    CHECK(mesh->chunks[i - 1].id < mesh->chunks[i].id);
  }
  // The order of display does not change the model.
  view3d::BuildContext plain;
  const auto same = detail::build_layout(stack, root, clip, plain, 16);
  REQUIRE(same.has_value());
  CHECK(view3d::test::models_byte_equal(*mesh, *same));
}

TEST_CASE("a build cancelled between chunks returns none",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint_grid(root, t, false);
  const auto stack = view3d::test::grid_stack(t);
  std::atomic<bool> cancel{false};
  view3d::BuildContext ctx;
  ctx.progress.cancel = &cancel;
  std::size_t seen = 0;
  ctx.on_chunk = [&](const view3d::Chunk&) {
    if (++seen == 2) {
      cancel = true;
    }
  };
  CHECK_FALSE(detail::build_layout(stack, root, view3d::ClipArea{Rect{-1, -1, 161, 161}, false, 0}, ctx, 16));
  CHECK(seen == 2);
}
