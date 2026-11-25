// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): run_lvs compares nothing; the result is not
// clean and carries CORE-NOTIMPL-009.
//
// Owner: LVSHIER.

#include "hefesto/lvs/hier/run.hpp"

#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

LvsResult run_lvs(Circuit /*first*/, Circuit /*second*/, const SetupState& /*setup*/, const RunOptions& options,
                  ProgressSink& /*progress*/) {
  LvsResult result;
  result.netgen_compat = options.netgen_compat;
  result.diagnostics.push_back(not_implemented("lvs::run_lvs"));
  return result;
}

}  // namespace hefesto::lvs
