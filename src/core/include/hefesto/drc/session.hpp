// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::DrcSession: the per-session DRC state, editor::EditorState::drc
// (openspec/changes/m3-drc/design.md D4). The struct is shared (owned by
// INTEGRATION); each member is owned by the workstream named beside it,
// which may change the member's own type in its own header but asks
// INTEGRATION (open item) to add or remove members here.
//
// Threading: every member is touched by the command thread only, except
// `rules` (a shared_ptr the worker copies under the shared database lock
// when a square starts), `worker` (thread-safe by itself) and `events`
// (published from the command thread).

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/drc/commands/reporting.hpp"
#include "hefesto/drc/engine/background.hpp"
#include "hefesto/drc/engine/pending.hpp"
#include "hefesto/drc/events.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/drc/types.hpp"

namespace hefesto::drc {

struct DrcSession {
  DrcSession();
  ~DrcSession();
  DrcSession(const DrcSession&) = delete;
  DrcSession& operator=(const DrcSession&) = delete;

  // ---- RULES --------------------------------------------------------------
  /// The compiled current style; never null (an empty "default" style until
  /// a technology with a drc section is loaded). Replaced, never mutated, on
  /// `drc style`, `tech load`, grid rescale and cifoutput style change.
  std::shared_ptr<const RuleSet> rules;
  /// The declared style names of the current technology.
  DrcStyleList styles{{"default"}};
  /// The current style's name as selected (DRCCurStyle->ds_name).
  std::string style = "default";
  /// The cifoutput styles DRC reads (contact borders, step size, cifstyle binding).
  CifCatalog cif;

  // ---- ENGINE -------------------------------------------------------------
  /// DRCBackGround: off, on, or not set (during a command). Starts `on`
  /// as in Magic; the CLI never runs the worker in batch mode (design D8).
  BackgroundMode background = BackgroundMode::on;
  PendingList pending;
  /// The worker (design D8); started by the hosts that run one.
  std::unique_ptr<BackgroundChecker> worker;
  /// `drc *halo d`: a reduced halo (never above the style halo); nullopt = the style halo.
  std::optional<std::int32_t> halo_override;
  /// `drc *stepsize d`: the step size in effect; nullopt = the style's.
  std::optional<std::int32_t> step_size;
  /// Counters for `drc statistics` (since they were last printed).
  CheckStatistics statistics;
  /// busy/idle, committed squares, style changes.
  DrcEvents events;

  // ---- COMMANDS -----------------------------------------------------------
  /// `drc euclidean on|off` (DRCEuclidean); Manhattan by default.
  bool euclidean = false;
  /// `drc ignore` (DRCIgnoreRules).
  IgnoreList ignore;
  /// `drc find`'s persistent counter (drc_nth); its initial value follows
  /// magic-compat (Q-DRC-15: 1 ON, 0 OFF), applied on first use.
  std::optional<std::int64_t> find_counter;
  /// `drc statistics`' totals (Magic's drcTotal* counters): the sum of the
  /// counters printed so far in this session.
  CheckStatistics statistics_total;
  /// The last Tcl `after` id DRC used. DRCContinuous evaluates `after idle
  /// magic::drcstate busy` and `... idle` when it has work, so `drc catchup`
  /// returns the second id (`after#N`) as its Tcl result; Magic's startup
  /// takes `after#0`. The M4 binding replaces this with Tcl's own ids.
  std::uint64_t tcl_after_id = 0;

  // ---- INTEGRATION --------------------------------------------------------
  /// The project's compatibility settings, set by cmd::Dispatcher::
  /// set_editor (not owned); nullptr = defaults (magic-compat ON).
  const compat::CompatSettings* compat = nullptr;

  /// The compat settings in effect.
  [[nodiscard]] compat::CompatSettings compat_settings() const { return compat != nullptr ? *compat : compat::CompatSettings{}; }
};

}  // namespace hefesto::drc
