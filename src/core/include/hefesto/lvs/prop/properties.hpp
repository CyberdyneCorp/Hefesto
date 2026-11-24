// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Property checks of a matched pair (openspec/changes/m9-lvs-simulation
// design.md D8, "Area contracts" LVSPROP; capability lvs-property-matching).
// Values are binary64 parsed with from_chars, so the checks give the same
// result on every platform.
//
// Owner: LVSPROP.

#pragma once

#include <string>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace hefesto::lvs {

/// One entry of "Property error results".
struct PropertyError {
  std::string device1;
  std::string device2;
  std::string property;
  std::string detail;  // e.g. "property-check-failed" (Q-LVS-10)
};

struct PropertyResult {
  std::vector<PropertyError> errors;
  std::vector<diag::Diagnostic> diagnostics;
};

[[nodiscard]] PropertyResult check_properties(const Matching& matching, const SetupState& setup);

}  // namespace hefesto::lvs
