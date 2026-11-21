// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// NetlistSession: the session-sticky netlister state of one open project,
// editor::EditorState::netlist (openspec/changes/m7-extraction/design.md
// D5). The options change under the exclusive lock their commands hold.
//
// Owner: INTEGRATION owns the struct; each member belongs to the named
// workstream.

#pragma once

#include "hefesto/extflat/names.hpp"
#include "hefesto/netlist/sim/sim.hpp"
#include "hefesto/netlist/spice/settings.hpp"

namespace hefesto::netlist {

struct NetlistSession {
  // SPICE: the `ext2spice` options.
  spice::SpiceSettings spice;
  // SIM: the `ext2sim` options.
  sim::SimSettings sim;
  // EXTFLAT: the name-conversion flags, one session-wide set with
  // magic-compat ON, reset per run with it OFF (Q-NETLIST-16).
  extflat::NameFlags names;
};

}  // namespace hefesto::netlist
