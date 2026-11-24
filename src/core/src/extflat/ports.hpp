// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Ports of a cell for the netlisters (see ports.cpp).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13).

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extflat/read.hpp"
#include "model.hpp"
#include "state.hpp"

namespace hefesto::extflat::detail {

/// topVisit's walk of `def`'s node-name table: one entry per name whose
/// node is a port (EF_PORT), in Magic's hash-table order.
[[nodiscard]] std::vector<PortEntry> port_entries(const Def& def);

/// esMakePorts for `def` (ext2spice/ext2hier.c:1995-2298): every net a
/// `merge`, or a coupling of at least the threshold, reaches in a child
/// becomes a port there, and EF_SUBS_PORT propagates to the parent net.
void make_ports(ReadState& st, std::uint32_t def, bool cap_infinite, double cap_threshold);

/// The port entries of `def` and its port flags.
[[nodiscard]] DefPorts def_ports(const Def& def);

/// esHierVisit's absorbed cell: every node loses EF_PORT and EF_SUBS_PORT.
void clear_ports(Def& def);

}  // namespace hefesto::extflat::detail
