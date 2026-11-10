// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::DrcEvents (openspec/changes/m3-drc/design.md D4).
// Owner: INTEGRATION (shared).

#include "hefesto/drc/events.hpp"

#include <algorithm>

namespace hefesto::drc {

void DrcEvents::add(DrcObserver& observer) {
  if (std::find(observers_.begin(), observers_.end(), &observer) == observers_.end()) {
    observers_.push_back(&observer);
  }
}

void DrcEvents::remove(const DrcObserver& observer) {
  std::erase(observers_, &observer);
}

void DrcEvents::publish_state(CheckerState state) {
  if (state == state_) {
    return;
  }
  state_ = state;
  // Copy: an observer may remove itself while being notified.
  const std::vector<DrcObserver*> observers = observers_;
  for (DrcObserver* o : observers) {
    o->on_state(state);
  }
}

void DrcEvents::publish_square(const db::CellDef& cell, const db::Rect& area) {
  const std::vector<DrcObserver*> observers = observers_;
  for (DrcObserver* o : observers) {
    o->on_square_committed(cell, area);
  }
}

void DrcEvents::publish_style(const std::string& style) {
  const std::vector<DrcObserver*> observers = observers_;
  for (DrcObserver* o : observers) {
    o->on_style_changed(style);
  }
}

}  // namespace hefesto::drc
