// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer classes of hierarchical generation (capability
// mask-hierarchical-generation "Classification of hierarchically processed
// layers", "Non-local layers", "Interaction search grid"; design D8): which
// layers are recomputed in interaction areas and which paint types are
// flattened for them (Magic's cs_hierLayers, cs_yankLayers), and the
// non-local (maxrect) sets.
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::mask {

/// The classes of a style's layers, indexed by layer.
struct LayerClasses {
  std::vector<bool> hier;          // HierLayers (cs_hierLayers)
  tech::TypeMask yank;             // YankTypes (cs_yankLayers)
  std::vector<bool> nonlocal;      // the maxrect layers and their readers (cs_nonlocalLayers)
  std::vector<bool> nonlocal_gen;  // NonlocalGen (cs_nonlocalGenLayers)
  tech::TypeMask nonlocal_yank;    // NonlocalYank (cs_nonlocalYankLayers)
  tech::TypeMask nonlocal_seed;    // NonlocalSeed (cs_nonlocalSeedLayers)
};

/// CIFTechFinal's classification. magic-compat OFF also forces grow-min,
/// not-square, orthogonal and maxrect layers (Q-MASK-405) and removes the
/// readers of a bbox or boundary layer transitively (Q-MASK-403).
[[nodiscard]] LayerClasses classify_layers(const OutputStyle& style, const compat::CompatSettings& compat);

/// The side of the interaction squares in internal units: the style's
/// stepsize when positive, else max(20·radius, 50) ("Interaction search
/// grid").
[[nodiscard]] std::int64_t interaction_step(const OutputStyle& style, std::int64_t radius) noexcept;

/// True when hierarchical evaluation stops layer `layer` before its last
/// operator ("Hierarchy-stopping operators").
[[nodiscard]] bool stops_hierarchically(const OutputStyle& style, std::size_t layer) noexcept;

}  // namespace hefesto::mask
