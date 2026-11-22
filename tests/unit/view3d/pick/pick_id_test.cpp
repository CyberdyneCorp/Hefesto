// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pick ids (view3d/pick/pick_id.hpp; M10 tasks 5.4, 9.3): moved from the
// Vulkan renderer's mesh3d_pick.hpp, which forwards to them.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/view3d/pick/pick_id.hpp"

namespace view3d = hefesto::view3d;
using view3d::PickKind;
using view3d::PickRef;

TEST_CASE("pick ids number every instance of the model from 1, chunk by chunk, in the core",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  view3d::MeshModel mesh;
  mesh.chunks.resize(2);
  mesh.chunks[0].rects.resize(2);
  mesh.chunks[0].sides.resize(3);
  mesh.chunks[1].triangles.resize(1);
  mesh.chunks[1].rects.resize(1);
  CHECK(view3d::pick_instance_count(mesh.chunks[0]) == 5);
  CHECK(view3d::pick_local_index(mesh.chunks[1], PickKind::triangle, 0) == 1);
  CHECK(view3d::encode_pick(mesh, PickRef{0, PickKind::rect, 0}) == 1);
  CHECK(view3d::encode_pick(mesh, PickRef{0, PickKind::side, 2}) == 5);
  CHECK(view3d::encode_pick(mesh, PickRef{1, PickKind::rect, 0}) == 6);
  CHECK(view3d::encode_pick(mesh, PickRef{1, PickKind::triangle, 0}) == 7);
  CHECK(view3d::encode_pick(mesh, PickRef{1, PickKind::side, 0}) == view3d::kNoPick);
  CHECK(view3d::encode_pick(mesh, PickRef{2, PickKind::rect, 0}) == view3d::kNoPick);
  for (view3d::PickId id = 1; id <= 7; ++id) {
    const auto ref = view3d::decode_pick(mesh, id);
    REQUIRE(ref.has_value());
    CHECK(view3d::encode_pick(mesh, *ref) == id);
  }
  CHECK_FALSE(view3d::decode_pick(mesh, view3d::kNoPick).has_value());
  CHECK_FALSE(view3d::decode_pick(mesh, 8).has_value());
}

TEST_CASE("an empty chunk takes no ids", "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  view3d::MeshModel mesh;
  mesh.chunks.resize(3);
  mesh.chunks[0].sides.resize(1);
  mesh.chunks[2].rects.resize(1);
  CHECK(view3d::decode_pick(mesh, 2) == PickRef{2, PickKind::rect, 0});
  CHECK(view3d::encode_pick(mesh, PickRef{2, PickKind::rect, 0}) == 2);
}
