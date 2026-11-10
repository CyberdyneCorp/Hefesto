// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::DrcSession (openspec/changes/m3-drc/design.md D4).
// Owner: INTEGRATION (shared).

#include "hefesto/drc/session.hpp"

namespace hefesto::drc {

DrcSession::DrcSession()
    : rules(std::make_shared<const RuleSet>()), worker(std::make_unique<BackgroundChecker>()) {}

DrcSession::~DrcSession() {
  if (worker != nullptr) {
    worker->stop();
  }
}

}  // namespace hefesto::drc
