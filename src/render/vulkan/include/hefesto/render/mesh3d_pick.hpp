// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pick ids Mesh3dRenderer writes to the R32_UINT id image
// (hefesto-3d-visualization "Interactive 3D inspection" (picking);
// openspec/changes/m10-3d-view/design.md D10, tasks.md 5.4, 9.3).
//
// The encoding moved to the core, hefesto/view3d/pick/pick_id.hpp (M10
// round 3, INSPECT), so picking and the iPadOS renderer share it; this
// header forwards its names into hefesto::render so the renderer sources
// and tests compile unchanged. The definitions live in the core
// (src/core/src/view3d/pick/pick_id.cpp).
//
// Owner: INSPECT (forwarding header); the renderer is RENDER3D's.

#pragma once

#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/pick/pick_id.hpp"

namespace hefesto::render {

using view3d::decode_pick;
using view3d::encode_pick;
using view3d::pick_instance_count;
using view3d::pick_local_index;
using view3d::PickKind;
using view3d::PickRef;

}  // namespace hefesto::render
