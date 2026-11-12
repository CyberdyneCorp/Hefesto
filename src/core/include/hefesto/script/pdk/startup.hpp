// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The script-host startup (openspec/changes/m4-scripting/design.md D7;
// capability scripting-pdk-integration "Script-host startup sequence",
// "Command-line options in the script host", "Command-line file
// arguments", "Startup files are scripts"; Q-SCRIPT-33, Q-SCRIPT-37): the
// command-line table of Magic's tcltk/magic.tcl.in with its quirks, the
// handling of command-line file arguments, and the startup files evaluated
// as Tcl at global scope with Magic's error texts (after consent for a
// PDK's rc file). Replaces the literal-`set` startup reader of M1
// (app/settings_layers.cpp); app::run_startup calls run_startup_files.
//
// Ported from Magic 8.3.694 (fc5daa9e) tcltk/magic.tcl.in:158-237 and
// 363-439 and utils/main.c:840-1060. Copyright (C) 1985, 1990 Regents of
// the University of California, see THIRD_PARTY_NOTICES.md.
//
// Owner: PDKINT (design D2).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script {

class ConsentHost;

// ---- Startup files ("Startup files are scripts") ---------------------------

/// The role of a startup file, which selects Magic's error text.
enum class StartupKind : std::uint8_t {
  pre,     // $CAD_ROOT/magic/sys/.initrc
  system,  // $CAD_ROOT/magic/sys/.magicrc
  pdk,     // a project's pinned PDK rc file (Hefesto; consent applies)
  user,    // ~/<rcname> or ~/.magic
  local,   // ./<rcname>, ./.magic, ./magic_setup, or an absolute -rcfile
};

/// One startup file in the order Magic runs them.
struct StartupFile {
  platform::Path path;
  StartupKind kind = StartupKind::local;
  /// The name Magic prints for a user or local file (the rc name as given,
  /// `.magic`, `magic_setup`); empty: the path.
  std::string name;
  /// The deprecated `.magic` or `magic_setup` names (Magic's "local" texts).
  bool legacy_name = false;
  std::optional<PdkIdentity> pdk;  // set for a PDK's rc file (consent applies)
};

/// Magic's text for an error in `file` (utils/main.c:860-1056): one error
/// line, and for a local file Magic's "continuing without" line.
[[nodiscard]] std::vector<diag::Diagnostic> startup_error(const StartupFile& file, std::string_view message);

/// What the startup sequence runs.
struct StartupPlan {
  std::vector<StartupFile> files;
  /// Asked for a PDK's rc file of an untrusted version; nullptr: the
  /// session TrustStore's host (editor.script.pdk_trust), which declines
  /// when it has none.
  ConsentHost* consent = nullptr;
  /// Polled after each file: true stops the sequence (`quit` ran).
  std::function<bool()> stop_requested;
  /// Called after each file was evaluated (also when it failed), e.g. to
  /// move the variables it set into its setting layer.
  std::function<void(const StartupFile&)> after_file;
};

/// Runs `plan` in the project's interpreter (editor.script.interp): each
/// file is `source`d at global scope (so `info script` names it). An error
/// stops that file and is reported with Magic's text; the next file still
/// runs. A PDK rc file runs only after consent (SCRIPT-TRUST-* notes).
/// Fails (SCRIPT-PDK-008) only when the project has no interpreter; the
/// value is the list of the files that ran.
[[nodiscard]] cmd::CommandResult run_startup_files(editor::EditorState& editor, const StartupPlan& plan);

// ---- Command line ("Command-line options in the script host") --------------

/// One row of the script host's table, in Magic's matching order.
struct CommandLineOption {
  std::string pattern;  // the Tcl regular expression, e.g. "^-rc(file)?$"
  int arguments = 0;    // words the option consumes after itself
  std::string meaning;  // one line
};

/// The table, in Magic's matching order (Q-SCRIPT-37: replicated in both
/// magic-compat settings, including the spellings it accepts by accident).
[[nodiscard]] std::span<const CommandLineOption> command_line_table();

/// What the script host knows before the engine starts.
struct ScriptHostInfo {
  std::string version;  // "<version>.<revision>" (--version)
  std::string commit;   // --commit
  std::string cad_root; // --prefix
};

/// The classified command line (magic.tcl.in's argument loop).
struct ScriptHostCommandLine {
  bool wrapper = true;   // false after -now..., -dnull
  bool recover = false;  // -r, -re, -recover, -rcover, ...
  /// The arguments passed to engine initialization, in order (Magic's
  /// `argafter` without `magic::initialize`; `-nowindow` is not inserted).
  std::vector<std::string> engine_args;
  /// The file arguments (Magic's `celllist`), in order.
  std::vector<std::string> files;
  /// --version, --commit, --prefix: the line printed before exiting 0.
  std::optional<std::string> print_and_exit;
};

/// Classifies `args` (argv after the program name; the launcher has already
/// consumed `-noc...`) with the table, first match wins. An option missing
/// its following argument passes the empty string, as `lindex` does.
[[nodiscard]] ScriptHostCommandLine classify_command_line(std::span<const std::string> args,
                                                          const ScriptHostInfo& info);

// ---- Command-line file arguments -------------------------------------------

/// How one file argument is loaded ("Command-line file arguments").
struct FileEntry {
  /// The command to run (`lef read <f>`, `source <f>`, `magic::load <f>`,
  /// `load <f>`, ...); empty when the file cannot be loaded.
  std::string command;
  /// Console lines in order: SCRIPT-PDK-006 (stdout: `Handling file entry
  /// ...` in wrapper mode, `Loading "<f>" from command line.` in NULL
  /// graphics) and SCRIPT-PDK-007 (stderr, wrapper mode only) diagnostics.
  std::vector<diag::Diagnostic> messages;
};

/// Tcl's `file extension`: from the last `.` of the last path element, or
/// empty.
[[nodiscard]] std::string file_extension(std::string_view file);

/// The handling of `file` in wrapper mode (`wrapper` true,
/// magic.tcl.in:363-411) or in NULL graphics, where Magic's C startup loads
/// it (utils/main.c:383-440,1189-1225; `.mag` stripped, no `handling file
/// entry` line; CIF and GDS read and exact extensions as in app-startup,
/// Q-UI-25).
[[nodiscard]] FileEntry script_host_file_entry(std::string_view file, bool wrapper);

}  // namespace hefesto::script
