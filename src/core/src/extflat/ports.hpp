// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A cell's port entries in def_nodes iteration order (see ports.cpp).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13).

#pragma once

#include <vector>

#include "hefesto/extflat/read.hpp"
#include "model.hpp"

namespace hefesto::extflat::detail {

/// topVisit's walk of `def`'s node-name table: one entry per name whose
/// node is a port (EF_PORT), in Magic's hash-table order.
[[nodiscard]] std::vector<PortEntry> port_entries(const Def& def);

}  // namespace hefesto::extflat::detail
