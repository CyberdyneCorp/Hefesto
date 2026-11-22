// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Node names (capability extract-nodes-and-labels "Node reference point and
// generated name", "Node name selection"; openspec/changes/m7-extraction
// design.md D8). Name selection follows Q-EXTRACT-4: Magic's list order
// with magic-compat ON, the first attached name with it OFF, in one branch.
//
// Scaffold state (tasks.md 3.3): both functions answer the empty string
// until EXTNODE lands them (tasks.md 5.2, 5.3).
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <string>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

/// The node's name: its selected label, or generated_name() without one.
[[nodiscard]] std::string node_name(const NodeRegion& node, const tech::Technology& technology,
                                    const compat::CompatSettings& compat);

/// extMakeNodeNumPrint: `<plane>_<x>_<y>#` with `n` for a negative
/// coordinate (`metal1_n10_n6#`).
[[nodiscard]] std::string generated_name(const NodeRegion& node, const tech::Technology& technology);

}  // namespace hefesto::extract
