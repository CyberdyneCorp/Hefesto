// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Application startup shared by hefesto-cli and the desktop app
// (capability app-startup: "Startup sequence", "Command-line options",
// "Command-line file arguments", "Installation root and search paths",
// "Startup file order", "Default technology selection", "Display type
// selection", "Batch and headless operation", "Crash recovery at startup",
// "Platform startup surface"; hefesto-settings "Magic startup-file
// compatibility"; scripting-pdk-integration "Startup files are scripts":
// startup files and a `.tcl` file argument are evaluated in the project's
// sandboxed Tcl interpreter, m4-scripting design D7).
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/app/project_file.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/tcl/sandbox.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::cmd {
class Dispatcher;
}

namespace hefesto::app {

/// Type of a command-line file argument ("Command-line file arguments").
enum class FileKind : std::uint8_t { cell, lef, def, cif, gds, script };

[[nodiscard]] std::string_view file_kind_name(FileKind kind) noexcept;

struct FileArgument {
  std::string name;  // `.mag` stripped for cells
  FileKind kind = FileKind::cell;

  friend bool operator==(const FileArgument&, const FileArgument&) = default;
};

/// Classifies one non-option argument: a trailing `.mag` is stripped, then
/// the last `.` extension (case-insensitive, exact per Q-UI-25) selects
/// LEF, DEF, CIF, GDS or a Tcl script; anything else is a layout cell.
[[nodiscard]] FileArgument classify_file_argument(std::string_view word);

/// Magic-compatible command-line options ("Command-line options").
struct StartupOptions {
  std::optional<std::string> technology;     // -T
  std::optional<std::string> display_type;   // -d (e.g. NULL, OGL)
  std::string monitor_type = "std";          // -m
  std::optional<std::string> graphics_port;  // -g
  std::optional<std::string> tablet_port;    // -i (-g sets it too unless -i was given)
  std::optional<std::string> rc_file;        // -rc / -rcfile: user startup file name instead of .magicrc
  bool recover = false;                      // -r, -re...
  bool no_window = false;                    // -nowindow / -w
  bool no_console = false;                   // -noconsole
  bool no_rc = false;                        // -norcfile
  bool debug = false;                        // -D
  std::vector<FileArgument> files;           // in command-line order (kept with -nowindow, Q-UI-25)
  std::vector<std::string> script_args;      // words after a `.tcl` argument
  /// Hefesto: session-layer settings from the front end (e.g. --setting).
  std::vector<std::pair<std::string, std::string>> session_settings;

