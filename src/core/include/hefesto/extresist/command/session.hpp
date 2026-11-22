// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResistSession: the `extresist` state of one open project,
// editor::EditorState::extresist (openspec/changes/m7-extraction/design.md
// D5, D16). The options change under the exclusive lock the command holds.
//
// Owner: INTEGRATION owns the struct; each member belongs to the named
// workstream.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hefesto/extresist/network/network.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::extresist {

/// The `extresist` options (capability extresist-command "Command
/// options"), with Magic's defaults.
struct ResistOptions {
  double threshold = 10000.0;  // `threshold`, milliohms (rthresh)
  double min_resist = 1000.0;  // `minresist`, milliohms
  double min_delay_ps = 1.0;   // `mindelay`, picoseconds (Q-NETLIST-28)
  bool all = false;
  bool simplify = true;
  bool extout = true;
  bool lumped = false;
  bool silent = false;
  bool debug = false;
  bool stats = false;
  bool blackbox = false;
  /// `skip` types removed from the net-copy connectivity (Q-NETLIST-23).
  tech::TypeMask skip;
  std::vector<std::string> force;
  std::vector<std::string> ignore;
  std::vector<std::string> include;
  /// `fasthenry [freq]`'s frequency; nullopt = never stored (Q-NETLIST-21).
  std::optional<double> fasthenry_frequency;
};

struct ResistSession {
  // RESCMD: the options.
  ResistOptions options;
  // RESNET: the path-resistance heap kept across nets with magic-compat
  // ON (Q-NETLIST-46).
  HeapState heap;
};

}  // namespace hefesto::extresist
