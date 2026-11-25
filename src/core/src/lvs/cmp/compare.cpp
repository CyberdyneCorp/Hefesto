// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 4.2): compare_pair answers `mismatch` with
// CORE-NOTIMPL-009 in the result.
//
// Owner: LVSALG.

#include "hefesto/lvs/cmp/compare.hpp"

#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

PairResult compare_pair(const Cell& first, const Cell& second, const SetupState& /*setup*/,
                        CompareOptions /*options*/) {
  PairResult result;
  result.cell1 = first.name;
  result.cell2 = second.name;
  result.diagnostics.push_back(not_implemented("lvs::compare_pair"));
  return result;
}

}  // namespace hefesto::lvs
