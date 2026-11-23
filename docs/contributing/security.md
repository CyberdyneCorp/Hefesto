# Security model of scripting

This page describes how Hefesto contains the Tcl scripts it runs: PDK startup files, PCell generators, menu callbacks, console input and user scripts. It covers the sandbox, consent and trust, the data-only files, PDK widget interception and `readspice`. It also records the results of the M4 security review (openspec change `m4-scripting`, design D19, task 13.3), the M5 review of the GDS, CIF and mask readers ("Layout input"), and explains how to add a command without opening a hole.

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
  - `pdk install` and `pdk updates` typed by the user start the downloader and decompressor processes (`pdk/commands.cpp` through `ProcessRunner`). Accepted by the user on 2026-11-11 as the only exception to "no process execution": built-in commands only, only from a typed line; the same commands from a script, macro or plugin are refused (S-01, S-03).

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

Found later by `fuzz_tcl_sandbox` (CI run 43487286969): `catch body var` with a `var` that cannot be set read the freed interpreter result, a heap use-after-free in Tcl itself (8.6.16, also 8.6.18 and 9.x). It is fixed by the patch `third_party/patches/tcl/0001-objsetvar2-hold-value-across-lookup.patch`, which is carried until upstream ships a fix (see THIRD_PARTY_NOTICES.md). The test "catch into a variable that cannot be set does not use the freed result" is in `tests/unit/script/tcl/sandbox_test.cpp`.

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

## LEF, DEF, OASIS and `.ext` input (M6 review, 2026-11-20)

The M6 readers (`src/core/src/lef/`, `def/`, `oasis/`, `extflat/`) follow the same rule: every file is hostile input (design D23 of `openspec/changes/archive/2026-11-22-m6-lef-def-oasis/`, task 17.3). The review covered M6-S1..S9 of D23 by reading the readers and the writers that re-encode read data, and by replaying the five M6 fuzz corpora (below). Caps that change behaviour where Magic does not crash are proposals until the user decides them (D21); the approved ones cite their Q-row.

### Bounds in place

