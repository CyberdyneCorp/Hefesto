# Security model of scripting

This page describes how Hefesto contains the Tcl scripts it runs: PDK startup files, PCell generators, menu callbacks, console input and user scripts. It covers the sandbox, consent and trust, the data-only files, PDK widget interception and `readspice`. It also records the results of the M4 security review (openspec change `m4-scripting`, design D19, task 13.3) and explains how to add a command without opening a hole.

Read it before you change anything under `src/core/src/script/`, before you add an engine command that takes a path or reaches the host, and before you add a new way to run Tcl.

Normative sources:
- `openspec/specs/hefesto-scripting/spec.md`, "Embedded sandboxed Tcl on all platforms"
- `openspec/specs/hefesto-security/spec.md`, "Script sandbox enforcement" and "Consent before implicit script execution"
- decision X-3 in `docs/decisions/cross-cutting.md`
- design D5, D7 and D19 of `openspec/changes/m4-scripting/design.md`

## Threat model

Some PDK files come from the internet: a downloaded PDK, or a project folder that carries its own `.magicrc` or `<tech>.magicrc`. Their scripts are untrusted until the user says otherwise. The sandbox must hold even after the user lets a script run. "Run once" means "let it set up my PDK", not "let it own my machine".

Goals:
1. A script never runs a process, opens a socket or loads native code. No other origin runs a process either: `shell` is refused for typed lines too (S-01, user decision 2026-11-11).
2. A script reads and writes only under the granted roots: the project folder, the PDK store, the session's temporary area, and documents the user opened explicitly.
3. A script never changes the trust decisions of the user.
4. A shell cannot bypass the sandbox. It is enforced in the core, in `script::create_project_interpreter`, and no API returns an unsandboxed interpreter.

Out of scope: a hostile user at the console of their own machine; denial of service by a script the user chose to run (see "Accepted limits").

## The sandbox

Each open project has one full Tcl 8.6 interpreter (`EditorState::script.interp`). Commands are removed from it or restricted, instead of using a Tcl safe interpreter, because PDK scripts need `source`, `file`, `glob`, `open` and `info script` with their usual error texts (design D5). The code lives in `src/core/src/script/tcl/`, the only directory that includes `tcl.h`.

### Layers

| Layer | Where | What it does |
|---|---|---|
| Embedded library | `tcl_interpreter.cpp`, `embedded_library.cpp` | `init.tcl` and Tcl's script library are served from memory under `/__hefesto_embedded__/tcl8.6`. `auto_path`, the module path and the encoding search path point only there. No host Tcl file is read, even with `TCL_LIBRARY` or `TCLLIBPATH` set. |
| Removed commands | `sandbox_commands.cpp` | `exec`, `socket`, `load`, `unload` and `exit` are deleted and replaced by denial commands (SCRIPT-SANDBOX-001, 002, 003, 007). `load` stays a command, so Magic's `rename load tcl_load` still works. |
| Restricted commands | `sandbox_commands.cpp` | Guards placed with `Tcl_SetCommandInfo`, so compiled, renamed and ensemble calls reach them too: <ul><li>`open |…` is refused (001).</li><li>`interp create` needs `-safe`; `invokehidden`, `expose` and `marktrusted` are refused, also on a safe child's own command (005).</li><li>`package require` loads only the packages of the embedded library (006).</li><li>`file tempfile` needs a template directory under a root (004).</li><li>`file link` checks the link target (004).</li></ul> |
| Filesystem layer | `tcl_filesystem.cpp` | A `Tcl_Filesystem` registered with `Tcl_FSRegister` in front of the native one. Every Tcl path operation asks it first: `open`, `source`, `file *`, `glob`, `cd`, encoding files and `load`. Each guarded command starts a new filesystem epoch, so no cached claim is reused (S-05), and makes its own project's session current (S-06). It claims the embedded tree (read-only) and every path outside the granted roots (EACCES, recorded as a denial). Paths under a root go to the native filesystem. Directories above a root expose `stat` and `access` only. |
| Path decision | `sandbox.cpp` | `SandboxFilesystem::check`: the path is made absolute, `.`/`..` are removed and every symbolic link on it is resolved, dangling ones from the link's directory (S-04). The result must lie under a granted root (or, for reads, a read-only root), compared component by component. A `~` path is refused. |
| Environment | `synthesize_environment` | `env` holds only `PDK_ROOT`, `PDK` and `CAD_ROOT` (D-31). The traces that mirror it into the process are dropped, so writes stay local to the interpreter. |
| Limits | `tcl_interpreter.cpp` | Per top-level evaluation: an optional command count and time limit (`SandboxLimits`). `request_interrupt` uses `Tcl_CancelEval` with `TCL_CANCEL_UNWIND`, so `catch` cannot absorb it. The script cannot change its own limits ("limits on current interpreter inaccessible"). |
| Reporting | `SandboxSession::refuse` | Every denial produces a SCRIPT-SANDBOX-* diagnostic with the script file and line (`info frame`) and a `DeniedOperation` event. This happens even when `catch` absorbs the Tcl error, as older PDK `query_*` procedures expect. |

