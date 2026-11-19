// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SelectChunk over a flat cell, for the -hide 0 pin rectangles of the LEF
// writer (capability lef-write "Hidden-detail pin geometry with zero
// setback"; chunk.cpp).
//
// Owner: LEFW (design D2).

#pragma once

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef::write_detail {

/// The chunk rectangle of `type` containing `area` in `flat` (SelectChunk's
/// pArea): the rectangle of a split tile of the type under `area`, else the
/// largest rectangle of the type (and the stacked contacts holding it) that
/// contains `area`; a null rectangle when there is none or `type` is space.
[[nodiscard]] db::Rect chunk_rect(const tech::Technology& tech, const db::CellDef& flat, const db::Rect& area,
                                  tech::TileType type);

}  // namespace hefesto::lef::write_detail
