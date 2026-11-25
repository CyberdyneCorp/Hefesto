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

#include <string>
#include <utility>
#include <vector>

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
  // EXTFLAT: EFSearchPath left over by a run that stopped before EFDone
  // (no cell named and no window), with magic-compat ON only; the next run
  // of either netlister uses it when it gives no `-p` (Q-NETLIST-61).
  std::vector<std::string> search_path;
};

/// The `.ext` search path of one run (Q-NETLIST-61, compat-neutral): the
/// run's own `-p`, else the left-over path, which is consumed. Magic's
/// EFDone frees EFSearchPath at the end of every run, so `-p` holds for
/// the run that names it in both tools.
inline std::vector<std::string> take_search_path(NetlistSession& session, const std::vector<std::string>& given) {
  std::vector<std::string> leftover = std::exchange(session.search_path, {});
  return given.empty() ? leftover : given;
}

}  // namespace hefesto::netlist
