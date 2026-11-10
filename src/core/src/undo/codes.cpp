// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "undo" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix UNDO-), then regenerate
// docs/diagnostics/codes/undo.md with `hefesto-cli --diagnostic-codes=undo`.
// Constants: hefesto/undo/diag_codes.hpp.
//
// Owner: UNDO (openspec/changes/m2-interactive-editing/design.md D1/D2).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

// Keep sorted by code. Never renumber or reuse a code once released.
using enum Severity;

constexpr std::array kCodes = {
    CodeInfo{"UNDO-CMD-001", error, "Invalid undo or redo arguments (usage, or a count that is not a positive integer)."},
    CodeInfo{"UNDO-CMD-002", warning, "Undo was attempted while undo recording is disabled; nothing was undone."},
    CodeInfo{"UNDO-LIMIT-001", error,
             "The undo history limit must be an integer of at least 1000; the previous limit is kept."},
    CodeInfo{"UNDO-REPORT-001", info, "A report the undo or redo command prints (Magic's TxPrintf text)."},
};

}  // namespace

std::span<const CodeInfo> undo() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
