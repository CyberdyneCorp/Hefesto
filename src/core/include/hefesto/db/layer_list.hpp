// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer-list argument of the paint commands (capability
// cmd-paint-erase, requirement "Layer list syntax"): `+`/`-` modes, type
// names and aliases, and the specials `*`, `$`, `errors`, `labels`,
// `subcell`, `connect`. Used by `paint`, `erase`, `select`, `see`, `fill`,
// `drop`, `xor` and every command that takes layers.
// (Magic ref: commands/CmdSubrs.c CmdParseLayers)
//
// Owner: EDIT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db {

/// The subcell pseudo-type (Magic's L_CELL); the label pseudo-type is
/// db::kLabelsPseudoType (db/labels.hpp).
inline constexpr tech::TileType kSubcellPseudoType = tech::kMaxTileTypes - 1;

/// The parsed set. The label and subcell pseudo-types and the DRC error
/// types are reported as flags and in `types` (kLabelsPseudoType,
/// kSubcellPseudoType, tech::kErrorP..kErrorPS).
struct LayerList {
  tech::TypeMask types;
  bool labels = false;
  bool subcells = false;
  bool errors = false;
};

/// What `$` reads: the types visible under the pointer (nullopt when the
/// pointer is not in a layout window; `$` then fails).
struct LayerListContext {
  std::optional<tech::TypeMask> under_pointer;
};

/// Parses `text`; fails with "Ambiguous layer: NAME" or "Unrecognized layer:
/// NAME" followed by the special names ("    $" ... "    connect"), Magic's
/// TxError part. The type names Magic prints before them go to standard
/// output: a command reports tech::layer_names_text() when
/// is_bad_layer_name() holds for the error (CMDEDIT-PAINT-02).
[[nodiscard]] diag::Outcome<LayerList> parse_layer_list(std::string_view text, const tech::Technology& tech,
                                                        const LayerListContext& context = {});

/// Whether `d` is parse_layer_list's unknown or ambiguous name error.
[[nodiscard]] bool is_bad_layer_name(const diag::Diagnostic& d);

}  // namespace hefesto::db
