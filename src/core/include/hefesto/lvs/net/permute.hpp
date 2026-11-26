// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Permutable pins of a device class (lvs-comparison-algorithm "Class
// identity and pin labels", lvs-device-reduction "Permutable pins in
// matching"): the declared pairs of a class cell resolved to port positions.
// Shared by the comparison (pin labels), reduction (parallel signatures) and
// property matching (pin-associated properties). Added in M9 R2.
//
// Owner: LVSREAD (lvs/net); written by LVSALG/LVSRED in M9 R2.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/lvs/net/netlist_db.hpp"

namespace hefesto::lvs {

/// The declared pairs of `klass` as port positions, in declaration order.
/// A pair naming a pin the class does not have stops the permutations of
/// the class: `missing` receives that pin's name and `pairs` is empty
/// (Q-LVS-9: the missing pin never leaves an unset position).
struct PermutePairs {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
  std::optional<std::string> missing;
};

[[nodiscard]] PermutePairs permute_pairs(const Cell& klass);

/// The port position of the pin named `pin` of `klass` (case per the
/// class), or nullopt.
[[nodiscard]] std::optional<std::uint32_t> port_position(const Cell& klass, std::string_view pin);

/// One group number per port of `klass`: ports chained by permutable pairs
/// share the smallest position of the chain, every other port keeps its
/// own position. Independent of the order the pairs were declared in.
[[nodiscard]] std::vector<std::uint32_t> permute_groups(const Cell& klass);

}  // namespace hefesto::lvs
