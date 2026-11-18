// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cut growing (capability lef-read "Cut growing";
// openspec/changes/m6-lef-def-oasis/design.md D6, Q-LEFDEF-17): a LEF or DEF
// via cut smaller than the contact size of the current output style
// (mask::OutputStyle::contact_size, CIFGetContactSize) is grown about its
// centre to that size, with Magic's warnings, so the via paints as one
// contact of the layout type.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFVIA fills
// in the implementation in src/core/src/lef/via/ (design D2).

#pragma once

#include "hefesto/db/geometry.hpp"

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::lef {

// The LEF layer table's record (lef/tech/table.hpp, scaffold task 2.5).
struct LayerRecord;

/// `cut`, grown to the contact size `style` gives for `record`'s magic type;
/// `cut` itself when the style has no size for it or the cut is not smaller.
/// SCAFFOLD: returns `cut` until LEFVIA lands it.
[[nodiscard]] db::Rect grow_cut(const db::Rect& cut, const LayerRecord& record, const mask::OutputStyle& style);

}  // namespace hefesto::lef
