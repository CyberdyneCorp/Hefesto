// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The first-word rule of ext2spice, ext2sim and antennacheck
// (netlist/first_word.hpp; Q-NETLIST-13).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:375-380,
// ext2sim/ext2sim.c:316-320 and extflat/EFantenna.c:157-161 (the lookup of
// the first word), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION.

#include "hefesto/netlist/first_word.hpp"

#include "hefesto/cmd/keywords.hpp"

namespace hefesto::netlist {

namespace {

FirstWordResult run_with(std::span<const std::string> args) {
  return FirstWordResult{FirstWord::run, 0, std::vector<std::string>(args.begin(), args.end())};
}

// magic-compat OFF: the table with `run` as the keyword of entry 0.
FirstWordResult classify_corrected(std::span<const std::string> args, std::span<const std::string_view> table) {
  std::vector<std::string_view> corrected(table.begin(), table.end());
  corrected.front() = "run";
  const cmd::LookupResult found = cmd::lookup(args.front(), corrected);
  if (found.match == cmd::Match::ambiguous) {
    return FirstWordResult{FirstWord::ambiguous, 0, {}};
  }
  if (found.match == cmd::Match::unrecognized) {
    return run_with(args);
  }
  if (found.index == 0) {
    return run_with(args.subspan(1));
  }
  return FirstWordResult{FirstWord::option, found.index, {}};
}

}  // namespace

FirstWordResult classify_first_word(std::span<const std::string> args, std::span<const std::string_view> table,
                                    bool magic_compat) {
  if (args.empty() || table.empty()) {
    return run_with(args);
  }
  if (!magic_compat) {
    return classify_corrected(args, table);
  }
  const cmd::LookupResult found = cmd::lookup(args.front(), table);
  if (found.match != cmd::Match::found) {
    return run_with(args);
  }
  if (found.index == 0) {
    // Only the literal `[run]` matches the help entry; Magic consumes it.
    return run_with(args.subspan(1));
  }
  return FirstWordResult{FirstWord::option, found.index, {}};
}

}  // namespace hefesto::netlist