| ID | Concern | State | Where |
|---|---|---|---|
| M6-S2 | LEF `RECT ITERATE` counts (nx·ny) and LEF via `ROWCOL` (rows·cols) | Cap decided by the user on 2026-11-19 (D1, Q-LEFDEF-118): at most 65536 repeats; above it the statement is rejected with an error (`Bad RECT ITERATE: ...`, `Bad ROWCOL: ...`). Only magic-compat OFF reads these statements (Q-LEFDEF-7), so Magic's behaviour is unchanged. Regression tests: `tests/unit/lef/read/via_section_test.cpp` ("regression: RECT ITERATE and ROWCOL above 65536 repeats are rejected"). | `lef/read/via_body.hpp` `kMaxViaRepeat` |
| M6-S2 | LEF coordinates that overflow once the macro offset is added | Fixed in both modes: the coordinate is dropped. | `lef/read/` |
| M6-S2 | DEF `VIAS ... + ROWCOL` rows·columns | Fixed in both modes, cap decided by the user on 2026-11-19 (D1, Q-LEFDEF-117): a hostile `+ ROWCOL 100000 100000` made the shared generator build 10^10 cuts (Magic runs out of memory). A generated via above 1,000,000 cuts is refused with `Via "<via>": ROWCOL <r> <c> gives more than 1000000 cuts; the via is not generated.` (DEF-READ-007) and gets no generated geometry. Regression test: `tests/unit/def/read/via_rule_test.cpp`. | `def/read/vias.cpp` `kMaxGeneratedCuts` |
| M6-S2 | DEF values whose float-to-int conversion overflows | Fixed in both modes: treated as unparsed, paint clipped to the plane (Q-LEFDEF-130). | `def/read/` |
| M6-S2 | DEF write: a LEF VIA's cut array (`via_rects`) | Fixed in both modes, cap approved by the user on 2026-11-20 (E3, Q-LEFDEF-150): `def write` wrote nx·ny RECT lines for a via whose area holds tiny cut squares, so a hostile LEF VIA made the output billions of lines (Magic does the same). Above 1,000,000 cuts the record gets one cut the size of its area, with `Via "<via>": cut array of <nx> x <ny> gives more than 1000000 cuts; one cut the size of the via area is written.` (LEF-VIA-012, error). The same review found the cut arithmetic's float-to-int conversions undefined for a large `def write -units` (int overflow, undefined in Magic too): they saturate now, and the counts and positions are 64-bit, in both modes. Regression tests "security fix M6-S2: a cut array above 1,000,000 cuts is written as one cut", "a cut array of exactly 1,000,000 cuts is written in full" and "regression: a DEF scale beyond the int range saturates the cut arithmetic" (`tests/unit/lef/via/regions_test.cpp`). | `lef/via/rects.cpp` `kMaxCuts` |
| M6-S3 | OASIS read transcode: stream size and instance count | Approved cap, both modes (D1 of the user decisions of 2026-11-19, Q-OASIS-41): at most 2 GiB for the in-memory GDS-equivalent stream and 2^28 element instances and uses per read; past either the rest of the file is not read (OASIS-READ-007). Regression tests "input limits: stream and instance caps" (`tests/unit/oasis/read/limits_test.cpp`). | `oasis/read/limits.hpp` (`oasis::ReadLimits`) |
| M6-S3 | OASIS read transcode: names and strings | Approved cap, both modes (D1, 2026-11-19, Q-OASIS-42): a cell name, placed cell name, TEXT string or `S_GDS_PROPERTY` value over 65530 bytes is cut, with a message (OASIS-READ-006); the top cell is loaded under its cut name. Regression tests "input limits: names and strings". | `oasis/read/transcode*.cpp` |
| M6-S3 | OASIS read transcode: layer, datatype and attribute numbers | Approved cap, both modes (D1, 2026-11-19, Q-OASIS-43): a number above 32767 is read as 32767, with a message once per element (OASIS-READ-009); an `S_GDS_PROPERTY` attribute below -32768 is read as -32768 (approved 2026-11-20, Q-OASIS-45). Regression tests "input limits: numbers above 32767". | `oasis/read/transcode*.cpp` |
| M6-S3 | OASIS read transcode: polygon point count | Approved, both modes (D1, 2026-11-19, Q-OASIS-44): a polygon over 8191 points is fractured into polygons of at most 8191 points, by median cuts with O(n log n) work and at most 64 levels (OASIS-READ-008). An outline the cuts cannot shrink, or wider or taller than 2^32, is dropped (OASIS-READ-006, Q-OASIS-46), and a path over 8191 points is dropped (Q-OASIS-47); both approved as proposed on 2026-11-20 (E2). Regression tests "input limits: polygons over 8191 points". | `oasis/read/fracture.cpp` |
| M6-S3 | OASIS write of a read-only GDS cell: an AREF with inexact steps (type 10 explicit displacements) | Approved cap, both modes (E3 of 2026-11-20, Q-OASIS-53): above 2^24 instances the array is not written and the write fails (OASIS-WRITE-010); without the cap a 32767 x 32767 COLROW allocates about 17 GB. No Magic counterpart (Magic has no OASIS). Regression test "an inexact AREF above 2^24 instances fails the transcode (approved cap)" (`tests/unit/oasis/write/vendor_test.cpp`). | `oasis/write/translate.cpp` (`kMaxExplicitOffsets`) |
| M6-S3 | gds write of a read-only OASIS cell: element instances after expansion | Approved cap, both modes (E3 of 2026-11-20, Q-OASIS-54): above 2^28 element instances per cell the cell and its parents fail (GDS-WRITE-055), mirroring the oasis-read cap (Q-OASIS-41). Regression test "a vendor OASIS cell above 2^28 element instances fails the cell (approved cap)" (`tests/unit/oasis/write/vendor_test.cpp`). | `gds/write/vendor_oasis.cpp` |
| M6-S3 | OASIS decoder: point lists and explicit repetitions (types 4, 5, 6, 7, 10, 11) | Approved cap, both modes (G5 of 2026-11-21, Q-OASIS-14): a point list of more than 2^24 points, or a list repetition of more than 2^24 displacements, is read without being stored and its element is dropped with OASIS-CODEC-030; a later element reusing it as its modal value is dropped too, and decoding continues. Without the cap a CBLOCK of about 1 MB inflating to 1 GiB of one-byte deltas decoded to about 16 GiB of points. No Magic counterpart (Magic has no OASIS). Regression tests "security fix M6-S3: a point list above 2^24 points is read, not stored, and dropped (approved cap)" and the repetition and modal-reuse tests (`tests/unit/oasis/codec/primitives_test.cpp`, `records_test.cpp`). | `oasis/codec/lists.cpp` (`oasis::kMaxListEntries`) |
| M6-S4 | CBLOCK inflation | One CBLOCK inflates to at most 256 times its compressed size, and at least `DecoderOptions::inflate_limit` (1 GiB), OASIS-CODEC-023 (design D11); a gzip-wrapped file goes through the M5 gzip source and its cap (GDS-STREAM-006, M5-S1). A CBLOCK inside a CBLOCK and a record spanning a CBLOCK end are decoded as oasis-format rules them (Q-OASIS-9); fuzz seeds `hostile-nested-cblock-*` and `hostile-split-cblock-*`. | `oasis/codec/decoder.cpp`, `validate.cpp` |
| M6-S8 | `.ext` read and flatten | Approved caps, both modes (G5 of the user decisions of 2026-11-21): `extflat::ReadLimits`, a file at most 1 GiB, a logical line at most 16 MiB, at most 1 Mi cells, use nesting 1000 deep, 4 Mi flattened instances, 16 Mi flat names and 16 Mi merge elements (past one the read fails, EXTFLAT-READ-009; instances and merge elements are counted per traversal of the hierarchy, so a design with exactly 4 Mi instances flattens however many visits the netlister makes); a `use` cycle fails the read (EXTFLAT-READ-010); a line over 128 tokens is ignored with the warning `<file>, line <n>: more than 128 tokens; line ignored` (EXTFLAT-READ-011, shown on every console, never silently). Tests in `tests/unit/extflat/hardening_test.cpp` (including "a hierarchy with exactly max_instances instances flattens") and the regression tests "a line of more than 128 tokens is ignored with the EXTFLAT-READ-011 warning (magic-compat ON)" and "(magic-compat OFF)" (`tests/unit/extflat/read_test.cpp`); fuzz seeds `hostile-use-cycle-*`, `hostile-deep-chain-*`, `hostile-merge-chain-*`, `hostile-array-use-*`. | `extflat/read.hpp`, `extflat/reader.cpp` |