The thread's current session (`ActiveSession`) tells the filesystem layer which roots apply, because Tcl's filesystem hooks receive no interpreter. With no current session, everything outside the embedded library is denied (fail-closed).

### Granted roots

`app::Startup::sandbox_policy` (`src/core/src/app/startup.cpp`) builds the policy. Design D5 lists the roots:
- granted (read and write): the project folder, the session temporary area, explicitly opened documents (each startup file and each `.tcl` file argument is granted as itself) and the folder of each layout file argument;
- read-only (`SandboxPolicy::read_only_roots`, S-03): the PDK store and the install root (`CAD_ROOT`);
- the working directory only when the front end asks for it (`StartupEnvironment::grant_working_directory`: hefesto-cli yes, the desktop app no), and never when it, or a layout file's folder, is `/`, the home folder or a folder above it (S-11).

### Engine commands inside Tcl

`script::bind_registry` makes every engine command a Tcl command (`::magic::<name>`, imported globally). These commands do their I/O in C++ through `platform::FileSystem` and `platform::ProcessRunner`, not through Tcl, so the Tcl filesystem layer does not see them. Two rules cover them instead:
- **Origin.** A command run from Tcl gets the origin `script` when the evaluation is a script's or a script file is on the Tcl stack (a sourced file, a procedure a file defined; `script::detail::command_origin`), else the evaluation's own origin (a line the user typed: `cli` for hefesto-cli `-c` and standard input, `gui` for the console and command bar). The shell runs a PDK's menu items and device entries as `script`. Commands that download or change trust (the `pdk` subcommands that download or change the store) refuse `script`, `key` and `plugin` origins (`cmd::refuse_host_access`, S-01, S-03). `shell` is refused for every origin, typed console, command-bar and hefesto-cli lines included (`cmd::refuse_process_execution`, SCRIPT-SANDBOX-001; user decision 2026-11-11): Hefesto runs no external process (X-3).
- **Files.** The dispatcher gives commands of those origins `script::SandboxedFileSystem` instead of the session's file system (`Dispatcher::set_script_files`, installed by `bind_registry`, S-02): every path is checked as the Tcl layer checks Tcl's own, writes under a read-only root are refused, and only regular files are read.

## Consent and trust

`script::request_consent` (`src/core/src/script/pdk/trust.cpp`) decides whether a PDK script may run. A script asks for consent in two cases:
- it is a PDK's startup file (explicit startup, or `-rcfile` naming a PDK rc);
- it is the implicit `<tech>.magicrc` that technology auto-discovery finds.

