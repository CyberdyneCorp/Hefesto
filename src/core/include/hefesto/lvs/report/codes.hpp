// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-REPORT-* (src/core/src/lvs/report/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSREP").
//
// Owner: LVSREP.

#pragma once

#include <string_view>

namespace hefesto::lvs::report_codes {

inline constexpr std::string_view kBadWidth = "LVS-REPORT-001";

}  // namespace hefesto::lvs::report_codes