### Findings of this review

| ID | Concern | Finding | Disposition |
|---|---|---|---|
| M6-S1 | Fixed-size buffers | Magic's 256-byte names (Q-LEFDEF-113) and 2048-character `fgets` lines: Hefesto's lexer keeps the 2048-character read semantics (`lef/lex/lexer.cpp`) with growable strings, so nothing overruns; a quoted token that runs to the end of a line appends the next read, so one token can grow to the size of the file. | Accepted limit: memory linear in the input. No cap (a token cap would refuse names Magic reads). |
| M6-S2 | Declared counts | DEF section counts (`COMPONENTS <n>`, `NETS <n>`, ...) are only compared with the records read (the count warnings); nothing is reserved from a file value. LEF/DEF ROWCOL and RECT ITERATE are capped (above). | No finding. |
| M6-S2 | Coordinates × UNITS | LEF and DEF values are read as float and converted with range checks (Q-LEFDEF-130; the LEF macro-offset overflow fix above); DEF route coordinates are clamped to the plane (`def/read/routes.cpp`). | No finding. |
| M6-S2 | DEF write cut arrays | Unbounded output and undefined conversions (above). | Fixed (Q-LEFDEF-150 and the saturating conversions), regression tests above. |
| M6-S3 | OASIS integers, reals, repetitions, points | Integers past 64 bits fail with OASIS-CODEC-005; reals of type 2/3 with a zero divisor are dropped with a message (golden case OASIS-001); repetition dimensions are kept as counts and expanded lazily (`offset_at`), and the farthest offset is checked in 128-bit arithmetic (OASIS-CODEC-008); the read and the vendor writes cap instances (above). A decoded point list or explicit repetition was a vector bounded only by the inflated input: a CBLOCK that inflates to 1 GiB (about 1 MB of deflate data) of one-byte deltas decoded to about 16 GiB of 16-byte points. | Fixed (round 6): capped at 2^24 entries (G5 of 2026-11-21, Q-OASIS-14; above). |
| M6-S4 | CBLOCK bombs and nesting | Bounded (above). | No finding. |
| M6-S5 | Name tables | Table offsets are read but names are resolved from the records as they come (no offset chasing, so no cycle); a refnum defined twice and a reference to a refnum never named follow Q-OASIS-34. Fuzz seeds `hostile-dup-refnum-*`, `hostile-unnamed-refnum-*`. | No finding. |
| M6-S6 | Placement cycles and depth | OASIS: the read goes through gds-read (design D12), whose cycle refusal and 512-deep look-ahead bound apply (GDS-READ-011, M5); fuzz seeds `hostile-self-place-*`, `hostile-mutual-place-*`. DEF: a COMPONENT whose master is the design cell or one of its ancestors is refused by `db::place` (DB-USE-001), and DEF has one level of placement. | No finding. |
| M6-S7 | CIRCLE and non-Manhattan work | `circlepoints` is limited to 8..4096 by the `oasis` option, and `circle_vertices` drops a radius above its maximum before allocating; the transcoded polygons go through gds-read's M5-S7 decomposition budget. Fuzz seed `hostile-big-circle-*`. | No finding. |
| M6-S8 | `.ext` recursion and merge chains | Bounded by `extflat::ReadLimits` (above); a `use` cycle fails the read instead of recursing (Magic recurses without end). | Fixed (round 2, EXTSUB); the limits approved 2026-11-21 (G5), the 128-token rule with a shown warning (round 6). |
| M6-S9 | Paths | `OASIS_FILE` (read-only cells) and `use /abs/path` (`.ext`) are opened through the session's `platform::FileSystem` (the project-root sandbox on iPadOS), as the M5 `GDS_FILE` rule; the named file's content is copied into a write, as Magic does with `GDS_FILE`. LEF and DEF output names (Q-LEFDEF-43) are opened through the same file system; the `LocalFileSystem` in the readers and writers is only the fallback of a null `files` option (round 3 fix). `$PDK_ROOT` in `OASIS_FILE` is E4's OASIS-032 (the Tcl variable only). | Accepted limit (as M5 `GDS_FILE`). |