Flow:
1. If the PDK version is trusted (`TrustStore`: trusted for this session, or recorded as trusted in the store's provenance record `<store>/ciel/<family>/.hefesto/<version>.json`) and the script is one of that version's files in the store (`TrustBackend::covers`: under `<store>/ciel/<family>/versions/<version>/` after links and `..` are resolved, S-09), the script runs.
2. If the version was declined earlier in this session, it is declined again without asking.
3. Otherwise the `ConsentHost` is asked:
   - Desktop: a dialog with Don't run, Run once and Trust this version. Return means Run once and Esc means Don't run. A script outside the PDK store gets no Trust button, and Don't run is its default.
   - CLI batch: `BatchConsentHost` runs only PDKs named by `--trust-pdk` (a family, a variant, `variant@version-prefix` or `family/variant@version`), and declines the rest.
   - No host: decline.
4. If declined, the layout opens read-only with the `minimum` technology (`tech::load_builtin_technology` replaces another loaded technology) and SCRIPT-TRUST-001 is reported. Other cells of the same variant follow without asking again, also when read with the technology ignored (`load -force`, subcell expansion, `flush`). The session records them (`cell::DeclinedCells`, per definition and file) and nothing writes them, not even after `cellname writeable` or `save <other name>` (SCRIPT-TRUST-005): they hold only what `minimum` could read. "Review scripts..." asks again; a yes runs the script, loads the PDK's technology and reads the cells again, editable (`cell::reopen_declined`; SCRIPT-TRUST-006 and the decline kept when the technology cannot be loaded).
5. Trust this version records `trusted: true` in the provenance record. `pdk trust` and `pdk untrust` change it from the GUI or a typed line; a script cannot run them (SCRIPT-SANDBOX-008), and the store is a read-only root for scripts, so no script can write the record either (S-03).

Two rules matter for the code. A trust decision must stay out of reach of the scripts it governs (findings S-03 and S-09). A script that runs through "Run once" has exactly the same sandbox as a trusted one.

## Files read as data

- **Toolbar presets** (`<tech>_toolbars.tcl`, `src/core/src/gui/toolbar_presets.cpp`). Magic sources this file; Hefesto parses it.
  - Only `set <name> {<layers>}` or `set <name> "<layers>"` lines are read. Lines with nesting, `$`, `[ ]` or backslashes are skipped and reported with a diagnostic.
  - Saving rewrites only the `set current_toolbar {…}` line.
  - Layer names reach commands as words: `palette_command` and `command_line` quote each word with `script::quote_list_element`, so a name such as `[exec ls]` stays one literal word. The test "quoted list elements round-trip adversarial words" covers this.
- **Key-binding files** (`key_bindings_sheet`, CARRY): Load file applies only `macro` and `imacro` lines.

## PDK widget interception

PDK menu callbacks expect Tk. `script::WidgetInterceptor` (`src/core/src/script/pdk/widgets.cpp`) defines Tcl procedures for the wrapper's frame paths and the common Tk commands:
- `…drc.toolmenu add command` and `…file.toolmenu insert` edit the `MenuModel`;
- `tk_getOpenFile` asks the shell's `platform::FilePicker`;
- every other widget statement is skipped with SCRIPT-PDK-004;
- `::unknown` forwards only names that start with `.`.

Nothing here widens the sandbox:
- menu commands run later, in the same project interpreter, under the same policy;
- labels are shown as text;
- a picked file is returned as a string.

The picker result does not become a granted root. A PDK callback that hands it to an engine command (`readspice`) reads it through the sandboxed file system, so a file outside the roots is refused even when the user picked it (an accepted limit: the user opens it with a typed `readspice`). The wrapper frames (`magic::openwrapper`, and the desktop's own window through `gui::adopt_wrapper`) install one interceptor each.

## readspice

`readspice` (`src/core/src/tools/readspice.cpp`) reads one netlist through `platform::FileSystem::read` and parses it line by line (`spice_lines.cpp`).
- It follows no includes and evaluates nothing.
- From a script, its path is checked against the roots and only a regular file is read (S-02).
- It reads the whole file into memory; a typed `readspice` of a huge file is the user's choice.

## Accepted limits

These are accepted by design D5 ("As landed in round 1") and were re-checked in this review:
- `file normalize` and `pwd` reveal how outside paths resolve. `::tcl::clock::getenv` reads `TZ`. `info nameofexecutable` and `file volumes` reveal host facts.
- Checking a path and then using it is not atomic: a symbolic link can be swapped in between.
- With a non-UTF-8 host locale, Tcl's one-time startup may read an encoding file through `TCL_LIBRARY` before the layer is registered.
- A `request_interrupt` that arrives just as an evaluation ends can cancel the next one.
- `cd` changes the process-wide working directory: with two projects open, one project's `cd` affects the other.
- Added by this review:
  - With no time limit set (app startup sets none by default), a script the user chose to run can loop until it is interrupted. hefesto-cli has `--script-time-limit <seconds>` (S-14); the desktop interrupts from the console.
  - `--trust-pdk <family|variant>` also runs a PDK found outside the store (S-12; accepted by the user on 2026-11-11 as a documented limit): batch runs name the PDK on purpose, and discovery only searches places the user's own environment names (`PDK_ROOT`, `PDKPATH`, `-rcfile`, ...); the golden cases rely on it for their copied PDK trees.
  - The cell file host (`cell/host.cpp`: permissions, `chmod` of a loaded cell's file, symlink tests) and the PDK manager's own stream I/O (`pdk/manager.cpp`, `packs.cpp`, `updates.cpp`) do not go through `platform::FileSystem`. A script reaches them only through cells it could load and through `pdk list`, `info`, `root`, which read the store (S-02, remainder).
  - The command limit does not count loops that compile to bytecode only (`while 1 {}`); the time limit and the interrupt do stop them.
  - Memory is not limited: a huge allocation makes Tcl panic, and the process aborts.

## Review results (M4, 2026-11-11)

The review was run on macOS arm64 (`build/m4r2-security`, macos-debug), with attack scripts through `hefesto-cli` and through the unit-test interpreter.

### Denied: kept as regression tests

The tests live in `tests/unit/script/tcl/sandbox_test.cpp` (round 1) and `tests/unit/script/tcl/security_review_test.cpp` and `tests/unit/script/pdk/security_review_test.cpp` (this review).

| Class | Attacks |
|---|---|
| Process | <ul><li>`exec`, `magic::exec`</li><li>`open |cmd`, `open [list |ls]`</li><li>A leading blank is a plain file name (nothing runs).</li></ul> |
| Network | `socket` |
| Native code | <ul><li>`load` and `unload`; `tcl_load` after Magic's rename</li><li>`package require` of a binary or unknown package, with `ifneeded`, `-exact`, abbreviated `req`, a custom `package unknown`, or `::tcl::tm::path add`</li><li>`Tk`</li></ul> |
| Interpreters | <ul><li>`interp create` without `-safe`, and `-- -safe`</li><li>Hidden commands through `invokehidden`, `expose` and `marktrusted`, abbreviated or with `-global`; through an alias to a hidden target, an ensemble mapping, a renamed child or a renamed `interp`; a nested safe child; hiding the project's own `open`</li></ul> |
| Limits | `interp limit {} …` from the script |
| Paths | <ul><li>Absolute, relative and `..`; `~`, `~user`; `//` and doubled slashes</li><li>`/dev/*`, `/dev/fd/0`, `/proc`; a trailing `/.`; an embedded NUL</li><li>`glob` with `-tails`, braces or `~`; `source` of a directory or with `-encoding`</li><li>`cd` outside the roots</li><li>Symbolic links that already point outside</li><li>`file link` to an outside or `..` target; hard links</li><li>`file copy` and `file rename` with either end outside; `file delete`, `file mkdir`, `file attributes` and `file tempfile` outside</li></ul> |
| Hooks | `trace add execution` on a guarded command; renamed `open` and `file` subcommands; `after` and `vwait` within one project |
| Environment | the process environment is not visible, and `env` writes do not reach it |
| Data | adversarial words through `quote_list_element` |
| Consent | <ul><li>A version not recorded in the store is not trusted.</li><li>A family or version unknown to the catalog is not trusted.</li></ul> |

### Findings and fixes

The review found 15 weaknesses. Each fix has a regression test named "security fix S-NN" in `tests/unit/script/tcl/security_review_test.cpp` or `tests/unit/script/pdk/security_review_test.cpp`, or the test named in the table (M4 round 2 integration, 2026-11-11).

| ID | Severity | Finding | Disposition |
|---|---|---|---|
| S-01 | Critical | The engine command `shell` ran `/bin/sh -c` for any script. | Fixed. `shell` and the `pdk` subcommands that download refuse `script`, `key` and `plugin` origins with SCRIPT-SANDBOX-001 (`cmd/host_access.hpp`); commands run from Tcl are `script` whenever a script file is on the stack (`binding/dispatch.cpp`); hefesto-cli `-c` and standard input are `cli`, script files `script`; the desktop runs PDK menu items and device entries as `script`. User decision 2026-11-11: `shell` is refused for every origin, typed console, command-bar and hefesto-cli lines included (`cmd::refuse_process_execution`; test "security fix S-01: shell is refused for a typed line too"). |
| S-11 | Critical | The working directory and the install root were granted read-write. | Fixed. Install root and PDK store are read-only roots; the working directory only for hefesto-cli and never `/`, home or above; layout file folders under the same rule (tests in `tests/unit/app/startup_run_test.cpp`). |
| S-03 | High | Any script could mark a PDK version trusted. | Fixed. `SandboxPolicy::read_only_roots` (Tcl writes refused by guards on `open`, `file delete|mkdir|rename|copy|link|attributes|mtime|atime|tempfile`; engine writes by `SandboxedFileSystem`); `pdk trust|untrust|uninstall|enable|pin|remove-library|export` refuse scripts (SCRIPT-SANDBOX-008). The record stays in the store (it is out of every script's write reach). |
| S-02 | High | Engine commands with path arguments bypassed the filesystem layer. | Fixed for `platform::FileSystem` I/O: `script::SandboxedFileSystem` for `script`/`key`/`plugin` commands, which also refuses non-regular files. Remainder accepted (see "Accepted limits"): the cell file host and the PDK manager's own streams. |
| S-04 | High | A relative link moved within a root wrote outside. | Fixed. `resolve()` follows every link, dangling ones from the link's directory (`sandbox.cpp`). |
| S-05 | High | Tcl's cached path claim skipped the check after a link was swapped in. | Fixed. Every guarded command starts a new filesystem epoch (`Tcl_FSMountsChanged`). |
| S-06 | High | Event callbacks ran with another project's roots. | Fixed. Every guarded command makes its own interpreter's session current (`ActiveSession` in `guarded_call`), whichever project services the event queue. |
| S-07 | Medium | `interp recursionlimit` above the C stack crashed the app. | Fixed. Limits above 1000 refused (SCRIPT-SANDBOX-005), also through a child's command. |
| S-09 | Medium | Trust followed a path pattern, not a location. | Fixed. `TrustBackend::covers`; `TrustStore::trusted(pdk, script)`; the desktop's `in_pdk_store` compares canonical paths. |
| S-08 | Medium | `encoding system|dirs` changed process-wide state. | Fixed. Refused with an argument (SCRIPT-SANDBOX-007). |
| S-12 | Medium | `--trust-pdk <variant>` matched copies outside the store. | Accepted limit, confirmed by the user on 2026-11-11: see "Accepted limits". |
| S-10 | Low | `file mkdir` failed under a root named through a link. | Fixed. Roots are kept as written too for the ancestor test. |
| S-13 | Low | The fuzz audit missed link-following escapes and engine I/O. | Fixed for links: the audit also checks each allowed path with its links followed and that nothing appears next to the root; the review's attacks seed the corpus. Engine commands are not bound in the fuzz target (covered by the unit tests above). |
| S-14 | Low | No time limit; the CLI could not be interrupted. | Fixed for the CLI: `--script-time-limit <seconds>` (default none, as in Magic). |
| S-15 | Low | The toolbar presets path was built from the technology name. | Fixed. A technology name with `/` reads no presets file (`gui/toolbar_presets.cpp`). |

Not a security issue, noted for BINDING: in `hefesto-cli` the `clock` command is missing (`invalid command name "clock"`, while `::tcl::clock::*` exists). The unit-test interpreter has it.

## Adding a command safely

Before you register a new engine command, Tcl command or `magic::` procedure:

1. **Paths.** Do all I/O through `ctx.services().files`, never with `std::fstream`, `fopen` or `std::filesystem` writes. Script invocations get the sandboxed file system automatically (S-02). Do not add a way around it, such as a cached `LocalFileSystem` or a path given to another service.
2. **Processes and network.** Do not run a process from any command: Hefesto runs no external program from any origin (X-3; `shell` answers `cmd::refuse_process_execution`). Do not call a fetcher from a command a script can reach; if you must, refuse with `cmd::refuse_host_access` (S-01) and add a denial test.
3. **Trust and settings that grant power.** Commands that change trust, the PDK store root, granted roots or limits refuse with `cmd::refuse_host_access(..., HostAccess::trust)`.
4. **Building Tcl text.** Never concatenate user data into Tcl source. Pass words as a `std::vector<std::string>`, or quote each one with `script::quote_list_element`. Data files (toolbar presets, key bindings) are parsed, never sourced.
5. **New Tcl commands in `script/tcl`.**
   - Anything that touches the host or process-wide Tcl state (`encoding`, `interp recursionlimit`, `cd`, channels to devices) needs a guard in `install_sandbox_commands`.
   - The guard reports through `SandboxSession::refuse` with a SCRIPT-SANDBOX-* code.
   - The code is documented in `docs/diagnostics/codes/script.md`.
6. **Event-loop callbacks.** A script that runs later (`after`, file events, menu callbacks) must run with its own project's session current (S-06).
7. **Tests.** For every denial, add a `[cap:hefesto-security][req:Script sandbox enforcement]` test that runs the attack as a script and checks:
   - the SCRIPT-SANDBOX-* code;
   - that the host file system is unchanged.

   If the change touches the filesystem layer, also run `fuzz_tcl_sandbox` (linux-fuzz) for 60 s.
8. **Review.** A change under `src/core/src/script/tcl/`, `script/pdk/trust.*`, `script/pdk/consent.*` or `app/startup.cpp` `sandbox_policy` needs a reviewer to check it against this page. Update this page when the model changes.
