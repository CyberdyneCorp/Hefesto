// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFFlatBuild with EF_FLATNODES | EF_NOFLATSUBCKT and EFVisitNodes
// (capability ext-flattening-hierarchy "Container iteration order", "Flat
// node construction", "Node merge semantics", "Kills", "Global name
// merging", "Visit orders for output"; ext-flattening-names "Instance and
// array element names", "Global and ground detection"): efFlatNodesStdCell,
// efFlatKills and efFlatGlob, then the node list.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/extflat/read.hpp"
#include "state.hpp"

namespace hefesto::extflat::detail {

/// Flattens the nodes below `root` and returns them in EFVisitNodes order
/// (killed nodes left out). A cap or a use cycle sets `st.fatal`.
[[nodiscard]] std::vector<FlatNode> flatten_nodes(ReadState& st, std::uint32_t root);

}  // namespace hefesto::extflat::detail