### Proposed caps (need a user decision)

- **`.ext` read limits (M6-S8).** Approved 2026-11-21 (G5) as implemented, in both modes, with one change made in round 6: a line over 128 tokens is ignored with a warning every console shows, never silently. No longer pending.

### Fuzz targets

`fuzz_lef_reader` (LEF, plain and gzip, with annotation and a second read), `fuzz_def_reader` (DEF after the fixed LEF library `tests/fuzz/data/lefdef_fuzz.lef`, plain and gzip, with `-labels`, `-noblockage` and annotation), `fuzz_lef_section` (the technology `lef` section, installed and rescaled), `fuzz_oasis_reader` (OASIS, plain and gzip: the decoder, END validation, the content hash, the full read, then an in-memory OASIS write and GDS write) and `fuzz_ext_reader` (`.ext` sets read and flattened) live in `tests/fuzz/` (design D21); their file comments list the mode bits and resource caps. The LEF and DEF targets use the fixed technology `tests/fuzz/data/lefdef_fuzz.tech`, the OASIS target the M5 `mask_fuzz.tech`. `tests/fuzz/gen_m6_seeds.py` regenerates their seeds from the LEFDEF and OASIS golden cases, the `.ext` test data, the 419-byte reference file and hostile files built with `oasgen.py` (`--check` reports a stale corpus; `--pdk` adds `pdk-*` excerpts of the pinned PDK LEF files). CI runs each for 60 s (`fuzz` job) and replays the seeds on every leg (`fuzz-seeds.*`).

When you change a LEF, DEF, OASIS or `.ext` reader, replay the seeds of these targets, and for a change to a bound above add a regression test named "security fix M6-..." or extend this table.

## Schematic input (M8, in progress)

The M8 readers (`src/core/src/schem/`) treat every `.sch`, `.sym` and `xschemrc` file as hostile input (design D19 of `openspec/changes/m8-schematic/`). The review itself comes later in M8; this section collects what round 1 put in place. Caps that change behaviour are proposals until the user decides them. **User decisions of 2026-11-21:** the input limits below marked "approved" (H2, `docs/decisions/open-questions/SCHEM.md` Q-SCHEM-32) apply in both modes of `xschem-compat`, each with a message and a regression test; the specs state them where they apply. The fuzzing schedule (H3, Q-SCHEM-33) is under "Fuzz targets".

