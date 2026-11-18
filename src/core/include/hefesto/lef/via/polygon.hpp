// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Polygon painting for the LEF and DEF readers
// (openspec/changes/m6-lef-def-oasis/design.md D4): db::decompose_polygon
// plus the port label a pin polygon carries, shared by LEF POLYGON
// (Q-LEFDEF-23), DEF pin polygons (Q-LEFDEF-66 OFF) and DEF route polygons
// (Q-LEFDEF-70 OFF).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFVIA fills
// in the implementation in src/core/src/lef/via/ (design D2).

#pragma once

#include <span>
#include <string_view>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::lef {

/// Paints the polygon `vertices` (internal units) of `type` into `cell`;
/// when `port_name` is not empty, also places the port label of a pin.
/// SCAFFOLD: fails with CORE-NOTIMPL-006 until LEFVIA lands it.
[[nodiscard]] diag::Status paint_polygon(db::CellDef& cell, tech::TileType type, std::span<const db::Point> vertices,
                                         std::string_view port_name = {});

}  // namespace hefesto::lef
