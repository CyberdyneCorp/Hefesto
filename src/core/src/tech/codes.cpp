// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "tech" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix TECH-), then regenerate
// docs/diagnostics/codes/tech.md with `hefesto-cli --diagnostic-codes=tech`.
//
// Severity convention: `error` for a problem that makes the technology load
// (or the command) fail, `warning` for one that is reported while the load
// goes on, `info` for informational output.

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kTechCodes = {
    CodeInfo{"TECH-CMD-001", error, "Bad `tech` option: ambiguous, unknown, or a wrong number of arguments."},
    CodeInfo{"TECH-CMD-002", error, "`tech load` usage error (`Usage: tech load <filename> [-noprompt] [-override]`)."},
    CodeInfo{"TECH-CMD-003", error,
             "`tech load` would invalidate painted layout and cannot ask for confirmation; use `-noprompt`."},
    CodeInfo{"TECH-CMD-004", error, "`tech lambda N [D]` is refused with magic-compat off; use `scalegrid`."},
    CodeInfo{"TECH-CMD-005", error, "Retired in M3, never reused. Was: `tech drc` needs the DRC rule set, which is not available before M3 (drc-*)."},
    CodeInfo{"TECH-CMD-006", error, "A layer name in a `tech` command does not resolve (`No such layer <n>`)."},
    CodeInfo{"TECH-FILE-001", error, "No technology file `X`, `X.tech` or `X.tech27` was found on the search path."},
    CodeInfo{"TECH-FILE-002", error,
             "The technology file cannot be read or does not begin with the `tech` section (readability pre-check)."},
    CodeInfo{"TECH-FILE-003", error, "A partial section reload was requested with no technology file loaded."},
    CodeInfo{"TECH-GRID-001", error, "`scalegrid` usage error (a and b must be strictly positive integers)."},
    CodeInfo{"TECH-GRID-002", error, "The grid scaling is finer than the limit set by the CIF output style."},
    CodeInfo{"TECH-GRID-003", info, "The internal grid scale after `scalegrid` (`<I> Magic internal units = <L> Lambda`)."},
    CodeInfo{"TECH-INCLUDE-001", error, "An `include` file was not found; the include line is discarded and the load fails."},
    CodeInfo{"TECH-INCLUDE-002", warning,
             "An `include` file was found only without the technology suffix (magic-compat off)."},
    CodeInfo{"TECH-LINE-001", error, "A section line was rejected; the section is flagged and the load fails."},
    CodeInfo{"TECH-LINE-002", warning, "A section line has a problem that does not fail the load."},
    CodeInfo{"TECH-LINE-003", info, "Informational message from a section handler."},
    CodeInfo{"TECH-LOAD-001", error, "The technology file failed to load; the previous technology stays in effect."},
    CodeInfo{"TECH-LOAD-002", info, "Technology values were scaled to match the internal grid (`Scaled tech values by I / L`)."},
    CodeInfo{"TECH-LOAD-003", warning, "After a load, the internal grid is finer than the process minimum."},
    CodeInfo{"TECH-LOAD-004", warning,
             "Existing layout may be invalid after a technology load (and the number of planes changed)."},
    CodeInfo{"TECH-NAME-001", error, "The `tech` section has no technology name line; the load fails (Q-TECH-8)."},
    CodeInfo{"TECH-READ-001", warning, "A logical line longer than 1023 characters was truncated."},
    CodeInfo{"TECH-REPEAT-001", warning, "`repeat` with an invalid count; the block is read once."},
    CodeInfo{"TECH-REPEAT-002", error, "A `repeat` block crosses an include file boundary; the load fails."},
    CodeInfo{"TECH-SECTION-001", warning,
             "Bad section header line or unrecognized section name; lines are skipped to the next `end`."},
    CodeInfo{"TECH-SECTION-002", warning, "A stray `end` outside any section was ignored (magic-compat off)."},
    CodeInfo{"TECH-SECTION-003", warning, "A section appears before its prerequisite sections and is skipped."},
    CodeInfo{"TECH-SECTION-004", error, "A required section is missing from the technology file; the load fails."},
    CodeInfo{"TECH-SECTION-005", warning, "A section appears more than once; its init runs again."},
    CodeInfo{"TECH-SECTION-006", error,
             "A `planes`, `types`, `contact`, `compose` or `connect` section is repeated (magic-compat off)."},
    CodeInfo{"TECH-SECTION-007", error, "Summary of the sections that contained rejected lines."},
};

}  // namespace

std::span<const CodeInfo> tech() noexcept {
  return kTechCodes;
}

}  // namespace hefesto::diag::area_codes
