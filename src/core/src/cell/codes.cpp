// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "cell" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix CELL-), then regenerate
// docs/diagnostics/codes/cell.md with `hefesto-cli --diagnostic-codes=cell`.
// Constants: hefesto/cell/diag_codes.hpp.

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kCellCodes = {
    CodeInfo{"CELL-BACKUP-001", error, "A crash backup or archive file cannot be read or is malformed."},
    CodeInfo{"CELL-BACKUP-002", info, "Crash backup or archive progress (file created, recovered or removed)."},
    CodeInfo{"CELL-BACKUP-003", warning, "A periodic crash backup failed, or periodic crash backups were halted."},
    CodeInfo{"CELL-CMD-001", error, "Invalid arguments to a cell file command."},
    CodeInfo{"CELL-CMD-002", error, "The cell named by a command does not exist or there is no edit cell."},
    CodeInfo{"CELL-EXT-001", info, "A HEFESTO_ property has a newer schema version; it is kept unchanged."},
    CodeInfo{"CELL-IO-001", info, "The cell file is not writeable or is locked by another session; the cell is read-only."},
    CodeInfo{"CELL-IO-002", error, "The cell is read-only or locked and cannot be written."},
    CodeInfo{"CELL-IO-003", error, "Writing the cell file failed; the previous file is unchanged."},
    CodeInfo{"CELL-IO-004", warning, "A saved parent references a cell renamed after a duplicate-cell conflict."},
    CodeInfo{"CELL-IO-005", info, "A cell is written while it still waits for a new timestamp."},
    CodeInfo{"CELL-IO-006", error, "A name, identifier or path contains characters that cannot be written back."},
    CodeInfo{"CELL-IO-007", info, "A cell was saved in a non-default form (command script)."},
    CodeInfo{"CELL-PATH-001", warning, "A cell was found somewhere other than its recorded path."},
    CodeInfo{"CELL-PATH-002", error, "No file could be found or read for the cell."},
    CodeInfo{"CELL-PATH-003", warning, "A use-line path conflicts with an already loaded cell of the same name."},
    CodeInfo{"CELL-PATH-004", warning, "A use-line path names an undefined variable; the search paths are used."},
    CodeInfo{"CELL-PATH-005", error, "The cell was already read from a different, conflicting location."},
    CodeInfo{"CELL-PATH-006", info, "Standard-cell library directories were added to the cell library path."},
    CodeInfo{"CELL-PATH-007", info, "Automatic technology discovery is loading the technology a cell file names."},
    CodeInfo{"CELL-PATH-008", error, "Automatic technology discovery could not load the technology a cell file names."},
    CodeInfo{"CELL-READ-001", error, "The cell file contained a format error; the content read so far is kept."},
    CodeInfo{"CELL-READ-002", error, "The first line of a cell file is not \"magic\"."},
    CodeInfo{"CELL-READ-003", error, "The \"tech\" line of a cell file is malformed."},
    CodeInfo{"CELL-READ-004", warning, "A header line (magscale, maxlabscale, timestamp) is malformed or deprecated."},
    CodeInfo{"CELL-READ-005", error, "The cell's technology differs from the current one."},
    CodeInfo{"CELL-READ-006", warning, "A malformed label, port, element or property record was skipped."},
    CodeInfo{"CELL-READ-007", warning, "A record value is unknown (layer, port use/class/shape) and was replaced or ignored."},
    CodeInfo{"CELL-READ-008", warning, "Geometry may be lost because the internal grid cannot be refined."},
    CodeInfo{"CELL-READ-009", info, "Informational message while reading a cell (identifiers assigned, scaling)."},
    CodeInfo{"CELL-READ-010", info, "A use block was adjusted or ignored (circular use, degenerate box)."},
    CodeInfo{"CELL-READ-011", warning, "The cell has a zero timestamp and should be written out."},
    CodeInfo{"CELL-READ-013", error, "A compressed cell file could not be decompressed."},
    CodeInfo{"CELL-READ-014", warning, "Two uses in a cell file share an instance identifier; one is renamed."},
    CodeInfo{"CELL-STAMP-001", info, "Timestamp mismatches were processed."},
};

}  // namespace

std::span<const CodeInfo> cell() noexcept {
  return kCellCodes;
}

}  // namespace hefesto::diag::area_codes