| Concern | State | Where |
|---|---|---|
| `.sch`/`.sym` reader: nesting of embedded symbol blocks | Fixed by design: blocks are read on an explicit frame stack, no recursion; the reader is linear in the input. A skipped block (no instance before it, a symbol already loaded, any `[` inside a block that does not follow a `C`) is discarded line by line with no reader state; discarded blocks share one scratch document. **Approved cap (2026-11-21, H2, Q-SCHEM-32), both modes:** at most 64 nested block levels open at once (`schem::kMaxEmbeddedBlockDepth`); a `[` that would open level 65 is skipped to the next `]` line with `embedded symbol blocks nested deeper than 64 levels; block skipped` (SCHEM-FILE-007, `schematic-file-format` "Embedded symbols", spec issue F4 answered). Symbols nested in one block stop at level 40 as "Schematic used as symbol" says (`schem::kMaxBlockSymbolLevel`, SCHEM-FILE-008). Implemented in M8 round 2 (FORMAT); regression tests "nesting deeper than 64 blocks", "symbols nested 40 levels in a block stop reading the block" and "hostile nesting of blocks is read in bounded work" (`tests/unit/schem/file/embedded_test.cpp`). | `schem/file/reader.cpp` |
| Schematic used as a symbol that places itself | Round 1: cycle detection plus a depth of 40 (SCHEM-LIB-005). Spec issue Y5 answered 2026-11-21: xschem nests a self-placing schematic to 40 levels (no cycle detection), and the spec keeps that; the flattened-size cap below bounds the exponential case. Implemented in M8 round 2 (SYMLIB): the cycle check is gone, reading stops at level 40 at every level (SCHEM-LIB-005); regression tests "Recursion limit" and "Recursion limit: nothing after the deep instance is read at any level" (`tests/unit/schem/lib/schematic_symbol_test.cpp`). | `schem/lib/symbol_cache.cpp`, `schem/lib/schematic_symbol.cpp` |
| Schematic used as a symbol: flattened size | **Approved cap (2026-11-21, H2, Q-SCHEM-32), both modes:** at most 1,000,000 records per flattened symbol (40 distinct schematics that each place the next one twice would expand to about 2^40); the 1,000,001st record is not added, reading stops, and `Symbol <reference>: more than 1000000 records when flattened; the rest is not loaded` is reported once (`schematic-symbols-and-libraries` "Schematic used as symbol"). Implemented in M8 round 2 (SYMLIB, `schem::kMaxFlattenedRecords`, SCHEM-LIB-009; generated pins count); regression test "Flattened size limit" (2^40 chain, both modes, `tests/unit/schem/lib/schematic_symbol_test.cpp`). | `schem/lib/schematic_symbol.cpp` |
| `xschemrc` evaluation time | **Approved cap (2026-11-21, H2, Q-SCHEM-32), both modes:** evaluation of the `xschemrc` files of one document stops after 10 s (`SandboxLimits`), handled like a Tcl error, with `xschemrc <file>: evaluation stopped after 10 s` (`schematic-symbols-and-libraries` "xschemrc as data"); `while 1 {}` no longer hangs the opening. Implemented in M8 round 2 (SYMLIB): a progress-hook deadline plus `request_interrupt` over both files, which a script cannot catch (SCHEM-LIB-010, `XschemrcInput::time_limit`); `schematic open` and `new` evaluate the files since CMD round 2. Regression tests "Endless loop in an xschemrc" and "an endless loop that catches errors is still stopped" (`tests/unit/schem/lib/xschemrc_test.cpp`). | `schem/lib/xschemrc.cpp` |
| Bus notation: expansion size and parenthesis depth | **Approved caps (2026-11-21, H2, Q-SCHEM-32), both modes** (coded as named constants): at most `schem::kMaxBusBits` = 2^20 names per expansion and a parenthesis depth of 256; larger input keeps the name unchanged (SCHEM-NET-006, `schematic-netlisting` "Bus and vector notation"). Checked integer arithmetic for the indices. | `schem/net/bus.cpp` |
| Bus notation: leading repeat counts | Fixed in both modes: a run of `n*` prefixes is read in a loop, not by recursion (a 1 MB `1*1*...a` overflowed the stack). | `schem/net/bus.cpp` |
| Bus notation: expanded text size | **Approved cap (2026-11-21, H2, Q-SCHEM-32), both modes:** `kMaxBusBits` counts names, not bytes, so a 64 KB name with `[1048575:0]` asked for about 64 GB. At most 64 MiB (67,108,864 bytes, `kMaxBusBytes`) of expanded text per name, commas included (`too_large`, SCHEM-NET-006 `bus expansion of <string> exceeds a limit (...); used unchanged`), checked before the text is allocated. Post-processing applies the same cap to the lines of one vector element (a width marker repeating a wide field per name); past it the element is written unexpanded with SCHEM-NET-006. Regression tests: "Expansion past the limits", "bus notation: the expanded text is capped at kMaxBusBytes", "post-processing: a vector element past kMaxBusBytes is written unexpanded" (`tests/unit/schem/net/`). | `schem/net/bus.cpp`, `schem/net/postprocess.cpp` |
| Flat netlist: subcircuit nesting | **Proposed, not approved** (2026-11-21): a subcircuit body that instantiates itself would be substituted forever in `flat` mode; substitution stops at `schem::kMaxFlatDepth` = 64 levels and the deeper instance stays an `X` line. Regression test: "flat netlist: a subcircuit that places itself stops at kMaxFlatDepth". Awaiting the user (no message yet). | `schem/net/flat.cpp` |
| `expr` markers | Fixed in both modes: matching `)` positions are computed in one pass, so markers with no `)` cost linear, not quadratic, time. | `schem/net/expr_text.cpp` |
| Connectivity and pin ordering | Fixed: wires that are neither vertical nor horizontal find their probes by binary search on x; generated pins are ordered like the sibling `.sym` through buckets by name (both were quadratic). | `schem/net/connectivity.cpp`, `schem/lib/schematic_symbol.cpp` |
| `expr` nesting | **Approved cap (2026-11-21, H2, Q-SCHEM-32), both modes:** `schem::kMaxExprDepth` = 200 levels; deeper input fails like a syntax error, with `expr nested deeper than 200 levels; text copied without evaluation` (SCHEM-NET-006, `schematic-netlisting` "expr evaluation"). | `schem/net/expr.cpp` |
| Undefined integer and arithmetic cases | **Proposed, not approved** (2026-11-21): out-of-range integer fields and negative polygon counts skip the record (Q-SCHEM-34); bus counts of 0 or below are syntax errors (Q-SCHEM-35); `expr` `%` by a zero divisor gives NaN instead of crashing (Q-SCHEM-36). xschem's behaviour on these is undefined C. | `schem/file/`, `schem/net/` |

### Fuzz targets

