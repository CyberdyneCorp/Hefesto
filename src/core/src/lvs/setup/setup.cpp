// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): evaluate_setup fails with CORE-NOTIMPL-009.
//
// Owner: LVSSETUP.

#include "hefesto/lvs/setup/setup.hpp"

#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

diag::Outcome<SetupState> evaluate_setup(const Source& /*setup*/, SetupContext& /*context*/) {
  return not_implemented("lvs::evaluate_setup");
}

}  // namespace hefesto::lvs
