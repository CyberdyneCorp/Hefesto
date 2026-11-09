// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the .mag writer shared with the command-script writer
// and the tests: the maximal-strip tiles of a mask-hints plane value and
// the residues of a type (the type itself when it is not a contact).

#pragma once

#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::cell::detail {

/// The tiles of a plane-valued property (mask hints) in enumeration order,
/// as Magic stores them in a private plane and writes them back.
[[nodiscard]] std::vector<db::Rect> hint_tiles(const db::PlaneValue& value);

/// The residue mask of a contact type, or the type alone.
[[nodiscard]] tech::TypeMask residues_of(const tech::Technology& tech, tech::TileType t);

}  // namespace hefesto::cell::detail
