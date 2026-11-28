// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: ExtCompareStyle's answer for a `.ext` file's `style` record, as
// the netlisters' reads take it (extflat::ReadOptions::style_units).
// (Magic ref: extflat/EFread.c:252-276, extract/ExtTech.c:286-305 and
// ExtLoadStyle :1080-1112)
//
// Owner: SPICE (openspec/changes/m7-extraction/design.md D3), shared with SIM.

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::netlist::detail {

/// exts_unitsPerLambda of the style NAME as ExtCompareStyle leaves
/// ExtCurStyle: the current style (micron-converted and scaled to the
/// grid) when it has that name, else the named style converted and scaled
/// the way ExtLoadStyle loads it; nullopt for a style the technology does
/// not declare. The parsed style alone keeps the units of the tech file,
/// before the grid scale (`scalegrid 1 2` of the PDK rc files halves them).
[[nodiscard]] std::optional<float> style_units(const editor::EditorState& editor, std::string_view name);

}  // namespace hefesto::netlist::detail
