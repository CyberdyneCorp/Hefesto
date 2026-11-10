// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The basic check kernel entry (drc-rule-semantics; openspec/changes/m3-drc
// design.md D4/D6): Magic's DRCBasicCheck over one flat cell (a real cell
// for the non-interaction part of a step square, or the engine's flattened
// yank cell for interaction areas).
//
// basic_check(input, K, L, sink) examines every tile of every paint plane
// (ascending plane order) overlapping the check rectangle K, evaluates the
// rules of each left and bottom edge segment in K ("Basic check scope"),
// reports subcell-error tiles (why 2, clipped to K), and reports every error
// rectangle to `sink` clipped to the clip rectangle L. The mask-layer
// (cif) rules are kept unchecked until M5 (design D13).
//
// The kernel reads only: the cell's planes and its `MASKHINTS_*` properties
// (hints::HintRegion, for exception/exemption scoping through
// hints::inside), the rule set and the cifoutput catalog (Euclidean derived
// borders). It never re-tiles or clips its input (tile structure decides
// area, maxwidth, exception and diagonal results). It allocates nothing in
// the edge loop beyond reused buffers and is safe to run on the background
// worker while the database lock is held shared.
//
// Every compat quirk of drc-rule-semantics (Q-DRC-7, 9, 10, 11/Q-MISC-13,
// 20) is one branch on `compat.magic_compat`; the replicated quirks
// (Q-DRC-6, 8, 19, 21) hold in both modes. The kernel adds to the sink's
// statistics (tiles, edges, rules, slow) when the sink has them. The
// implementation is split over src/core/src/drc/check/ (see kernel.hpp).
//
// (Magic ref: drc/DRCbasic.c DRCBasicCheck, drcTile, areaCheck;
// drc/DRCextend.c)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <atomic>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/drc/types.hpp"

namespace hefesto::db {
class CellDef;
class DatabaseLock;
}  // namespace hefesto::db

namespace hefesto::tech {
class Technology;
}

namespace hefesto::drc {

class CifCatalog;

/// When a running check must stop early (design D8): the worker's own
/// cancel flag, or a writer waiting for the database lock. Polled between
/// tiles; both pointers optional.
struct CancelToken {
  const std::atomic<bool>* flag = nullptr;
  const db::DatabaseLock* lock = nullptr;

  /// True when the check should stop now.
  [[nodiscard]] bool requested() const noexcept;
};

/// Everything the kernel reads. Pointers are not owned and must outlive the
/// call; `cell`, `rules` and `technology` are required.
struct CheckInput {
  const db::CellDef* cell = nullptr;           // planes and MASKHINTS_* hint regions
  const tech::Technology* technology = nullptr; // plane and type tables (home planes, contacts)
  const RuleSet* rules = nullptr;              // the compiled style
  const CifCatalog* cif = nullptr;             // contact borders for Euclidean derived edges; nullptr = none
  bool euclidean = false;                      // `drc euclidean on` (DRCEuclidean)
  compat::CompatSettings compat;               // magic-compat switches (Q-DRC-7/9/10/11/20)
  CancelToken cancel;                          // early stop (background worker only)
};

enum class CheckOutcome : unsigned char {
  completed,  // every tile was examined
  cancelled,  // stopped by `cancel`; the reports so far are partial and must be dropped
};

/// DRCBasicCheck(cell, K, L, ...): see the file comment. A null K, or an
/// input without rules, reports nothing.
CheckOutcome basic_check(const CheckInput& input, const db::Rect& check_area, const db::Rect& clip_area,
                         ErrorSink& sink);

}  // namespace hefesto::drc
