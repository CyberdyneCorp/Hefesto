// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtractSession: the M7 extraction state of one open project,
// editor::EditorState::extract (openspec/changes/m7-extraction/design.md
// D5). The style changes only on tech load, `extract style`, `scalegrid`
// and finer-grid reads (the editor-side hook, editor/extract_hook.hpp);
// readers hold the shared_ptr they started with. The options change under
// the exclusive lock the `extract` command holds.
//
// Owner: INTEGRATION owns the struct; each member belongs to the named
// workstream.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/extract/tech/style.hpp"

namespace hefesto::extract {

/// ExtOptions and the warning set (capability extract-commands "Extraction
/// options", "Option defaults", "Warnings"); the defaults are Magic's
/// start-up values (ExtMain.c:56-70).
struct ExtractOptions {
  bool adjust = true;
  bool capacitance = true;
  bool coupling = true;
  bool lumped = true;  // `lumped`: lumped resistance (EXT_DORESISTANCE)
  bool length = true;
  bool unique = true;
  bool unique_notopports = false;
  bool fringehalo = true;  // reset from the tech format on every tech load
  bool labelcheck = true;
  bool aliases = true;
  bool corners = false;
  double corner_factor = 1.0;
  bool extresist = false;  // `extresist` / `resistance`: full R-C extraction
  bool warn_dup = true;
  bool warn_fets = true;
  bool warn_labels = false;
  /// `extract path` (ExtLocalPath); nullopt = none.
  std::optional<std::string> path;

  friend bool operator==(const ExtractOptions&, const ExtractOptions&) = default;
};

/// The `extract halo` and `extract stepsize` overrides of the loaded style
/// (Q-EXTRACT-25: scaled on grid rescale, lost on style reload).
struct Overrides {
  std::optional<std::int32_t> halo;
  std::optional<std::int32_t> step_size;

  friend bool operator==(const Overrides&, const Overrides&) = default;
};

/// `extract length driver|receiver` terminal lists.
struct LengthLists {
  std::vector<std::string> drivers;
  std::vector<std::string> receivers;

  friend bool operator==(const LengthLists&, const LengthLists&) = default;
};

struct ExtractSession {
  // EXTTECH: the style table of the loaded technology and the current style
  // (after micron conversion and grid rescale); nullptr = no style.
  StyleTable styles;
  std::shared_ptr<const ExtStyle> current;
  // EXTCMD: options, overrides and path-length lists.
  ExtractOptions options;
  Overrides overrides;
  LengthLists length;
};

}  // namespace hefesto::extract
