// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MZTECH-private steps of derive_after_load (route/tech/styles.hpp), with
// the two magic-compat branch points of design D5:
//   - apply_width_statements (Q-LEGACY-33): ON the DRC-derived width wins
//     (Magic), OFF an explicit `width` statement overrides it;
//   - maybe_derive_irouter (Q-LEGACY-37, J2): ON never derives, OFF derives
//     an `irouter` style from the `lef` section when the technology has none.
// Design D5 calls them route::tech::*; they live in hefesto::route so that
// `tech::` keeps naming hefesto::tech inside the route namespace.
//
// Owner: MZTECH.

#pragma once

#include "hefesto/route/tech/styles.hpp"

namespace hefesto::route {

/// MZAfterTech for one style: widths and lengths from DRC, spacings to every
/// defined tile type (Q-LEGACY-35), the explicit non-SUBCELL spacings, the
/// `width` statements (apply_width_statements), the SUBCELL maxima, then
/// the explicit SUBCELL spacings. `rules` may be null (all 0).
void derive_style(MazeStyle& style, const tech::Technology& tech, const drc::RuleSet* rules, bool magic_compat);

/// Q-LEGACY-33 branch point: with magic-compat OFF the style's `width`
/// statements replace the derived width and length of their route types,
/// in file order; with it ON nothing changes.
void apply_width_statements(MazeStyle& style, bool magic_compat);

/// Q-LEGACY-37 branch point: with magic-compat OFF and no style named
/// `irouter` in `styles`, appends the style derived from the technology's
/// `lef` section (router-maze-iroute "Derived irouter style") when it has at
/// least one route layer. Returns whether a style was added. Widths and
/// spacings are left to derive_style.
bool maybe_derive_irouter(MazeStyles& styles, const tech::Technology& tech, const lef::LayerTable& lef_layers,
                          bool magic_compat);

}  // namespace hefesto::route