`fuzz_sch_reader` (`.sch`/`.sym` read then written), `fuzz_schem_props` (property and brace strings), `fuzz_schem_bus` (bus notation and net naming) and `fuzz_schem_expr` (`expr` and format substitution) live in `tests/fuzz/` with seed corpora under `tests/fuzz/corpus/`. CI runs each for 60 s (`fuzz` job) and replays the seeds on every leg (`fuzz-seeds.*`). **Decided 2026-11-21 (H3, Q-SCHEM-33):** pull-request CI keeps 60 s per fuzz target, and a scheduled nightly workflow runs each fuzz target for 10 minutes; that nightly run satisfies the 10-minute run of task 6.4 of `openspec/changes/m8-schematic`. The workflow is `.github/workflows/fuzz-nightly.yml` (M8 round 2, FORMAT): every `tests/fuzz` target, found by its seed corpus, runs 600 s with the `linux-fuzz` build, split over four shards; crashes and the grown corpora are uploaded as artifacts (`workflow_dispatch` takes another duration).

## Layout input: GDS, CIF and the mask sections (M5 review, 2026-11-15)

GDS and CIF files come from foundries, IP vendors and other tools, so their readers treat every byte as hostile (hefesto-security "Untrusted input handling", design R9 of `openspec/changes/m5-mask-gds-cif/design.md`). The `cifoutput` and `cifinput` technology sections come with a PDK and get the same parser hardening, but a PDK is trusted to describe sensible operators (see "Accepted limits" below). The review covered `src/core/src/gds/` (stream, read, write), `src/core/src/cif/` (read, write) and `src/core/src/mask/` (style and input readers, generation), task 15.3.

### Bounds in place

