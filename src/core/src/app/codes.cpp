// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "app" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix APP-), then regenerate
// docs/diagnostics/codes/app.md with `hefesto-cli --diagnostic-codes=app`.

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kAppCodes = {
    CodeInfo{"APP-DISPLAY-001", error, "Unknown display type given with -d; startup fails with status 1."},
    CodeInfo{"APP-FILE-001", info, "A file named on the command line is being loaded (`Loading \"F\" from command line.`; in wrapper mode `Handling file entry F extension E`)."},
    CodeInfo{"APP-FILE-002", warning,
             "A command-line file of this type (LEF, DEF, CIF, GDS, Tcl script) cannot be read in this release; it was skipped."},
    CodeInfo{"APP-LIBPATH-001", info,
             "Standard-cell libraries of an open_pdks PDK were added to the library path (`Cell path is now \"PATH\"`)."},
    CodeInfo{"APP-OPTION-001", error, "Unknown command-line option (`Unknown option: '-X'`); startup fails with status 1."},
    CodeInfo{"APP-OPTION-002", error, "An option argument starts with '-' (`Bad name after '-X' option`); startup fails with status 1."},
    CodeInfo{"APP-OPTION-003", error, "Ambiguous `-n` option (use -nowindow, -noconsole or -norcfile); startup fails with status 1."},
    CodeInfo{"APP-OPTION-004", error, "An option that needs an argument is the last word; startup fails with status 1."},
    CodeInfo{"APP-PIN-001", warning,
             "The PDK version pinned by the project is not installed; the `minimum` technology is used until it is installed."},
    CodeInfo{"APP-PROJECT-001", error, "The hefesto.project file is malformed."},
    CodeInfo{"APP-PROJECT-002", error, "The hefesto.project file cannot be read or written."},
    CodeInfo{"APP-QUIT-001", warning,
             "On iPadOS `quit` closed the project instead of ending the app (D-36): the script that called it stopped and the project browser is shown."},
    CodeInfo{"APP-RECENT-001", warning, "The recent-projects list is malformed or unreadable; it is treated as empty."},
    CodeInfo{"APP-RECOVER-001", warning, "Crash recovery (-r) is not available in this release; command-line files were not loaded."},
    CodeInfo{"APP-SETTINGS-001", warning, "The user preferences file is malformed; the user settings layer is left empty."},
    CodeInfo{"APP-STARTUP-001", info,
             "A startup file was found; its script commands run only once the Tcl binding exists (M4), its plain settings were applied."},
    CodeInfo{"APP-STARTUP-002", info, "A deprecated startup file name (`~/.magic`, `.magic`) is in use."},
    CodeInfo{"APP-TECH-001", warning, "The technology named at startup could not be loaded; the previous choice is kept."},
    CodeInfo{"APP-TECH-002", info, "The technology chosen at startup (`Using technology \"NAME\"`)."},
};

}  // namespace

std::span<const CodeInfo> app() noexcept {
  return kAppCodes;
}

}  // namespace hefesto::diag::area_codes
