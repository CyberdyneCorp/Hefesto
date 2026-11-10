// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::DrcEvents: the observer list of the DRC engine (openspec/changes/
// m3-drc/design.md D4/D8/D11). The engine publishes
//   - busy / idle: the background checker starts working / runs out of
//     work, at Magic's DRCContinuous transitions (`magic::drcstate busy`
//     and `idle` in Magic's Tcl build; M4 turns them into Tcl callbacks);
//   - a committed square: the cell and the area whose error tiles changed
//     (the shell redisplays it, the DRC browser marks itself stale);
//   - a style change: `drc style`, `tech load`, grid rescale.
// Consumers: the desktop shell (status chip, browser, redisplay), the CLI
// and, in M4, the Tcl binding.
//
// Delivery is synchronous on the thread that publishes. The engine
// publishes from the command thread only (commits happen between commands,
// design D8), so observers need no locking of their own. Observers are not
// owned and must be removed before they are destroyed.
//
// Owner: INTEGRATION (shared file); published by ENGINE (busy/idle,
// squares) and RULES/COMMANDS (style changes).

#pragma once

#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::drc {

enum class CheckerState : unsigned char { idle, busy };

class DrcObserver {
 public:
  DrcObserver() = default;
  DrcObserver(const DrcObserver&) = delete;
  DrcObserver& operator=(const DrcObserver&) = delete;
  virtual ~DrcObserver() = default;

  /// The background checker became busy or idle (magic::drcstate).
  virtual void on_state(CheckerState /*state*/) {}
  /// A checked square was committed: the error plane of `cell` changed
  /// under `area` (cell coordinates).
  virtual void on_square_committed(const db::CellDef& /*cell*/, const db::Rect& /*area*/) {}
  /// The current DRC style changed to `style`.
  virtual void on_style_changed(const std::string& /*style*/) {}
};

class DrcEvents {
 public:
  void add(DrcObserver& observer);
  void remove(const DrcObserver& observer);
  [[nodiscard]] std::size_t size() const noexcept { return observers_.size(); }

  /// Publishes a state; a repeat of the last published state is dropped,
  /// so observers see strict busy/idle alternation.
  void publish_state(CheckerState state);
  void publish_square(const db::CellDef& cell, const db::Rect& area);
  void publish_style(const std::string& style);

  /// The last published state (idle before any).
  [[nodiscard]] CheckerState state() const noexcept { return state_; }

 private:
  std::vector<DrcObserver*> observers_;
  CheckerState state_ = CheckerState::idle;
};

}  // namespace hefesto::drc
