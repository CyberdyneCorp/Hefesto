// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the undo area (prefix UNDO-). The table with
// severities and summaries is src/core/src/undo/codes.cpp, documented in
// docs/diagnostics/codes/undo.md (`hefesto-cli --diagnostic-codes=undo`).
//
// Owner: UNDO (openspec/changes/m2-interactive-editing/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::undo::codes {

/// Invalid `undo`/`redo` arguments (usage text, bad count).
inline constexpr std::string_view kBadUsage = "UNDO-CMD-001";
/// `undo` while recording is suspended (Magic refuses to replay then).
inline constexpr std::string_view kUndoDisabled = "UNDO-CMD-002";
/// An `undo-history-limit` value that is not an integer >= 1000.
inline constexpr std::string_view kHistoryLimit = "UNDO-LIMIT-001";
/// A report line (Magic's TxPrintf): "Nothing more to undo", listings.
inline constexpr std::string_view kReport = "UNDO-REPORT-001";

}  // namespace hefesto::undo::codes
