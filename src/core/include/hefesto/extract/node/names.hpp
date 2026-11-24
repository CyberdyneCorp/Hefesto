// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Node names (capability extract-nodes-and-labels "Node reference point and
// generated name", "Node name selection"; openspec/changes/m7-extraction
// design.md D8). Name selection follows Q-EXTRACT-4: Magic's list order
// with magic-compat ON, the first attached name with it OFF, in one branch.
//
// Also the label roles ("Label types by trailing character") and the
// label-list insertion of "Label attachment", where Q-EXTRACT-4's branch
// lives. generated_name() does not apply the substrate name rule of
// extract-substrate (extSubsName): callers name the substrate node.
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

/// Added in M7 R3: the role of a label by its last character (extLabType):
/// `@` node attribute, `$` terminal attribute, `^` gate attribute, any
/// other a node name; an empty text has none.
enum class LabelRole : std::uint8_t { none, name, node_attribute, terminal_attribute, gate_attribute };
[[nodiscard]] LabelRole label_role(std::string_view text) noexcept;

/// Added in M7 R3: where a newly attached label goes in a node's label
/// list ("Label attachment"; Q-EXTRACT-4). A port goes to the head. A
/// non-port goes to the head of an empty list; otherwise, with
/// magic-compat ON, after the run of ports at the head or, when the head is
/// not a port, after the head (Magic's order); with it OFF, at the end
/// (attachment order).
[[nodiscard]] std::size_t label_insert_position(const std::vector<LabelRegion>& labels, bool port,
                                                const compat::CompatSettings& compat) noexcept;

/// Added in M7 R3: the first label of `node` with the name role, or
/// nullptr (extNodeName's choice).
[[nodiscard]] const LabelRegion* name_label(const NodeRegion& node) noexcept;

/// The node's name: its selected label, or generated_name() without one.
[[nodiscard]] std::string node_name(const NodeRegion& node, const tech::Technology& technology,
                                    const compat::CompatSettings& compat);

/// extMakeNodeNumPrint: `<plane>_<x>_<y>#` with `n` for a negative
/// coordinate (`metal1_n10_n6#`).
[[nodiscard]] std::string generated_name(const NodeRegion& node, const tech::Technology& technology);

}  // namespace hefesto::extract
