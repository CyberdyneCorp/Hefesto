// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The auxiliary scripting tools of Magic's Tcl library (openspec/changes/
// m4-scripting/design.md D11; capability scripting-auxiliary-tools):
//
//   - the socket command server (socketcmd.tcl) is absent: no command opens
//     a listener and the sandbox denies `socket` (hefesto-security);
//   - the BSI menu helpers (bsitools.tcl), the revision-0 gencell toolkit
//     (toolkit_rev0.tcl) and the technology-builder prototype
//     (techbuilder.tcl) are not loaded: none of their commands exists
//     (not_loaded_commands()); rev0_cell_name() documents the legacy naming
//     so that cells it produced open as ordinary cells with properties;
//   - the reference-library stripper (strip_reflibs.tcl) is strip_reflibs(),
//     exposed by the CLI;
//   - console integration and the launchers are front-end behaviour: the
//     prompt characters are console_prompt(), `batch_mode` is the headless
//     CLI's (launcher_batch_mode());
//   - mazeroute is registered with TOOLS-MAZE-001 until M10 (Q-SCRIPT-30).
//
// Owner: TOOLS (design D2).

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::tools {

/// "Reference-library stripper": `text` without its lines beginning with
/// `string GDS_START` or `string GDS_END`; every kept line ends with `\n`
/// (Tcl's `gets`/`puts`). (Magic ref: tcltk/strip_reflibs.tcl:21-41)
[[nodiscard]] std::string strip_reflib_lines(std::string_view text);

/// Runs the stripper on every `*.mag` file directly in `directory` (in name
/// order), rewriting each in place (write_atomic, the `tmp.mag` + rename of
/// the script). A file that cannot be read or written gives TOOLS-STRIP-001
/// (`Error: can't open file <name>`) and the next file is processed.
/// Returns the diagnostics; the stripped file names are appended to
/// `stripped` when not null.
std::vector<diag::Diagnostic> strip_reflibs(platform::FileSystem& files, const platform::Path& directory,
                                            std::vector<std::string>* stripped = nullptr);

/// The commands of the auxiliary scripts that are installed but not loaded
/// (socketcmd.tcl, bsitools.tcl, toolkit_rev0.tcl, techbuilder.tcl): none
/// of them is registered.
[[nodiscard]] std::span<const std::string_view> not_loaded_commands() noexcept;

/// "Legacy gencell toolkit revision 0": `<type>_<n>` with the first n >= 1
/// for which no name in `existing` is taken. Informative (the toolkit is
/// not loaded). (Magic ref: tcltk/toolkit_rev0.tcl)
[[nodiscard]] std::string rev0_cell_name(std::string_view type, std::span<const std::string> existing);

/// "Console integration": the console prompt character (`%` normally, `]`
/// while DRC runs, `:` during long-command entry).
[[nodiscard]] char console_prompt(bool drc_busy, bool long_command) noexcept;

/// "Interpreter launchers": the value of the global `batch_mode` of a
/// session started with these options (`true` only for `-noconsole -dnull`,
/// the headless CLI).
[[nodiscard]] bool launcher_batch_mode(bool noconsole, bool dnull) noexcept;

}  // namespace hefesto::tools
