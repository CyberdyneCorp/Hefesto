// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the .mag writer shared with the command-script writer
// and the tests: the maximal-strip tiles of a mask-hints plane value, the
// port keywords and the residues of a type (the type itself when it is not
// a contact).

#pragma once

#include <string_view>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::cell::detail {

/// The tiles of a plane-valued property (mask hints) in enumeration order,
/// as Magic stores them in a private plane and writes them back.
[[nodiscard]] std::vector<db::Rect> hint_tiles(const db::PlaneValue& value);

/// The port keywords of a `port` record, each with a leading space; the
/// default shape is empty (the record omits it).
[[nodiscard]] std::string_view use_word(db::PortUse u);
[[nodiscard]] std::string_view class_word(db::PortClass c);
[[nodiscard]] std::string_view shape_word(db::PortShape s);

/// The residue mask of a contact type, or the type alone.
[[nodiscard]] tech::TypeMask residues_of(const tech::Technology& tech, tech::TileType t);

}  // namespace hefesto::cell::detail
