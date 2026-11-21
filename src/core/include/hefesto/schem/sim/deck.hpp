// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic circuit source and simulator vector names, M8 part
// (openspec/changes/m8-schematic design.md D13, "Area contracts" SIMINT;
// capability schematic-simulation-integration "Schematic circuit source",
// "Control block translation", "Simulator vector names").
//
// Owner: SIMINT.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/schem/edit/hier_path.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

struct Session;

/// The deck of a `schematic <file>` source for simulation-netlist-preparation:
/// the testbench's `hier` netlist (not LVS) with its control blocks
/// translated, before that capability's validation.
struct Deck {
  std::string text;
  std::vector<diag::Diagnostic> diagnostics;  // the netlister's warnings
};

[[nodiscard]] diag::Outcome<Deck> build_deck(const Session& session, const Document& testbench);

/// The simulator node name of `net` seen at `path` ("Simulator vector
/// names"). Port nets resolve upwards through the session's documents.
[[nodiscard]] std::string vector_name(const Session& session, const HierPath& path, std::string_view net);

}  // namespace hefesto::schem