| Concern | Bound | Where |
|---|---|---|
| Record framing | A record length below 4 or past the end of the stream ends the records (GDS-STREAM-001); payloads are at most 65531 bytes, so no read allocates more. | `gds/stream/records.cpp` |
| Gzip bombs | Inflating streams through a 64 KiB buffer and keeps at most 256 inflate checkpoints; the decompressed stream is capped at 256 times the compressed size, at least 1 GiB (GDS-STREAM-006, M5-S1). | `gds/stream/gzip.cpp`, `gds::inflate_cap` |
| Recursion (read) | Post-order look-ahead and magnified-copy reads nest at most 512 deep; magnified copies at most 32 deep and only while the scale stays within 2^30 (GDS-READ-011). Reference cycles are refused. | `gds/read/reader.hpp`, `sref.cpp`, `structure.cpp` |
| Recursion (write, generation) | Subtree loading and the writers stop at `gds::kMaxSubtreeDepth` (1024); hosts that write off the command thread run on a 64 MiB stack (`platform::run_on_stack`). | `gds/write/subtree.hpp`, `platform/stack_thread.hpp` |
| Recursion (commands) | The selected-use search (`db::find_selected_use`: `edit`, `flatten -doinplace`, the selection edits) and the `expand`/`unexpand` walks descend at most `db::kMaxSearchDepth` (1024) levels in both modes; `expand` reports VIEW-EXPAND-003 when it stops. A GDS read limits only nested look-ahead reads, so a library written leaf-first can be deeper than the stack. | `db/use_index.hpp`, `view/display_ops.cpp` |
| AREF counts | Counts are 16-bit and pitches 64-bit, so negative or extreme values cannot overflow (checked with crafted inputs); one AREF makes at most 2^20 flatten copies and one read at most 2^26 flatten tile paints (GDS-READ-011). | `gds/read/sref.cpp` |
| Read-plane triangles | A triangle a GDS or CIF read paints into its read planes (a polygon's, or a split tile a rescale or flatten copy repaints) descends into at most 1024 non-Manhattan pieces, drawn from a pool of the read that starts at 65536 and grows by 1024 per triangle painted (at most 65536 per polygon); a piece past it is painted as a rectangle (Q-MODEL-37, fix, approved 2026-11-17). The pieces stay linear in the triangles, the work does not: a chain of n pieces costs about n^2 tile steps, so at most about 1.05M per paint (0.26M for a database paint, which keeps the 512 of Q-MODEL-35) and about 1.05M per triangle over a read. Fuzz seed `regress-nm-near-parallel-diagonals` (fuzz_cif_reader). | `mask/input/engine.hpp` (`ReadTriangleBudget`) |
| Coordinates | GDS coordinates and scale products use 64-bit arithmetic and are clamped to the usable plane; CIF integers saturate at 2^61 and every sum and product is checked (Q-MASK-222). Mask-operator distances beyond `db::kPlaneMax` are rejected by the style reader and clamped after a rescale. | `gds/read/library.cpp`, `cif/read/parser.cpp`, `mask/style/` |
| Symbols and structures | A CIF read defines at most 2^20 symbols (later definitions are dropped with a message) and reads at most 2 GiB of text (CIF-READ-010). A GDS read has no structure count cap: cells grow with the file size. | `cif/read/symbols.hpp`, `cif/read/read.hpp` |
| Unit subdivision | CIF area labels and finer grids subdivide the units at most to the `cif limit` and 2^30. | `cif/read/parser_labels.cpp` |

Fixed in round 2 of the change, each with a regression test (listed under task 15.3 in `tasks.md`): saturated CIF sums overflowing 64 bits, quadratic `SymbolTable::free_name`, `VendorStamper` inflating a whole `.gds.gz`, `leading_integer` negating INT64_MIN, `CoordinateScale::set_scale` skipping the clamp, a gzip member boundary at the inflate feed window's edge, and mask-operator distances overflowing the 32-bit kernels.

### Findings of this review

| ID | Severity | Finding | Disposition |
|---|---|---|---|
| M5-S1 | Medium | A few KiB of gzip data inflate to many GiB; the streaming source never ran out of memory but kept a read busy for hours. Magic has no cap. | Fixed, approved as a fix in both modes on 2026-11-16 (Q-MASK-416). `make_gzip_source` stops at `gds::inflate_cap` (256 times the compressed size, at least 1 GiB) with GDS-STREAM-006, reported once by the reader. Tests "security fix M5-S1: ..." in `tests/unit/gds/stream/gzip_test.cpp` and `tests/unit/gds/read/hardening_test.cpp`. |
| M5-S2 | Medium | Every structure read appends its cell to the DRC pending list through `drc::check_this`, whose duplicate test is a linear scan (`drc::PendingList::append`), and `check_this` recurses through every ancestor: a GDS file with n structures costs O(n^2) (100 000 empty structures, 10 MB: 38 s in a debug build), a chain of n nested structures O(n^3) (5 000 levels, 350 KB: hours). Magic has the same algorithm. | Open: drc-engine code (ENGINE). Recommended fix: keep a count per cell next to the list so the Magic "never looks at the last entry" rule stays exact in O(1), and skip the parent walk when the parent already holds the area. Until then an accepted limit; `gds drccheck no` avoids it. |
| M5-S3 | Medium | `db::UseIndex` re-synchronized the whole index after every placement (`sync_locked` walked all uses when the use list was touched, and `db::place` touched it), so placing n uses in one cell cost O(n^2): the GDS duplicate-use probe after each SREF made a 200 000-instance cell (5.6 MB) take hours, and `gds read` of gf180mcu_re_efuse `efuse_wb_mem_1024x32.gds` (about 105 000 uses in one structure) passed the 1200 s limit of MASK-RT-gf180mcu_re_efuse-f04. | Fixed for `db::place`, `db::remove_use` and the GDS reader (round 6); other callers remain (below). `db::place` and `db::remove_use` change the use list without marking it touched (`CellDef::append_indexed_use`, `erase_indexed_use`), since they tell the index themselves; the enumeration order is unchanged (property test "Use index property: placing and removing without marking the list touched changes no order", `tests/unit/db/use_index/property_test.cpp`, round 6 review). Callers that still take `CellDef::uses()` for writing make the next index read rescan the whole list: `select/selection.cpp` (selection copies), `select/edit.cpp` (scratch uses), `cell/mag_reader_use.cpp` (`.mag` read), and the remove loops of `cell/flush_command.cpp` and `gds/read/reader.cpp` `clear_definition`. Since round 7 (`m5-r7-expand`) `UseIndex::sync_locked` first runs an allocation-free check (same uses, serials and boxes) and reconciles only when that check finds a difference, so those callers cost one O(n) comparison per later index read, not a rescan and rebuild per placement; moving the read-only loops to `std::as_const(def).uses()` would drop that comparison too. That read went from past 1200 s to 148 s in a debug build. Still O(n) per placement: `db::place`'s default identifier scan (DBLinkCell), which Magic's GDS reader never makes; `db::PlaceOptions::link_id = false` skips it, and the GDS reader sets it (`gds/read/sref.cpp`, round 6 integration, as Magic's calmaElementSref does): the read then takes 14 s. The reader's duplicate check (DBCellFindDup) also no longer searches the index per SREF: `Reader::duplicate_placement` checks the uses placed in the structure being read (round 6, GDSR). The whole MASK-RT-gf180mcu_re_efuse-f04 case took 81 s locally (macos-debug at -O2). Regression tests "security fix M5-S3: place and remove do not make the use index rescan the use list" (`tests/unit/db/hierarchy_test.cpp`), "regression: many uses in one structure are checked without rescanning the index" "regression: a read SREF leaves no default-id entry in the parent's id table" and "regression: a duplicate placement is found after a grid subdivision in the same structure" (`tests/unit/gds/read/sref_test.cpp`; the incremental check kept the old scale's keys after a subdivision, round 6 review). |
| M5-S4 | Low | Hierarchical generation visits every interaction square (at least 50 internal units on a side) that a use reaches, as Magic's CIFGenSubcells does, and builds a DRC context per square: a 263-byte GDS whose AREF spans the plane makes the write after the read run for days. | Accepted limit (Magic behaves the same): the write is cancellable through `mask::Progress`. Reported to GEN as a performance item (reuse one context per generation). The fuzz targets skip writes of cells spanning more than 20 000 squares. |
| M5-S5 | Low | Polygon decomposition (`db::decompose_polygon`) is quadratic in the point count. GDS bounds a BOUNDARY to 8191 points, but a CIF polygon or wire has no point limit: a 20 000-point polygon (190 KB) takes 52 s in a debug build. Magic's decomposition is quadratic too. | Accepted limit until decided: capping CIF points per element would refuse input Magic reads (a new decision, recorded for the user). |
| M5-S7 | Medium | Making a read polygon Manhattan (`db::decompose_read_polygon`, CIFMakeManhattanPath) splits a diagonal edge, and each split searches the whole path for a crossing (path_intersect). A split that finds one leaves a shorter diagonal behind it: two edges with huge, nearly parallel slopes split about once per unit of length, so a 5-point GDS boundary never finished (CI fuzz run 43754937069, `fuzz_gds_reader` timeout). The first fix counted splits (1024 + n^2 + n), which still left O(n^3) work (about 5 x 10^11 edge visits for the XY record's 8191 points; round 5 review). | Fixed, approved as a fix in both modes on 2026-11-16 (Q-MASK-416, `docs/decisions/m5-decision-table.md` "User decisions of 2026-11-16"): one path gets at most 2^24 + 16 n^2 edge visits for n vertices (quadratic, as the strip decomposition, M5-S5; about 1.1 x 10^9 visits, seconds in Release, for 8191 points), then stays non-Manhattan and is reported as one (MASK-INPUT-020's "non-manhattan polygon."). The fixed part matters: the splits depend on the geometry, and a 9-point 45-degree power-rail band of gf180mcu_fd_io needs 262 splits and about 88 000 visits; a first budget of 16 n^2 + 4096 refused it (MASK-RT-gf180mcu_fd_io-f01 failed in Performance run 43789233970) and is replaced. The largest need seen in the round-trip libraries is about 190 times below 2^24. Regression tests "the read decomposer bounds the splits of a degenerate path" and "security fix M5-S7: the read decomposer bounds the work of a crafted path" (including the gf180mcu_fd_io band) (`tests/unit/db/paint_commands_test.cpp`) and the seed `tests/fuzz/corpus/fuzz_gds_reader/regress-timeout-6faed2e3...`. A behaviour change from Magic (which has no bound) on such input only. |
| M5-S8 | Medium | The use index's sub-binning (`db::use_index_detail::bin_array_build`, bpBinArrayBuild) recursed without end when a bin's area did not shrink: eleven 1x1 uses in a 3x2 box size bins of 2x3, taller than the box, and each bin rebuilt the same array until the stack overflowed (CI fuzz run 43776774834, `fuzz_cif_reader`). Magic crashes on it too: the same uses placed with `getcell` and binned by a partial `expand` (a use-order probe case) made magic exit with SIGSEGV, status 139 (golden-capture run 43789239840, round 5 review), so the case cannot be a golden case. | Fixed (round 5 integration): sub-binning stops at 64 levels and the deepest bins keep plain lists, which changes the enumeration order of such degenerate arrays only. Approved as a fix in both modes on 2026-11-16 (Q-MASK-416): Magic has no result to match here. Regression test "Use index: degenerate bins stop sub-binning" (`tests/unit/db/use_index/use_index_test.cpp`) and the seed `tests/fuzz/corpus/fuzz_cif_reader/regress-crash-2e18953f...`. |
| M5-S6 | Info | The content-stamp table of a read-only read grows with the number of structures and SNAME records, as Magic's does. | Accepted limit (linear in the file size). |

### Fuzz targets

`fuzz_gds_reader` (plain and gzip, with the content-hash scan, a seek back and an in-memory GDS write after the read), `fuzz_cif_reader` (read, then a CIF or GDS write), `fuzz_cifoutput_section` and `fuzz_cifinput_section` (design D21) live in `tests/fuzz/`; their file comments list the mode bits and resource caps. `tests/fuzz/gen_mask_seeds.py` regenerates their seeds from the MASK golden cases and the test technologies (`--check` reports a stale corpus); hand-added regression inputs are named `regress-*`. CI runs each for 60 s (`fuzz` job) and replays the seeds on every leg (`fuzz-seeds.*`).

When you change a GDS, CIF or mask reader, replay the seeds of these targets, and for a change to a bound above add a regression test named "security fix M5-..." or extend this table.
