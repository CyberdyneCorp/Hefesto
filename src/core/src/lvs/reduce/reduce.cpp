// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): reduce changes nothing and reports
// CORE-NOTIMPL-009 in the summary.
//
// Owner: LVSRED.

#include "hefesto/lvs/reduce/reduce.hpp"

#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

ReductionSummary reduce(Cell& /*cell*/, const SetupState& /*setup*/) {
  ReductionSummary summary;
  summary.diagnostics.push_back(not_implemented("lvs::reduce"));
  return summary;
}

}  // namespace hefesto::lvs
