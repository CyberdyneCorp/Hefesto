// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DRC on generated mask layers (capability drc-cif-rules;
// openspec/changes/m5-mask-gds-cif/design.md D14): Magic's drcCifCheck.
// At the end of basic_check, when the DRC style binds a cifoutput style and
// has at least one mask-layer rule, mask_layer_check generates the bound
// style's layers that the rules use over K grown by the mask halo
// (mask::generate_flat), runs the width and spacing edge checks, the area
// flood (`cifarea`) and the maxwidth flood (`cifmaxwidth`) on those planes,
// and maps each error back to layout coordinates (Q-DRC-13: ON truncates
// toward zero, widens degenerate errors and drops collapsed ones; OFF
// rounds outward). Scratch planes come from the caller's GenerationContext,
// so nothing is allocated per square.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); DRCCIF fills
// in the implementation in src/core/src/drc/check/mask_check*.cpp (design D2).

#pragma once

#include <optional>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/mask/gen/context.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::drc {

/// drcCifCheck over the check rectangle `check_area` (K) of input.cell.
/// Reports nothing when the style has no mask-layer rule.
void mask_layer_check(const CheckInput& input, const db::Rect& check_area, ErrorSink& sink,
                      mask::GenerationContext& ctx);

/// An error rectangle of the generated planes (`mask_rect`, CIF units)
/// mapped back to layout coordinates under `compat` (Q-DRC-13); nullopt
/// when the error collapses and is dropped.
[[nodiscard]] std::optional<db::Rect> map_mask_error(const db::Rect& mask_rect, const mask::OutputStyle& style,
                                                     const compat::CompatSettings& compat);

}  // namespace hefesto::drc