  [[nodiscard]] std::string rc_name() const { return rc_file.value_or(".magicrc"); }
};

/// Parses argv words after the program name. Errors use the exact Magic
/// texts ("Unknown option: '-x'", "Bad name after '-T' option: '-x'",
/// "-T requires a following technology", "Ambiguous option -n: ...") with
/// APP-OPTION-00x codes; the process then exits with status 1.
[[nodiscard]] diag::Outcome<StartupOptions> parse_startup_options(std::span<const std::string_view> args);

/// Magic's usage line, printed after "Unknown option".
[[nodiscard]] std::string_view startup_usage_line() noexcept;

/// "Display type selection": headless (NULL...) or the default GUI backend.
struct DisplayChoice {
  std::string name;  // the matched compiled-in name, e.g. "NULL", "OGL", "XWIND"
  bool headless = false;
};

/// Resolves -d (upper-cased, leading whitespace skipped, first compiled-in
/// name that the value starts with). Without -d: XWIND when a display is
/// available, NULL otherwise. Unknown: APP-DISPLAY-001 with Magic's text.
[[nodiscard]] diag::Outcome<DisplayChoice> resolve_display(const std::optional<std::string>& display_type,
                                                           bool display_available);

/// "Startup file order": a startup file that exists, in processing order.
struct StartupFile {
  platform::Path path;
  std::string role;  // "pre", "system", "pdk", "user", "local"
  bool deprecated_name = false;  // ~/.magic or ./.magic
};

/// Inputs from the host ("Installation root and search paths", "Platform startup surface").
struct StartupEnvironment {
  platform::Path install_root;     // CAD_ROOT (empty: no system files); see resolve_install_root
  platform::Path home;
  platform::Path working_directory;
  /// Whether the working directory becomes a granted root of the project's
  /// script sandbox (the command-line front end; security finding S-11). It
  /// never does when it is "/", the home folder or a folder above it. The
  /// desktop app does not set it: it grants the project folder only.
  bool grant_working_directory = false;
  /// Whether the front end can show windows: the display chosen without -d
  /// (resolve_display). hefesto-cli has none (NULL graphics); the desktop
  /// app has one, so its session behaves as Magic's with a running display.
  bool display_available = true;
  /// The session's temporary area, a granted root of the script sandbox
  /// (design D5); empty: none.
  platform::Path temporary_folder;
  /// The evaluation limits of the project's interpreter (security finding
  /// S-14; hefesto-cli --script-time-limit). Default: none.
  script::SandboxLimits script_limits;
  /// Environment variables (PDK_ROOT, PDKPATH, path expansion) and symbolic
  /// links; nullptr = none (iPadOS, tests without a host). Must outlive the
  /// session: the editor's path-variable lookup keeps using it.
  pdk::InstallHost* host = nullptr;
  std::optional<platform::Path> project_folder;     // folder with hefesto.project
  std::optional<platform::Path> user_preferences;   // preferences JSON (user layer)
  std::optional<platform::Path> pdk_root;           // explicit PDK root; else settings, PDK_ROOT, ~/.ciel
  std::optional<platform::Path> pdk_startup_file;   // the PDK's .magicrc (role "pdk"), if known
  /// The recent-projects list (app/recent_projects.hpp); when set, the opened
  /// project folder is recorded in it and the session setting
  /// `recent-projects-file` names it for the `pdk` command.
  std::optional<platform::Path> recent_projects;
  /// Called once the project's interpreter is created and bound, before any
  /// startup file runs: the front end attaches its console (e.g. hefesto-cli's
  /// `puts` to its output streams). Not called when the interpreter existed.
  std::function<void(script::Interpreter&)> configure_interpreter;
  /// Wrapper mode only (-d names a display other than NULL): called once the
  /// startup layout window is open, before the command-line files load, as
  /// Magic's magic.tcl.in calls `magic::openwrapper` there. The front end
  /// wraps the window (gui::adopt_wrapper); hefesto-cli also installs its
  /// headless widget model (gui::install_tk_model).
  std::function<void()> open_wrapper;
};

/// "Installation root and search paths": the environment variable
/// CAD_ROOT when set and non-empty, else the built-in install prefix. (The
/// scripting variable CAD_ROOT, step 1, arrives with the Tcl binding, M4.)
[[nodiscard]] platform::Path resolve_install_root(const std::optional<std::string>& cad_root_env,
                                                  const platform::Path& builtin_prefix);

/// The system library path `. $CAD_ROOT/magic/sys $CAD_ROOT/magic/sys/current`
/// (unexpanded entries).
[[nodiscard]] std::vector<std::string> system_library_path();
/// The cell library path: `$CAD_ROOT/magic/<tech>` first when a technology
/// is known, then `$CAD_ROOT/magic/sys/current $CAD_ROOT/magic/tutorial`.
[[nodiscard]] std::vector<std::string> cell_library_path(const std::optional<std::string>& technology);

/// The technology named by a `.mag` file's header (`magic` then
/// `tech NAME`, Magic's DBGetTech); std::nullopt when the text has none.
[[nodiscard]] std::optional<std::string> mag_technology(std::string_view text);

[[nodiscard]] std::vector<StartupFile> startup_files(platform::FileSystem& fs, const StartupOptions& options,
                                                     const StartupEnvironment& env);

/// What startup did, for the front end.
struct StartupResult {
  std::vector<StartupFile> startup_files;
  std::optional<ProjectFile> project;
  std::string technology;                       // name of the technology in force
  std::optional<platform::Path> technology_file;
  std::vector<std::string> loaded_files;        // file arguments processed, in order
};

/// Runs steps 6–11 of "Startup sequence" on the dispatcher's editor
/// session: built-in technology, settings layers (defaults, PDK, user,
/// project, session; the effective compat settings are copied into
/// `project_settings`), the project's interpreter (granted roots: the startup
/// files, the `.tcl` file argument, working directory, project, PDK store and
/// CAD_ROOT), startup files (evaluated as Tcl, a PDK's after consent),
/// technology selection (-T, project pin, first file argument, `minimum`),
/// crash recovery (-r) or the file arguments (through the `load` command),
/// and the box. Fails only without an editor session (CMD-SESSION-001);
/// every other problem is reported to `sink` and startup continues.
[[nodiscard]] diag::Outcome<StartupResult> run_startup(cmd::Dispatcher& dispatcher,
                                                       settings::ProjectSettings& project_settings,
                                                       platform::FileSystem& fs, const StartupOptions& options,
                                                       const StartupEnvironment& env, diag::DiagnosticSink& sink);

}  // namespace hefesto::app
