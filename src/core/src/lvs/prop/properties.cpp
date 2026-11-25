// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): check_properties finds no error and reports
// CORE-NOTIMPL-009.
//
// Owner: LVSPROP.

#include "hefesto/lvs/prop/properties.hpp"

#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

PropertyResult check_properties(const Matching& /*matching*/, const SetupState& /*setup*/) {
  PropertyResult result;
  result.diagnostics.push_back(not_implemented("lvs::check_properties"));
  return result;
}

}  // namespace hefesto::lvs
