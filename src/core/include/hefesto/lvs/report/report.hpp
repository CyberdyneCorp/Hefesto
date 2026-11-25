// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LVS reports (openspec/changes/m9-lvs-simulation design.md D16, "Area
// contracts" LVSREP; capability lvs-reports): `comp.out` in the two-column
// layout, `comp.json` and the structured list result. Sections whose order
// would come from hash order are written in a deterministic order.
//
// Owner: LVSREP.

#pragma once

#include <string>

#include "hefesto/cmd/value.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/hier/run.hpp"

namespace hefesto::lvs {

[[nodiscard]] diag::Outcome<std::string> write_comp_out(const LvsResult& result);
[[nodiscard]] diag::Outcome<std::string> write_json(const LvsResult& result);
/// The `-list` result ("Structured list result").
[[nodiscard]] diag::Outcome<cmd::Value> to_list(const LvsResult& result);

}  // namespace hefesto::lvs
