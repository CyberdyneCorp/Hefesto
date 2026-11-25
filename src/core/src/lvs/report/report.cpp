// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): the three writers fail with CORE-NOTIMPL-009.
//
// Owner: LVSREP.

#include "hefesto/lvs/report/report.hpp"

#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

diag::Outcome<std::string> write_comp_out(const LvsResult& /*result*/) {
  return not_implemented("lvs::write_comp_out");
}

diag::Outcome<std::string> write_json(const LvsResult& /*result*/) {
  return not_implemented("lvs::write_json");
}

diag::Outcome<cmd::Value> to_list(const LvsResult& /*result*/) {
  return not_implemented("lvs::to_list");
}

}  // namespace hefesto::lvs
