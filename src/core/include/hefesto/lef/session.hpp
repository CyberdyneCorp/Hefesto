// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LefSession: the M6 LEF/DEF state of one open project,
// editor::EditorState::lefdef (openspec/changes/m6-lef-def-oasis/design.md
// D4). It changes only on tech load, `scalegrid` and finer-grid reads, and
// on `lef read`, `def read` and `def write`, all under the exclusive
// database lock, so it needs no copy-on-write object.
//
// Owner: INTEGRATION owns the struct; each member belongs to the named
// workstream.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/write/number.hpp"

namespace hefesto::lef {

struct LefSession {
  // LEFTECH: the LEF layer table of the tech `lef` section.
  LayerTable layers;
  // LEFVIA / LEFR / DEFW: the VIA definitions and NONDEFAULTRULEs, which
  // persist across reads and writes as Magic's globals do (Q-LEFDEF-99 ON).
  ViaTable vias;
  RuleTable rules;
  // LEFR: `lef datestamp` / `def datestamp` (lefCmd.c:446-482); nullopt =
  // `default` (the cells' own timestamps).
  std::optional<std::int64_t> datestamp;
  // LEFW: the `lef nocheck` net names.
  std::vector<std::string> nocheck;
  // LEFW: the session-sticky LEF database units of `lef write`
  // (Q-LEFDEF-34 ON).
  LefUnits units;
};

}  // namespace hefesto::lef
