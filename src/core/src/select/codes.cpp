// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "select" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix SELECT-), then regenerate
// docs/diagnostics/codes/select.md with `hefesto-cli --diagnostic-codes=select`.
// Constants: hefesto/select/diag_codes.hpp.
//
// Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D1/D2).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kCodes = {
    CodeInfo{"SELECT-CMD-001", error, "Bad arguments for a selection command (the usage line is printed)."},
    CodeInfo{"SELECT-CMD-002", error, "The word is not a valid select option."},
    CodeInfo{"SELECT-CMD-003", error, "The layer type cannot be parsed."},
    CodeInfo{"SELECT-CMD-004", error, "The layer list given to point selection is invalid."},
    CodeInfo{"SELECT-CMD-005", error, "Unknown display style for select feedback."},
    CodeInfo{"SELECT-CONTEXT-001", error, "The command needs the box and it is not set."},
    CodeInfo{"SELECT-CONTEXT-002", error, "The command needs the pointer in a layout window."},
    CodeInfo{"SELECT-CONTEXT-003", error, "The pointer (or the box) is not in a window on the edit cell."},
    CodeInfo{"SELECT-CONTEXT-004", error, "The current cell is not editable."},
    CodeInfo{"SELECT-CONTEXT-005", error, "Nothing is selected."},
    CodeInfo{"SELECT-FIND-001", error, "No paint of the requested type is under the cursor or box."},
    CodeInfo{"SELECT-FIND-002", error, "No cell use has the given instance path."},
    CodeInfo{"SELECT-FIND-003", error, "No shorting path was found between the two labels."},
    CodeInfo{"SELECT-ID-001", error, "The new instance identifier contains an illegal character."},
    CodeInfo{"SELECT-ID-002", error, "The new instance identifier is not unique within its parent."},
    CodeInfo{"SELECT-ID-003", error, "No subcell is selected, so no identifier can change."},
    CodeInfo{"SELECT-ID-004", error, "The selected cell is not a child of the edit cell."},
    CodeInfo{"SELECT-XFORM-001", error, "The rotation angle must be 90, 180 or 270 degrees."},
};

}  // namespace

std::span<const CodeInfo> select() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
