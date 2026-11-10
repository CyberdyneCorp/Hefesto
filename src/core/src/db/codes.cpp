// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "db" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix DB-), then regenerate
// docs/diagnostics/codes/db.md with `hefesto-cli --diagnostic-codes=db`.
// Constants: hefesto/db/diag_codes.hpp.

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kDbCodes = {
    CodeInfo{"DB-CELL-001", error, "A cell definition with that name already exists."},
    CodeInfo{"DB-CELL-002", error, "The cell definition is still instantiated in another cell and cannot be deleted."},
    CodeInfo{"DB-CELL-003", error, "Internal cells cannot be renamed or deleted."},
    CodeInfo{"DB-CELL-004", error, "The cell is a vendor (GDS_FILE) cell; renaming it needs -force."},
    CodeInfo{"DB-CELL-005", error, "No cell definition with that name exists."},
    CodeInfo{"DB-CMD-001", error, "The command needs an edit cell and there is none."},
    CodeInfo{"DB-CMD-002", error, "The command needs the box and it is not set."},
    CodeInfo{"DB-CMD-003", error, "Unknown or ambiguous layer name."},
    CodeInfo{"DB-CMD-004", error, "Invalid command argument value."},
    CodeInfo{"DB-CMD-005", error, "No label matches the request."},
    CodeInfo{"DB-CMD-006", error, "The box is not in a window on the edit cell."},
    CodeInfo{"DB-CMD-007", error, "The new box lies outside the usable plane (box out of bounds)."},
    CodeInfo{"DB-CMD-008", error, "The command is not available in this build or on this platform."},
    CodeInfo{"DB-CMD-009", error, "A cell the command would change is not editable or does not match the edit cell."},
    CodeInfo{"DB-GEOM-001", error, "Geometry lies outside the usable plane (+-1073741818) and was rejected."},
    CodeInfo{"DB-LABEL-001", error, "A label needs non-empty text."},
    CodeInfo{"DB-PROP-001", error, "A property value cannot be parsed as the requested type."},
    CodeInfo{"DB-REPORT-001", info, "A report or note a command prints while it runs (Magic's TxPrintf text)."},
    CodeInfo{"DB-REPORT-002", warning, "A warning a command prints while it still runs (Magic's TxError text)."},
    CodeInfo{"DB-USE-001", error, "Placing the cell would make it contain itself."},
    CodeInfo{"DB-USE-002", error, "The instance identifier is already used in the parent."},
    CodeInfo{"DB-USE-003", error, "No instance with that identifier exists in the cell."},
    CodeInfo{"DB-USE-004", error, "The instance identifier is not valid."},
};

}  // namespace

std::span<const CodeInfo> db() noexcept {
  return kDbCodes;
}

}  // namespace hefesto::diag::area_codes
