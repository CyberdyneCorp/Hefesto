// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cut growing (capability lef-read "Cut growing";
// openspec/changes/m6-lef-def-oasis/design.md D6, Q-LEFDEF-17): a LEF or DEF
// via cut on a contact type is resized about its centre to the contact size
// of the current output style (mask::OutputStyle::contact_cut,
// CIFGetContactSize), with Magic's warnings when its size differs from the
// style's cut size, so the via paints as one contact of the layout type.
// The input style plays no part (LefGrowVia, lef/lefRead.c:2473-2523).
//
// Owner: LEFVIA (design D2).

#pragma once

#include "hefesto/db/geometry.hpp"
#include "hefesto/lef/via/context.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::lef {

/// `cut` (half internal units) on `type`, grown to the output style's
/// contact size: C = ceil(2(size + 2 border)/F) and E = ceil(2 size/F) half
/// units; when both are positive the result is the C x C square at the
/// truncated centre of `cut`, with the warnings LEF-VIA-003/004 when `cut`
/// is not E x E. `cut` itself when `type` is not a contact, no style is
/// current or the style generates no cut for `type`.
///
/// `named_type` is the type the first warning names: with magic-compat ON
/// the via entry's normal type at that moment, as Magic prints
/// DBTypeLongNameTbl[lefl->type] (`type` when the entry has none yet, where
/// Magic reads before its table); with magic-compat OFF the caller passes
/// `type`, the contact that is grown.
[[nodiscard]] db::Rect grow_cut(const db::Rect& cut, tech::TileType type, tech::TileType named_type,
                                const ViaContext& context);

}  // namespace hefesto::lef
