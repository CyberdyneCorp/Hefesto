# Building Hefesto

Hefesto builds with CMake 3.25 or later and a C++20 compiler: GCC 11+, Clang 14+ or Apple Clang 15+ (capability `hefesto-platform-targets`). Builds are out of tree, under `build/<preset>`. On Linux, the pinned libcurl that `hefesto-cli` and the desktop download with is built against the system OpenSSL 3, so install its headers first (`libssl-dev` on Debian and Ubuntu, `openssl-devel` on Fedora).

## Quick start

```sh
# macOS (Apple silicon)
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug

# Linux
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

`cmake --list-presets` shows the presets available on the current host.

## Presets

| Preset | Host | What it builds |
|---|---|---|
| `linux-debug`, `linux-release` | Linux | core, CLI, tests; desktop app if a Vulkan SDK is found |
| `macos-debug`, `macos-release` | macOS | the same, arm64 only, macOS 14 deployment target |
| `macos-xcode` | macOS | an Xcode project (build and test with `--preset macos-xcode`, Debug configuration) |
| `ipados` | macOS | an Xcode project for iPadOS 17, core only, plus the `hefesto_ios_link_probe` app that links the core (build it with `cmake --build build/ipados --target hefesto_ios_link_probe --config Debug -- CODE_SIGNING_ALLOWED=NO`) |
| `linux-asan`, `macos-asan` | Linux / macOS | Debug with AddressSanitizer and UndefinedBehaviorSanitizer |
| `linux-tsan` | Linux | Debug with ThreadSanitizer, desktop off (the background DRC worker and its race tests); add `-DHEFESTO_BUILD_DESKTOP=ON -B build/linux-tsan-desktop` to build the desktop DRC host's tests under TSan, as CI does |
| `linux-fuzz` | Linux | Clang libFuzzer targets (Apple Clang has no libFuzzer) |
| `ci-linux`, `ci-macos` | CI | RelWithDebInfo, warnings as errors; Linux takes the compiler from `CC`/`CXX` |
| `linux-bench`, `macos-bench` | Linux / macOS | Release with the desktop app and canvas benchmark forced on (Vulkan required); reproduces the canvas spike |
| `ci-linux-desktop` | CI | `ci-linux` with the desktop app and canvas benchmark forced on, and the window smoke tests registered (`HEFESTO_TEST_DESKTOP_WINDOW`) |
| `ci-macos-desktop` | CI | `ci-macos` with the desktop app forced on (MoltenVK required) |

## Options

| Option | Default | Meaning |
|---|---|---|
| `HEFESTO_BUILD_DESKTOP` | ON if a Vulkan SDK is found, else OFF; always OFF for iPadOS | Vulkan renderer, Raven desktop shell, `hefesto` app |
| `HEFESTO_BUILD_TESTS` | ON | unit tests and golden-test registration |
| `HEFESTO_BUILD_FUZZ` | OFF | libFuzzer targets; when OFF (and tests are on), the same targets are built with a replay driver and their seed corpora run as CTests `fuzz-seeds.<target>` |
| `HEFESTO_BUILD_BENCH` | OFF | canvas benchmark (needs the desktop build) |
| `HEFESTO_WARNINGS_AS_ERRORS` | OFF | `-Werror` for first-party code |
| `HEFESTO_SANITIZERS` | empty | comma list for `-fsanitize=`, applied to all targets |
| `HEFESTO_TEST_DESKTOP_WINDOW` | OFF | registers the window smoke tests (labels `desktop-window-x11`, `desktop-window-wayland`): each starts `hefesto --smoke-frames 30` through Raven and needs a display |
| `HEFESTO_BUILD_NGSPICE` | OFF | builds the simulation engine library `libngspice-hefesto` from the pinned ngspice-47 (`third_party/ngspice/`, M9); see [Simulation engine library](#simulation-engine-library-m9) |
| `HEFESTO_DEVELOPER_BUILD` | OFF | developer build: also builds the in-process engine host `hefesto_simd_devhost` (Q-SIM-22 (a)); never for a release. `scripts/check_engine_boundary.py` reads it from `CMakeCache.txt`; see [The simulation worker](#the-simulation-worker-hefesto-simd) |

Pass options after the preset, e.g. `cmake --preset macos-debug -DHEFESTO_BUILD_DESKTOP=OFF`.

The core and the CLI never need Vulkan or Raven.

## Desktop app prerequisites

The desktop app (`hefesto`), its renderer and the canvas benchmark need Vulkan headers and a Vulkan library that CMake's `find_package(Vulkan)` can find. Raven is fetched at its pinned commit.

- **Linux:** `libvulkan-dev` and `libx11-dev` (plus `libwayland-dev`, `libxkbcommon-dev` and `libdecor-0-dev` for the Wayland backend). Without a GPU, `mesa-vulkan-drivers` (lavapipe) lets the GPU readback tests run instead of being skipped. Set `HEFESTO_REQUIRE_VULKAN_DEVICE=<part of the device name>` (for example `llvmpipe`) to make them fail instead of skip when no such Vulkan device is found; the CI job `gpu-readback-lavapipe` does, with `VK_DRIVER_FILES` pointing at the lavapipe manifest (`/usr/share/vulkan/icd.d/lvp_icd*.json`).
- **macOS:** the LunarG Vulkan SDK, or Homebrew `molten-vk`, `vulkan-headers` and `vulkan-loader`. With MoltenVK alone, point CMake at the Khronos headers and the MoltenVK library:

  ```sh
  git clone --depth 1 --branch v1.4.321 https://github.com/KhronosGroup/Vulkan-Headers.git /path/to/vulkan-headers
  cmake --preset macos-bench \
    -DVulkan_INCLUDE_DIR=/path/to/vulkan-headers/include \
    -DVulkan_LIBRARY=/opt/homebrew/lib/libMoltenVK.dylib
  cmake --build --preset macos-bench
  ctest --preset macos-bench
  ```

`hefesto --help` lists the app's options; `--rects N --bench-frames F` runs the in-window benchmark and `build/<preset>/bench/canvas/hefesto-bench-canvas` the headless one (see `docs/spikes/canvas-spike.md`).

## Running the tools

Both front ends run the same Magic-compatible startup (`app::run_startup`, capability `app-startup`): settings layers, the project's sandboxed Tcl interpreter, startup files (evaluated as Tcl; a PDK's rc file only after consent, see below), the technology, then the layout files named on the command line, then the box. `CAD_ROOT` (environment) is the Magic installation root: its `magic/sys` directories are searched for technology, display-style and font files (FreeSans/FreeSerif/FreeMono `.pt3`, needed for `flabel` labels); without it there are no system files and the session starts on the built-in `minimum` technology.

**Installation root.** When `CAD_ROOT` is unset or empty, `hefesto-cli` and the desktop app use the install root that holds the running executable (decision M8-D10, approved 2026-11-23): the first of the executable's folder, `<folder>/../Resources` (a macOS app bundle) and `<folder>/..` (an install prefix with the executable in `bin/`) that contains an `assets` folder. The build copies `assets/schematic/` next to `hefesto-cli` and `hefesto` (`build/<preset>/src/cli/assets/schematic`, `build/<preset>/src/shell/desktop/assets/schematic`), and `cmake --install` puts it under `<prefix>/assets/schematic`, so the schematic system library (`<root>/assets/schematic/xschem_library/devices`, the `${XSCHEM_SHAREDIR}/xschem_library/devices` the PDK `xschemrc` files append) is found without setting anything. A `CAD_ROOT` set in the environment still overrides it; then `$CAD_ROOT/assets/schematic` must hold the library. The build-tree copy is refreshed when the executable is relinked.

### `hefesto-cli`

| Option | Meaning |
|---|---|
| `-T <tech>` | technology to load at startup (a name resolved through the PDK store, or a `.tech` path) |
| `<cell>.mag`, `.lef`, `.def`, `.cif`, `.gds` | layout files loaded at startup, in command-line order; any other word is a script |
| `--project=<dir>` | project folder (default: `.` when it holds `hefesto.project`); its PDK pin selects the technology |
| `--pdk-root=<dir>` | PDK root (default: setting `pdk-root`, then `$PDK_ROOT`, then `~/.ciel`); the session's PDK store resolves through the PDK manager's installs there |
| `--setting <name>=<value>` | a session setting (the highest settings layer); e.g. `undo-history-limit=2000` (undo steps kept, at least Magic's 1000) |
| `--interactive` | after the scripts, read commands from standard input at a prompt (`%` idle, `]` while background DRC is busy; the background DRC worker runs only here); implied by `--magic` on a terminal without `-c` or scripts; give it before `--magic` |
| `--magic ...` | the rest is Magic's own command line (`-T`, `-d`, `-rcfile`, `-norcfile`, `-r`, cells, a `.tcl` script); the `.tcl` script is evaluated as Tcl in the project's sandboxed interpreter, then Tcl commands are read from standard input (at the interactive prompt when it is a terminal); only `puts` prints, as in Magic |
| `--trust-pdk <selector>` | let this PDK's scripts run in this batch session (its `.magicrc`, given with `-rcfile` or by the project pin, and the device scripts it sources): a family (`sky130`), a variant (`sky130A`), `<variant>@<hash prefix>` or `<family>/<variant>@<version>`; repeatable, give it before `--magic`. Without it a batch run declines every PDK script the PDK store does not already trust (`SCRIPT-TRUST-001`), and the session keeps the `minimum` technology. A family or variant selector also trusts a copy of that PDK found outside the PDK store (for example a PDK tree named by `-rcfile` or `PDKPATH`): name only PDKs you mean to run (accepted limit S-12, `docs/contributing/security.md`) |
| `--strip-reflibs <dir>` | remove the `string GDS_START`/`GDS_END` lines from every `.mag` file in `<dir>` (Magic's reference-library stripper) and exit |
| `-c <commands>` | run commands, e.g. `-c "tech load sky130A; tech layers"` |
| `--diagnostics=text\|json\|magic` | diagnostic format on stderr; `magic` prints Magic's console text and each script command's result, as the golden reference scripts do (see `tools/golden/README.md`) |
| `--diagnostic-codes[=AREA]` | print a diagnostic code table (`core`, `tech`, `db`, `cell`, `display`, `view`, `pdk`, `app`, `undo`, `select`, `connect`, `ui`, `wiring`, `drc`, `script`, `pcell`, `tools`, `gui`, `hints`, `mask`, `gds`, `cif`) |
| `--log-dir=<dir>`, `--no-log` | session log location, or none |

Example: run the sky130A rc file of the pinned data set and a Tcl script that places a generated transistor.

```sh
V=.cache/pdks/ciel/sky130/versions/5740c4a81c42f9fe4af5ef2ee258a99151053d28
build/macos-debug/src/cli/hefesto-cli --no-log --trust-pdk sky130A --pdk-root=$V \
  --magic -dnull -noconsole -rcfile $V/sky130A/libs.tech/magic/sky130A.magicrc place_nfet.tcl
# place_nfet.tcl:  load top -silent; box 0 0 0 0; magic::gencell sky130::sky130_fd_pr__nfet_01v8 nf1 w 1.0 l 0.15; writeall force; quit -noprompt
```

Outside `--magic` mode, `-c`, script files, standard input and `--interactive` are Tcl too (M4), read like a Tcl console: one complete command at a time in the project's sandboxed interpreter, and each command's non-empty result is printed (in `--magic` mode, as in Magic's batch mode, only `puts` prints). A Tcl error stops that source at its line, and a command that fails quietly still makes the run exit 1. For example `-c 'tech name'` prints the technology's name (the command's result), and `-c 'puts [tech name]'` prints it once (the `puts`; its empty result prints nothing). The golden replays run each case's `commands.tcl` with `--magic -dnull -noconsole -norc`, so no user `.magicrc` takes part.

**Scripts and trust.** Tcl runs in a sandbox (capabilities `hefesto-scripting`, `hefesto-security`): no `exec`, `socket`, `load`, `exit` or command pipelines, and the engine command `shell` is refused from every origin, typed lines included (`SCRIPT-SANDBOX-001`; Hefesto runs no external programs); file access only under the granted roots (the project folder, the PDK store, the startup files and the `.tcl` script named on the command line, the working directory and `CAD_ROOT`); a synthesized `env` with `PDK_ROOT`, `PDK` and `CAD_ROOT`. Tcl reads script files in the system encoding of the locale, as Tcl does on Linux on every platform: `LC_ALL`/`LC_CTYPE`/`LANG` set to `C` or `POSIX` means ISO 8859-1 and a `.UTF-8` locale means UTF-8 (with no locale variable set, Tcl's platform default stays), so a script's non-ASCII text and the gencell names hashed from it match Magic's. A PDK's scripts run only with consent: the desktop asks before the first one runs (Don't run, Run once, Trust this version; the prompt names the script, the PDK release, what the startup file runs next and, for a PDK store release, whether its digest verified), and `hefesto-cli` declines unless `--trust-pdk` names the PDK. Trusting a version records it in the PDK store (`Provenance::trusted`), so later sessions, batch ones included, run it without asking; the PDK panel lists the trusted versions with Trust scripts / Revoke trust, and `pdk trust|untrust <variant> <version>` does the same from the console. A declined layout opens read-only with the `minimum` technology (`SCRIPT-TRUST-005`); Review scripts… asks again.

### `hefesto` (desktop)

`hefesto [-T tech] [-r] [-norcfile] [cell.mag ...]` takes Magic's startup words (`-T`, `-r` crash recovery, `-norcfile`, cells), plus:

| Option | Meaning |
|---|---|
| `--asset-dir DIR` | where the bundled UI fonts and icons are read (default: the source tree's `assets/`) |
| `--theme dark\|light` | the UI theme (dark by default; the canvas stays dark in both) |
| `--width PX`, `--height PX` | the initial window size |
| `--console` | open the Console panel (command output) at startup |
| `--run 'CMD; CMD ...'` | run these commands once the canvas has its size (after startup), e.g. `--run 'see no metal1; zoom 2'` |
| `--pdk-manager` | open the PDK manager panel at startup |
| `--capture FILE.ppm [--capture-frames N]` | render N frames (default 1), write the window to a PPM file and exit (screenshot checks of `docs/design/m1-ui-conformance.md`) |
| `--capture FILE.ppm --capture-running` | write the window in the first frame a running script draws with its Interrupt card shown (a `--run` line that runs over 2 s), then exit when it ends (`docs/design/m4-ui-conformance.md`) |
| `--capture-window main\|pcell\|texthelper\|reorder` | with `--capture`, save that window instead of the main one (the PCell dialog, text helper or reorder dialog once a `--run` line opened it) |
| `--drag X0,Y0,X1,Y1`, `--pinch X,Y,FACTOR` | after `--run`, a left-button drag or a pinch at window points (scripted checks) |
| `--input 'STEP; STEP ...'` | after `--run`, play pointer and key events frame by frame: `move X,Y`, `down X,Y`, `up X,Y`, `key [MOD+]NAME`, `text T`, `wait MS`; prints each panel's position and size at the end (`docs/design/m2-ui-conformance.md` "M4 polish round") |
| `--press X,Y` | after `--run`, press and hold the left button at a window point (opens a menu of the in-window bar for a capture) |
| `--menu-bar-in-window` | draw the menu bar inside the window on macOS too (the system menu bar is the default), so captures see it |
| `--no-vsync`, `--validation` | presentation without vsync; Vulkan validation layers |
| `--rects N`, `--seed N`, `--bench-frames N`, `--bench-output FILE`, `--smoke-frames N`, `--expect-window-system` | benchmark and smoke-test modes (`docs/spikes/canvas-spike.md`, CI window tests) |

**Canvas input (M2).** Pointer buttons and keys on the canvas go through Magic's macro tables of the current tool, as in Magic: with the box tool, the left button moves the box by its lower-left corner, the right button resizes it by its upper-right corner and the middle button paints by example; keys follow Magic's default bindings (for example `f` is `sideways` and `v` is `view`; `:` opens the command bar). `tool` (or the tool rail) switches between the box, wiring and pick tools. The view moves directly: Option-drag (Alt-drag on Linux) pans, the wheel and pinch zoom about the pointer, and two-finger trackpad scroll pans; the middle and right drags no longer pan, because Magic gives those buttons to the box tool. Shell commands added in M2: `panel layers|inspector|console show|hide|toggle` collapses and restores a panel, and `palette paint LAYER` sets the palette's paint layer. Key bindings can be changed with `macro` and `imacro`; a file of `macro` lines can be run with `source FILE`.

**Key-binding files (M2).** A key-binding file is a text file of Magic `macro` / `imacro` command lines, for example `macro ^Z "undo"` or `macro XK_F5 "zoom 2"`. Key names are X11 keysym names on every platform (the keysym table is built in, so `XK_*` names work on macOS too); `^X` is Control-X. Load one with `source FILE` in the command bar or a script, or put its lines in a `.magicrc` read at startup. `macro` lists the current tool's bindings; the desktop Keys tab (`panel keys show`) shows the same tables.

**Command names (M2).** Scripts, `-c` lines, macros and `source` files need exact command names, as Magic's Tcl build does: `vers` gives `invalid command name "vers"` (decision Q-UI-41). Lines typed in the desktop command bar still accept Magic's unique abbreviations (`vers` runs `version`). In the command bar, GNU Readline history expansion applies (`!!`, `!n`, `!string`, `^old^new^`, `:p`); `history` lists the typed lines.

**Undo.** Every editing command is undoable (`undo [N]`, `redo [N]`, the `u`/`U` keys, the Undo and Redo buttons; right-click either button for the undo history). `undo disable` stops recording (the top bar shows "undo off"; click it to issue `undo enable`). The number of steps kept is the setting `undo-history-limit` (default and minimum 1000, Magic's limit), e.g. `--setting undo-history-limit=5000` or in a settings file.

**Desktop panels (M2).** `panel layers|inspector|console|keys show|hide|toggle`, `panel layers|inspector dock left|right`, `panel reset`; F6 / Shift-F6 move the keyboard focus between the tool rail, Layers, canvas, Inspector, Console and command bar. `specialopen color [INDEX]` shows Magic's color window in the right dock.

**Design rule checking (M3).** Both front ends run Magic's `drc` command set (`drc check`, `catchup`, `count`, `find`, `why`, `listall`, `ignore`, `style`, `euclidean`, `printrules`, and the rest). The DRC settings are session state set by commands, as in Magic: `drc on|off` (the background checker; on by default), `drc style NAME` (the technology's DRC styles, e.g. `drc(fast)` and `drc(full)` in sky130A), `drc euclidean on|off`, `drc *halo N` and `drc *stepsize N` (a positive integer; 0, negative and non-numeric values are refused with `Step size must be a positive integer.`, decision Q-DRC-24). The background checker runs in the desktop app and in `hefesto-cli --interactive`; scripts, `-c` and piped input are batch and check only on `drc check; drc catchup`. In the desktop app the top-bar chip and the status bar show the DRC state (not run, checking, clean, N errors, off; N is `drc list count total`), and `panel drc show|hide|toggle` (or a click on the chip) opens the DRC panel: errors grouped by why-text, `#k of n` stepping (`drc find`), Re-check (`select top cell; drc check; drc catchup`), ignored rules and the style selector, each a logged command. When a long `drc catchup` ends while the window is not focused, the app requests the Dock's attention and posts a message in the window; `hefesto::notifications off` turns that off (setting `notifications`, on by default).

The desktop installs the same host services as `hefesto-cli` (`src/platform_posix`: POSIX file locks, the crash-backup file host, the process runner; `src/platform_net`: the HTTPS Fetcher): a session holds a lock on its crash-backup file, `crashbackups` saves periodically, SIGTERM and SIGHUP save a backup before exiting, a normal exit removes it, and `-r` (or `crash recover`) restores the newest backup whose session is gone.

### GDS and CIF

Both front ends read and write GDS II and CIF with Magic's `gds` (alias `calma`) and `cif` commands (capabilities `gds-read`, `gds-write`, `cif-format-io`); the masks come from the technology's `cifoutput` and `cifinput` styles. With the sky130 data set fetched (see "Reference data for tests"), this reads a `sky130_fd_sc_hd` cell from its `.mag` file, writes it as GDS and reads the file back:

```sh
V=.cache/pdks/ciel/sky130/versions/5740c4a81c42f9fe4af5ef2ee258a99151053d28
build/macos-debug/src/cli/hefesto-cli --no-log --pdk-root=$V -T $V/sky130A/libs.tech/magic/sky130A.tech \
  -c "load $V/sky130A/libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__inv_1.mag; property GDS_FILE {}; gds write inv; gds read inv"
```

The library cells carry `GDS_FILE` (a vendor view): a write copies such a cell from the library GDS, which the `sky130` set does not include, and the window's own vendor cell is then skipped without a message, as in Magic, leaving an empty library. `property GDS_FILE {}` removes the property, so the write generates the masks from the cell's paint. `--pdk-root` makes `$V` trusted, so the `load` of a path under it works; reading the file back into the same session warns that the cell already existed.

- **Dates and the time zone.** GDS BGNLIB/BGNSTR dates and the CIF header date are written in the host's local time zone, as Magic does (Q-MASK-203), so `gds nodatestamp` or `cellname timestamp <cell> 0` output is reproducible only under a fixed zone. Pin the zone with the setting `gds-time-zone` (decision Q-MASK-132), a POSIX TZ string read at startup, for example `--setting gds-time-zone=UTC0` or a `gds-time-zone` key in the project or preferences file; it applies to `gds read`, `gds write` and `cif write`, and when it is unset or empty the host zone (`TZ`) is used. The golden replay and the unit tests pin the zone the same way.
- **Compressed input.** `gds read` opens `<name>.gds`, `.gds.gz`, `.gds2`, `.strm` or the name itself. A `.gz` file inflates to at most 256 times its compressed size (at least 1 GiB) before the read stops with `GDS-STREAM-006`.
- **Desktop.** File > Read GDS and Write GDS issue `promptload gds` and `promptsave gds`. A long read or write keeps the window drawing and shows the Interrupt card ("Writing chip.gds · N s", "Completed N%"); Interrupt stops it without leaving an output file.

### Managing PDKs

The `pdk` command (both front ends; capability `hefesto-pdk-manager`) manages the installs under the PDK root:

| Command | Meaning |
|---|---|
| `pdk list` | installed versions; the columns include the disk size and the projects (from the recent-projects list) that pin each version |
| `pdk remote FAMILY ?-prereleases?` | versions available from the catalog |
| `pdk install VARIANT ?VERSION\|latest? ?-lib NAME ...? ?-enable? ?-progress?` | download, verify (SHA-256) and unpack; `-progress` reports `PDK-PROGRESS-001` diagnostics (`phase=… part=i/n bytes=… total=… asset=…`) for a front end's progress bar |
| `pdk updates ?-auto?` | newer stable versions of the installed families; `-auto` checks at most once a day and only when the setting `pdk-update-check` is on (default) |
| `pdk uninstall FAMILY VERSION ?-force?` | refused with `PDK-UNINSTALL-001` while a recent project pins the version, unless forced |
| `pdk export FAMILY VERSION ?-to DIR?` | write an offline pack (`pack.json` and the assets) |
| `pdk import PACK ?-accept-unverified? ?-progress?` | install from an offline pack without network access; every asset is verified, and the install is recorded as unverified (packs are not signed yet) |
| `pdk enable`, `pin`, `info`, `remove-library`, `root` | see the usage that `pdk` prints without arguments |

Downloads run in process (capability `hefesto-licensing`, decision D-13; `src/platform_net`): URLSession on macOS, the pinned libcurl on Linux, HTTPS with the system trust store, redirects followed, `file://` read from disk. A partial download is resumed with a range request, and a transient failure (timeout, host not resolved, connection lost, HTTP 408, 429, 500, 502, 503 or 504) is retried three times after 1 s, 2 s and 4 s, like `curl --retry 3`. `GITHUB_TOKEN`, when set, is sent to the GitHub API only (`api.github.com`). The core keeps a `curl`-process Fetcher (`pdk::make_process_fetcher`) for hosts that supply none.

## Reference data for tests

The PDK and Magic-reference tests use pinned data that is fetched, never committed (design D8/D9 of `openspec/changes/m1-core-viewer`, D15 of `openspec/changes/m3-drc` and of `openspec/changes/m4-scripting`). Without it they skip cleanly.

```sh
# After building hefesto-cli (any preset): the tech/.mag PDK data sets, then Magic's sources.
python3 scripts/fetch_pdk.py --cli build/macos-debug/src/cli/hefesto-cli
```

Each PDK data set is a subset of one ciel release, installed into `.cache/pdks/ciel/<family>/versions/<pin>/` by `hefesto-cli`'s own `pdk install … -only …` and verified against a pinned tree digest (SHA-256 over the sorted `path NUL sha256 LF` records of the subset's files). Tests use only these exact versions (`tests/unit/common/reference_data.hpp`), never another installed one.

| Set | Release (pin) | Subset kept | Downloaded | Kept |
|---|---|---|---|---|
| `sky130` | `sky130-5740c4a81c42f9fe4af5ef2ee258a99151053d28` (open_pdks, stands in for `c6df28c1`) | `sky130A/libs.tech/magic/`, `sky130A/libs.ref/sky130_fd_sc_hd/mag/`, `sky130A/libs.ref/sky130_fd_pr/mag/` | 148 MB (`common` 6.6 MB, `sky130_fd_sc_hd` 127 MB, `sky130_fd_pr` 14 MB) | 15 MB, 791 files (446 + 290 cells) |
| `gf180mcu` | `gf180mcu-5740c4a81c42f9fe4af5ef2ee258a99151053d28` (open_pdks, stands in for `c6df28c1`) | `gf180mcuD/libs.tech/magic/`, `gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag/`, `gf180mcuD/libs.ref/gf180mcu_fd_pr/mag/` | 350 MB (`common` 170 MB, `gf180mcu_fd_sc_mcu7t5v0` 180 MB, `gf180mcu_fd_pr` 0.05 MB) | 3.4 MB, 267 files (229 + 21 cells) |
| `ihp` | `ihp-sg13g2-5e6d592e4002946a4616f798c357f0f3c06cf3b6` (IHP-Open-PDK) | `ihp-sg13g2/libs.tech/magic/` | 435 MB (`common` 434 MB, `sg13g2_pr` 0.2 MB) | 0.8 MB, 37 files |

- **Why `5740c4a8` for `c6df28c1`.** The specs and the SCRIPT golden cases pin open_pdks `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607` (Q-SCRIPT-9), for which ciel publishes no release. The next release, `5740c4a8` (2026-11-10), is three commits later and changes only `VERSION` and the sub-library `reference` blocks of `sky130.json`/`gf180mcu.json`; no Magic template, script or Makefile changes, so its `libs.tech/magic` files are those of `c6df28c1` except for the build stamps every release carries (the tech `version` line, `1.0.588-126-g5740c4a`, and the build directory in the fallback `PDK_ROOT`/`TECHPATH` of the `.magicrc` and `.tcl`). `scripts/fetch_pdk.py` names both (`OPEN_PDKS_SPEC_PIN`, `OPEN_PDKS_PIN`); design D15 of `openspec/changes/m4-scripting` records the check. The `ihp` set is IHP-Open-PDK `5e6d592e` itself; its `libs.tech/magic` files equal that commit's byte for byte.
- The `*_fd_pr/mag/` directories hold the primitive-device cells that the fixed-layout (`nocell`) PCell devices place with `getcell` (for example `sky130_fd_pr__rf_npn_05v5_W1p00L1p00`); the native generators are checked against them (`tests/unit/pcell/native/`, `scripts/pcell_equivalence.py`). They were added in M4 round 2 with the same pins, which changed the `sky130` and `gf180mcu` digests and cache keys. IHP's `sg13g2_pr` has no `mag/` directory (its fixed devices build their cells with the PDK's own cell scripts), so the `ihp` set is unchanged.
- The M4 pins replace open_pdks `0fe599b2` and IHP `ddb601a4` of M1–M3. Those older sets are not fetched any more; an old version directory left in `.cache/pdks` is ignored by the tests.
- A cold cache downloads about 0.95 GB in all. `pdk install` keeps no archive after the install, but it decompresses each archive in full before filtering, so the `gf180mcu` install needs about 1.6 GB of free disk space for a moment (`ihp` about 1.2 GB).
- `pdk install` needs at least one library: `ihp` installs `sg13g2_pr`, the smallest, and keeps none of its files.
- `magic-ref`: a sparse, shallow checkout of Magic at `fc5daa9e` into `build/ref/magic` with only `scmos/`, `graphics/` and `tcltk/` (display styles, color maps, glyphs, the `.pt3` fonts, the scmos tech sources, the auxiliary Tcl scripts; about 5 MB). A full clone (`git clone https://github.com/RTimothyEdwards/magic build/ref/magic && git -C build/ref/magic checkout fc5daa9e`) works too and is what reading Magic's sources for porting needs.
- `scmos.tech` is generated from `build/ref/magic/scmos` at configure time (`tests/data/tech/generate_scmos.cmake`, run by `tests/unit/tech/CMakeLists.txt`), so fetch `magic-ref` before configuring, or reconfigure afterwards.
- Golden replay (`ctest -L golden`, `tools/golden/replay.py`) takes the outline fonts that `flabel` cases need from `build/ref/magic/graphics/` and the auxiliary scripts some SCRIPT cases source from `build/ref/magic/tcltk/` (`magic-sys` and `magic-tcl` external inputs) and runs `hefesto-cli` with its own `CAD_ROOT`, so a `CAD_ROOT` set in your shell does not change the results; cases whose data is missing are skipped.

### Library GDS data sets (M5)

The M5 library round trips (`MASK-RT-*`), the GDS benchmark and the GDS read tests use the library GDS files of the same three releases (design D19 of `openspec/changes/archive/2026-11-24-m5-mask-gds-cif`). They come in six more data sets, fetched only when named (a bare `fetch_pdk.py` does not fetch them):

| Set | Libraries (`<variant>/libs.ref/<lib>/gds/`) | Downloaded | Kept (cached) | Peak disk |
|---|---|---|---|---|
| `sky130-gds` | sky130A: `sky130_fd_io`, `sky130_fd_pr`, `sky130_fd_sc_hd`, `sky130_fd_sc_hdll`, `sky130_fd_sc_hs`, `sky130_fd_sc_hvl`, `sky130_fd_sc_lp`, `sky130_fd_sc_ls`, `sky130_fd_sc_ms`, `sky130_sram_macros` | 2.9 GB (+ `common` 6.6 MB per library) | 211 MB, 21 files (30 MB) | 4.7 GB |
| `gf180mcu-gds` | gf180mcuD: `gf180mcu_as_sc_mcu7t3v3`, `gf180mcu_fd_io`, `gf180mcu_fd_ip_sram`, `gf180mcu_fd_pr`, `gf180mcu_fd_sc_mcu7t5v0`, `gf180mcu_fd_sc_mcu9t5v0`, `gf180mcu_ocd_io`, `gf180mcu_ocd_ip_sram`, `gf180mcu_ocd_sc_rt12t3v3`, `gf180mcu_osu_sc_gp12t3v3`, `gf180mcu_osu_sc_gp9t3v3`, `gf180mcu_re_efuse` | 684 MB (`common` 170 MB) | 130 MB, 35 files (15 MB) | 2.0 GB |
| `ihp-gds` | ihp-sg13g2: `sg13g2_io`, `sg13g2_pr`, `sg13g2_sram`, `sg13g2_stdcell` | 456 MB (`common` 434 MB) | 90 MB, 33 files (6 MB) | 1.2 GB |
| `sky130-gds-sc` | `sky130_fd_sc_hd` (`sky130_fd_sc_hd.gds`) | 134 MB | 4.2 MB | 0.9 GB |
| `gf180mcu-gds-sc` | `gf180mcu_fd_sc_mcu7t5v0` (`gf180mcu_fd_sc_mcu7t5v0.gds`) | 350 MB | 1.5 MB | 1.4 GB |
| `ihp-gds-sc` | `sg13g2_stdcell` (`sg13g2_stdcell.gds`) | 441 MB | 0.6 MB | 1.2 GB |

- **Pins and root.** The pins are those of the `sky130`, `gf180mcu` and `ihp` sets. The GDS sets install under their own PDK root, `.cache/pdks/gds/` (`.cache/pdks/gds/ciel/<family>/versions/<pin>/`), never into the tech/.mag sets' version directories: `pdk install` skips a library whose `libs.ref/<lib>` directory exists, so a GDS install there would skip `sky130_fd_sc_hd` and the other `.mag` libraries, and a shared directory would also mix the two kinds of set in one CI cache entry. The `*-gds-sc` sets are subsets of the full sets and share their version directories; installing either first leaves the other verifiable.
- **Libraries left out.** `sky130_ml_xx_hd` and `gf180mcu_ocd_alpha_{large,misc,small}` have no `gds/` directory. `sky130_fd_pr_reram` exists only for sky130B (`pdk install sky130A` rejects it).
- **Disk.** `pdk install` decompresses each archive in full before filtering and keeps the archives of one command until it ends. `sky130-gds` therefore runs one `pdk install` per library: its largest step is `sky130_fd_sc_ls` (743 MB archive, 3.6 GB tar). `gf180mcu-gds` and `ihp-gds` run one command, so their large `common` archive is downloaded once. The peak column is the measured peak disk use of a fresh install (macOS, 2026-11-13), archives and tars included; a cold `sky130-gds` fetch takes about 13 minutes on a home connection. The cached column is the set compressed as `actions/cache` does (zstd `--long=30`).
- **Cache keys.** `pdk-<set>-<first 12 characters of the pin>-<first 16 characters of the digest>`: `pdk-sky130-gds-5740c4a81c42-ff704e64e85d0ee3`, `pdk-gf180mcu-gds-5740c4a81c42-227560121bb6ce03`, `pdk-ihp-gds-5e6d592e4002-cc04c5c56dc037ea`, `pdk-sky130-gds-sc-5740c4a81c42-8953c7159e79611b`, `pdk-gf180mcu-gds-sc-5740c4a81c42-50469c4cd42efa29`, `pdk-ihp-gds-sc-5e6d592e4002-fd0edbc877f78824`. `--cache-info` prints them with `-` replaced by `_` in the output name (`sky130_gds_key`, `sky130_gds_path`).
- **CI.** The `reference-data` job of `ci.yml` caches the three `*-gds-sc` sets, and the build legs restore and verify them for the three `MASK-RT-*` cases every golden leg replays. The full sets are fetched only by the jobs that run the other round trips and the GDS benchmark (`golden-capture.yml` `capture-gds-rt` and `d19-magic-timing`, `perf.yml`), each set under its own key; a `capture-gds-rt` shard restores only its PDK's tech and GDS sets (`scripts/gds_rt_shards.py`). Together the full sets keep about 430 MB, about 50 MB cached, far below the 4 GB cache budget of design D19, so every set is cached.
- **Tests.** `tests/unit/common/reference_data.hpp` gives the paths (`pinned_gds_library`, `pinned_gds`, and `kSky130FdScHdGds`, `kGf180mcu7t5v0Gds`, `kSg13g2StdcellGds` for the standard-cell files); a test whose set is missing skips.

```sh
python3 scripts/fetch_pdk.py sky130-gds-sc gf180mcu-gds-sc ihp-gds-sc --cli build/macos-debug/src/cli/hefesto-cli
python3 scripts/fetch_pdk.py sky130-gds gf180mcu-gds ihp-gds --cli build/macos-debug/src/cli/hefesto-cli
python3 scripts/fetch_pdk.py --print-digest sky130-gds
```

### Standard-cell inputs of the extraction cases (M7)

The M7 PDK golden cases (`EXTRACT-PDK-<lib>`, `NETLIST-PDK-<lib>`; design D21 and Open Question 4 (a) of `openspec/changes/m7-extraction`) extract every cell of one standard-cell library per PDK. They need no new data set:

| Library | Read from | Set | Cells | Default extract style | Tech format |
|---|---|---|---|---|---|
| `sky130_fd_sc_hd` | `.mag` (`sky130A/libs.ref/sky130_fd_sc_hd/mag/`) | `sky130` | 446 | `ngspice()` | 35 |
| `gf180mcu_fd_sc_mcu7t5v0` | `.mag` (`gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag/`) | `gf180mcu` | 229 | `ngspice()` | 35 |
| `sg13g2_stdcell` | GDS (`ihp-sg13g2/libs.ref/sg13g2_stdcell/gds/sg13g2_stdcell.gds`, `gds read`) | `ihp-gds-sc` | 84 | `ngspice()` | 35 |

- **Counts.** Checked on the pinned sets on 2026-11-21: 446 and 229 `.mag` files, and 84 structures in IHP-Open-PDK `5e6d592e`'s `sg13g2_stdcell.gds` (also 84 macros in the `ihp-lef` set's `sg13g2_stdcell.lef`). IHP ships no `.mag` standard cells; the 456 MB `ihp-gds` set is not used. The `sky130` and `gf180mcu` tree digests (`--print-digest`) reproduce their pins.
- **Extract defaults.** Each PDK's extract section (`<variant>-extract.tech`, included by the tech file) declares `style ngspice variants (),...` first, so the default style is `ngspice()`: Magic names each variant style by the style name followed by the variant (`extract/ExtTech.c:2087-2135`) and loads the first one. No PDK `.magicrc` selects another. Every tech file is `format 35`, at or above the format 34 at which the fringe-halo sideoverlap model is on by default (`extract-tech-section` "Fringe model default chosen from tech format").
- **Tests.** `tests/unit/common/reference_data.hpp` gives `pinned_stdcell_mag(lib)` (nullopt for `sg13g2_stdcell` or a missing set), `pinned_stdcell_gds(lib)` (the `*-gds-sc` file, nullopt when the set is missing) and the `PinnedStdcell` records `kSky130FdScHd`, `kGf180mcu7t5v0`, `kSg13g2Stdcell` with the counts and defaults above; `tests/unit/pdk/reference_pdks_test.cpp` checks them on a leg that has the sets.

### LEF data sets (M6)

The LEF/DEF tests on real PDK data (the PDK LEF read tests, the `LEFDEF-RT-<lib>` golden cases and LEFDEF-240; design D2 and Open Question 6 (a) of `openspec/changes/archive/2026-11-22-m6-lef-def-oasis`) read the tech LEF and the standard-cell LEF of the same three releases. They come in three more data sets, fetched only when named:

| Set | Subset (`<variant>/libs.ref/<lib>/…`) | Files | Downloaded | Kept (cached) | Peak disk |
|---|---|---|---|---|---|
| `sky130-lef` | `sky130A/libs.ref/sky130_fd_sc_hd/{lef,techlef}/` | `lef/sky130_fd_sc_hd.lef`, `lef/sky130_ef_sc_hd.lef`, `techlef/sky130_fd_sc_hd__{nom,min,max}.tlef`, `techlef/sky130_fd_sc_hd__rram__{nom,min,max}.tlef` | 134 MB (`common` 6.6 MB, `sky130_fd_sc_hd` 127 MB) | 2.0 MB, 8 files (0.17 MB) | 0.9 GB |
| `gf180mcu-lef` | `gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/{lef,techlef}/` | `lef/gf180mcu_fd_sc_mcu7t5v0.lef`, `techlef/gf180mcu_fd_sc_mcu7t5v0__{nom,min,max}.tlef` | 350 MB (`common` 170 MB, `gf180mcu_fd_sc_mcu7t5v0` 180 MB) | 0.8 MB, 4 files (0.08 MB) | 1.6 GB |
| `ihp-lef` | `ihp-sg13g2/libs.ref/sg13g2_stdcell/lef/` | `lef/sg13g2_tech.lef` (the tech LEF), `lef/sg13g2_stdcell.lef` | 441 MB (`common` 434 MB, `sg13g2_stdcell` 6.4 MB) | 0.2 MB, 2 files (0.03 MB) | 1.1 GB |

- **Root.** The LEF sets install under their own PDK root, `.cache/pdks/lef/` (`.cache/pdks/lef/ciel/<family>/versions/<pin>/`), for the same reason as the GDS sets: `pdk install` skips a library whose `libs.ref/<lib>` directory exists, and each set is its own CI cache entry.
- **Tech LEF.** sky130 and gf180mcu ship the tech LEF in the library's `techlef/` directory, one file per corner (sky130 also has the `rram` variants); the tests use `__nom.tlef`. IHP ships it in `lef/` as `sg13g2_tech.lef`.
- **Other libraries.** The other libraries' LEF files are listed in design D2 of `openspec/changes/archive/2026-11-22-m6-lef-def-oasis` (PDK row); no test uses them, so no set holds them.
- **Cache keys.** `pdk-sky130-lef-5740c4a81c42-b8ebb4f6dcfe84d1`, `pdk-gf180mcu-lef-5740c4a81c42-1b748b5dc37d40a6`, `pdk-ihp-lef-5e6d592e4002-fbc2d9eaf68f4d1c` (`--cache-info` output names `sky130_lef_key`, `sky130_lef_path`, and so on).
- **CI.** `FETCH_LEF_SETS` in the `env` of `ci.yml` and of `golden-capture.yml` is `"true"` (since M6 round 3): `ci.yml`'s `reference-data` job looks the three sets up, fetches the missing ones and saves them, and the build legs restore and verify them; `golden-capture.yml`'s `capture` job restores, fetches and saves them. `perf.yml`'s `lefdef-bench` restores `sky130-lef` by the same keys.
- **Tests.** `tests/unit/common/reference_data.hpp` gives `pinned_lef_library`, `pinned_tech_lef` and `pinned_cell_lef` for `kSky130FdScHdLef`, `kGf180mcu7t5v0Lef` and `kSg13g2StdcellLef`; a test whose set is missing skips.

```sh
python3 scripts/fetch_pdk.py sky130-lef gf180mcu-lef ihp-lef --cli build/macos-debug/src/cli/hefesto-cli
python3 scripts/fetch_pdk.py --print-digest ihp-lef
```

### xschem data sets (M8)

The schematic work of M8 (the SCHEM golden captures, the inventory `scripts/schem_inventory.py`, the PDK round-trip and netlist tests) uses the xschem trees of the same three releases (design D17 of `openspec/changes/m8-schematic`). They come in three more data sets, fetched only when named:

| Set | Trees | Downloaded | Kept |
|---|---|---|---|
| `sky130-xschem` | `sky130A/libs.tech/xschem` (`xschem_sky130` @ `2bfd7d65`, as installed by open_pdks) and `sky130A/libs.tech/netgen` (the netgen setup file of SCHEM-LVS-01) | 20 MB (`common` 6.6 MB, `sky130_fd_pr` 13.9 MB) | 6.3 MB, 955 files |
| `gf180mcu-xschem` | `gf180mcuD/libs.tech/xschem` (`gf180mcu_fd_pr` @ `5e4be872` `cells/xschem`, as installed) | 170 MB (`common`) | 0.5 MB, 269 files |
| `ihp-xschem` | `ihp-sg13g2/libs.tech/xschem` and `ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem` (IHP-Open-PDK @ `5e6d592e`) | 441 MB (`common` 434 MB, `sg13g2_stdcell` 6.4 MB) | 3.2 MB, 362 files |

- **Root.** The sets install under their own PDK root, `.cache/pdks/xschem/` (`.cache/pdks/xschem/ciel/<family>/versions/<pin>/`), for the reason given for the GDS sets. Each `pdk install` names one library because the command needs one; only the subset is kept (the `-lib` of `ihp-xschem` is `sg13g2_stdcell`, whose archive holds the standard-cell symbols).
- **Cache keys.** `pdk-sky130-xschem-5740c4a81c42-8a3b28a791383eb5`, `pdk-gf180mcu-xschem-5740c4a81c42-e70989764fe362e8`, `pdk-ihp-xschem-5e6d592e4002-a38b0850a131fbe4` (`--cache-info`: `sky130_xschem_key`, `sky130_xschem_path`, ...).
- **Inventory.** `scripts/schem_inventory.py` reads only these sets and writes `tests/data/schem/inventory.json` (design D18): the `devices/` symbols, Tcl commands and `$::` variables, `xschem` subcommands and multi-line subcircuit templates each PDK uses. After a pin change, rerun it and commit the result; the `schem-inventory` job of `ci.yml` runs `--check` on the cached sets and fails on any difference.
- **Tests.** `tests/unit/common/reference_data.hpp` gives the paths (`pinned_xschem(pdk)` for `libs.tech/xschem`, `pinned_xschem(kIhpSg13g2, kIhpStdcellXschemSymbols)` for IHP's standard-cell symbols); a test whose set is missing skips.

```sh
python3 scripts/fetch_pdk.py sky130-xschem gf180mcu-xschem ihp-xschem --cli build/macos-debug/src/cli/hefesto-cli
python3 scripts/fetch_pdk.py --print-digest ihp-xschem
```

### netgen and ngspice data sets (M9)

LVS and simulation (M9: the LVS NG-PDK cases, the SIM and SIMADV captures, the capture image's OSDI build) use the netgen setup files and the ngspice model trees of the same three releases (design D19 of `openspec/changes/m9-lvs-simulation`). They come in five more data sets, fetched only when named:

| Set | Trees | Downloaded | Kept |
|---|---|---|---|
| `gf180mcu-netgen` | `gf180mcuD/libs.tech/netgen` (`gf180mcuD_setup.tcl`, `setup.tcl`) | 170 MB (`common`) | 0.04 MB, 2 files |
| `ihp-netgen` | `ihp-sg13g2/libs.tech/netgen` (`ihp-sg13g2_setup.tcl`, Q-LVS-31) | 435 MB (`common` 434 MB, `sg13g2_pr` 0.2 MB) | 0.01 MB, 1 file |
| `sky130-ngspice` | `sky130A/libs.tech/combined` (the continuous models, Q-SIMADV-2), `sky130A/libs.tech/ngspice` and `sky130A/libs.ref/sky130_fd_pr/spice`, which both model trees `.include` | 20 MB (`common` 6.6 MB, `sky130_fd_pr` 13.9 MB) | 55 MB, 1065 files |
| `gf180mcu-ngspice` | `gf180mcuD/libs.tech/ngspice` | 170 MB (`common`) | 1.6 MB, 8 files |
| `ihp-ngspice` | `ihp-sg13g2/libs.tech/ngspice` and `ihp-sg13g2/libs.tech/verilog-a` (the Verilog-A sources of PSP103, `r3_cmc`, `mosvar`, `cap_cmom*`, compiled to OSDI by the capture image, Q-SIM-23) | 435 MB (`common` 434 MB, `sg13g2_pr` 0.2 MB) | 1.2 MB, 74 files |

The sky130 netgen setup file (`sky130A/libs.tech/netgen/sky130A_setup.tcl`) is in `sky130-xschem`; with the two netgen sets it makes the three setup files of the NG-PDK cases (Q-LVS-31).

- **Root.** The netgen sets install under `.cache/pdks/netgen/`, the ngspice sets under `.cache/pdks/ngspice/` (`.cache/pdks/<root>/ciel/<family>/versions/<pin>/`), for the reason given for the GDS sets. Each `pdk install` names the variant's smallest library (`gf180mcu_fd_pr`, `sg13g2_pr`, `sky130_fd_pr`), because the command needs one; only the subset is kept.
- **Cache keys.** `pdk-gf180mcu-netgen-5740c4a81c42-91b3dffaffe8abcb`, `pdk-ihp-netgen-5e6d592e4002-7f6cf90b9540018d`, `pdk-sky130-ngspice-5740c4a81c42-a4e109d0323a049a`, `pdk-gf180mcu-ngspice-5740c4a81c42-31ead4717ee91797`, `pdk-ihp-ngspice-5e6d592e4002-89ad3139194e79c8` (`--cache-info`: `gf180mcu_netgen_key`, `gf180mcu_netgen_path`, ...).
- **Tests.** `tests/unit/common/reference_data.hpp` gives the paths: `pinned_tool_file(kGf180mcuNetgenSetup)`, `kIhpNetgenSetup`, `kSky130NetgenSetup` (from `sky130-xschem`), `kSky130NgspiceLib`, `kSky130CombinedLib`, `kGf180mcuNgspiceLib`, `kIhpNgspiceMosLib` and `kIhpPsp103Source`; a test whose file is missing skips.

```sh
python3 scripts/fetch_pdk.py gf180mcu-netgen ihp-netgen sky130-ngspice gf180mcu-ngspice ihp-ngspice \
    --cli build/macos-debug/src/cli/hefesto-cli
python3 scripts/fetch_pdk.py --print-digest sky130-ngspice
```

Run some steps only by naming them; every step is a no-op once its data is present and verified:

```sh
python3 scripts/fetch_pdk.py gf180mcu ihp --cli build/macos-debug/src/cli/hefesto-cli
python3 scripts/fetch_pdk.py magic-ref
python3 scripts/fetch_pdk.py --print-digest gf180mcu   # tree digest of an installed set
python3 scripts/fetch_pdk.py --cache-info              # the CI cache keys and paths
```

The steps are `sky130` (alias `pdk`), `gf180mcu`, `ihp`, the six GDS sets, the three LEF sets, the three xschem sets and the five netgen and ngspice sets above and `magic-ref`; a bare run fetches `sky130`, `gf180mcu`, `ihp` and `magic-ref`. `--root DIR` installs into another PDK root (the GDS sets into `DIR/gds`, the LEF sets into `DIR/lef`, the xschem sets into `DIR/xschem`, the netgen and ngspice sets into `DIR/netgen` and `DIR/ngspice`). In CI each set is its own `actions/cache` entry, keyed `pdk-<set>-<first 12 characters of the pin>-<first 16 characters of the digest>` (for example `pdk-gf180mcu-5740c4a81c42-354c55418060a14c`; the M4 keys are `pdk-sky130-5740c4a81c42-8aedf059155765ef`, `pdk-gf180mcu-5740c4a81c42-354c55418060a14c` and `pdk-ihp-5e6d592e4002-e01cd6e2db86f2d0`), and Magic's checkout is keyed `magic-ref-<first 12 characters of the commit>`. The `reference-data` job of `ci.yml` fetches the sets missing from the cache once and saves them; the build legs restore all three and only verify them, and `golden-capture.yml` restores the same entries. Changing one set's pin or digest refetches that set alone.

The PDK network test (`hefesto_pdk_network_tests`, CTest label `network`) downloads about 6.4 MB from GitHub into a temporary root, once with the in-process HTTPS Fetcher and once with the `curl`-process fallback. Leave it out with `ctest -LE network`, or set `HEFESTO_SKIP_NETWORK_TESTS=1` to make it skip; CI runs it on one leg only. A transient download failure (PDK-FETCH-001) is retried three times, after 2 s, 4 s and 6 s; when GitHub cannot be reached at all (the host cannot be resolved or connected to) or keeps answering with an HTTP 5xx server error after the last attempt, the test skips instead of failing. Any other failure still fails it.

### Native PCell generators against the PDK scripts

The native PCell generators (`src/core/src/pcell/native/`, design D10 of `openspec/changes/m4-scripting`) must produce the same cell names and the same saved `.mag` files, except the `timestamp` lines, as the PDKs' Tcl generators (decision D-29). `scripts/pcell_equivalence.py` checks that over the parameter sweeps of `tests/data/pcell/sweeps/` (about 4,600 cases for sky130A, gf180mcuD and ihp-sg13g2): it runs each sweep twice with the same `hefesto-cli`, once natively (with the PDK's shared Tcl drawing procedures replaced by failing stubs, so a silent fallback to Tcl is reported) and once with `set ::hefesto::native_generators 0`, and compares the outputs. It needs the `sky130`, `gf180mcu` and `ihp` data sets.

```sh
python3 scripts/pcell_equivalence.py --cli build/macos-debug/src/cli/hefesto-cli            # every sweep
python3 scripts/pcell_equivalence.py sky130 --limit 50 --keep --cli build/macos-debug/src/cli/hefesto-cli
```

`--jobs N` sets how many `hefesto-cli` sessions run at a time (default: the CPU count; a full run takes about 7 minutes with 6 jobs on an M-series Mac), `--chunk N` the cases per session, `--work DIR` keeps the two runs' files (`native/` and `tcl/` per chunk, with `console.txt`). The exit status is 0 when every case agrees, 1 on a difference, 3 when a data set is missing. The console lines of the two runs are compared too and only reported: the framework prints its own diagnostics when the `gencell` command returns, so their order may differ.

## Simulation engine library (M9)

`libngspice-hefesto` is ngspice-47 (`a80f6e3e`) with Hefesto's patch series
(`third_party/ngspice/patches/`), built by a Hefesto-written CMake wrapper
(Q-SIM-7, Q-SIM-20). Only the simulation worker `hefesto-simd` loads it; the
editor, `hefesto-cli` and the core never link it, and
`scripts/check_engine_boundary.py` checks that in CI. It is off by default:

```sh
cmake --preset macos-debug -DHEFESTO_BUILD_NGSPICE=ON
cmake --build --preset macos-debug --target ngspice-hefesto
# or on its own, as .github/workflows/ngspice.yml and the capture image do:
cmake -S third_party/ngspice -B build/ngspice -G Ninja -DCMAKE_BUILD_TYPE=Release [-DHEFESTO_NGSPICE_EXECUTABLE=ON]
cmake --build build/ngspice -j4
cmake --build build/ngspice --target ngspice_hefesto_source_archive
```

It needs Python 3 and `patch`; the first configure downloads the pinned
`ngspice-47.tar.gz` (SHA-256 checked) and applies `patches/series`. Outputs:
`libngspice-hefesto`, `ngspice-manifest.json` (source, patches, configure line,
features, code models and licences; the build fails if a compiled file carries a
GPL notice) and, from the target `ngspice_hefesto_source_archive`, the source
archive the LGPL and MPL terms require. `third_party/ngspice/README.md` has the
details, including how to update the pin.

### Replacing the ngspice library

The library is replaceable by the user (`hefesto-licensing`; design D2 of
`openspec/changes/m9-lvs-simulation`):

1. Unpack the source archive shipped with the release
   (`ngspice-hefesto-<version>-src.tar.gz`: every compiled ngspice file as
   patched, all headers, the whole `src/osdi` tree, cppduals, the code-model
   inputs, the patches and the wrapper), or start from the pinned tarball and
   `third_party/ngspice/patches/`.
2. Make your change and build as above. Keep the exported C API (`ngSpice_*`,
   `ngGet_*`, `ngCM_Input_Path`) unchanged.
3. Put the new library into the install's `lib/` directory, replacing the
   installed files: the versioned file (`libngspice-hefesto.47.<level>.dylib`
   on macOS, `libngspice-hefesto.so.47.<level>` on Linux) and its links
   `libngspice-hefesto.47.dylib` / `libngspice-hefesto.so.47` (the name the
   adapter records) and `libngspice-hefesto.dylib` / `.so`. The worker
   `hefesto-simd` in `bin/` finds it through its install-relative run path
   (`$ORIGIN/../lib` on Linux, `@loader_path/../lib` on macOS).
4. macOS: sign the rebuilt library ad hoc, naming the versioned file, not a
   link: `codesign --force -s - lib/libngspice-hefesto.47.<level>.dylib`. Only `hefesto-simd`
   carries `com.apple.security.cs.disable-library-validation`, so a notarized
   Hefesto loads an ad-hoc-signed library in the worker and nowhere else.

The desktop linking (a replaceable library loaded only by the sandboxed
worker) goes through a legal review before the Desktop Beta (D-28, task 20.1).

### The simulation worker (`hefesto-simd`)

Simulations run in a separate, confined process (design D2 and D9 of
`openspec/changes/m9-lvs-simulation`; capability
`simulation-engine-embedding`). Three pieces, all in `src/simd/`:

| Target | Built | What |
|---|---|---|
| `hefesto-simd` | always on Linux and macOS | the worker executable: frame server on stdin/stdout, a heartbeat thread (a `beat` frame every 2 s that never waits for the engine), confinement, then loading the plugin; no ngspice symbol |
| `hefesto-ngspice-adapter` | with `HEFESTO_BUILD_NGSPICE=ON` | the engine plugin, a shared module that alone includes `sharedspice.h`, links `libngspice-hefesto` and exports the C ABI `hefesto_engine_v1` (`src/simd/abi/hefesto_engine_v1.h`) |
| `hefesto_simd_devhost` | with `HEFESTO_DEVELOPER_BUILD=ON` | the in-process developer host: the same plugin behind `sim::Engine` in the calling process, for debugging only |

```sh
cmake --preset macos-debug -DHEFESTO_BUILD_NGSPICE=ON
cmake --build --preset macos-debug --target hefesto-simd hefesto-ngspice-adapter hefesto_platform_tests
build/macos-debug/tests/unit/platform/hefesto_platform_tests
```

Install layout: `bin/hefesto-simd`, `lib/hefesto-ngspice-adapter.{so,dylib}`,
`lib/libngspice-hefesto.*`. The worker looks for the plugin in `<bin>/../lib`
and then next to itself (the build tree puts both in `src/simd/`); the plugin
finds `libngspice-hefesto` through its run path (`$ORIGIN`, `@loader_path`).
`hefesto-simd --adapter=<path>` loads another plugin (the tests load the
stand-in `tests/unit/platform/fake_adapter.cpp`); `--heartbeat-ms=<n>` lowers
the heartbeat interval. The core starts the worker through
`platform::WorkerHost` (`src/cli/worker_host_posix.hpp` on Linux and macOS),
which adds `--job-dir=<dir>` and one `--readonly-root=<dir>` per read-only
root, runs it in the job directory and gives it an empty environment.

Confinement is applied before the plugin is loaded:

- macOS: a `sandbox_init` profile that denies the network, sockets, process
  creation, and writes outside the job directory (the read-only roots stay
  read-only even inside it). The build signs `hefesto-simd` ad hoc with
  `src/simd/hefesto-simd.entitlements`
  (`com.apple.security.cs.disable-library-validation`; no other binary has it).
- Linux: `PR_SET_NO_NEW_PRIVS`, a seccomp filter (sockets other than
  `AF_UNIX`, `fork`, `vfork`, `clone` without `CLONE_THREAD`, `clone3`,
  `execve`, `execveat` are refused), and a Landlock ruleset that allows writes
  only under the job directory. Without Landlock (an older kernel) the worker
  runs with the seccomp filter alone and says so in the job log.

The worker reports what it applied as job-log lines starting with
`confinement:`. A release tree is checked with
`python3 scripts/check_engine_boundary.py --build-dir build/<preset>`: no
binary other than the plugin and the library has an `ngSpice_` symbol, and no
release binary other than `hefesto-simd` names the plugin or its entry point
`hefesto_engine_v1`. A tree whose `CMakeCache.txt` sets
`HEFESTO_DEVELOPER_BUILD=ON` is checked as a developer build (the plugin rule
is off); `--developer-build` on a release tree is refused. CI's
`engine-boundary-release` leg builds a release tree with the plugin and runs
the check and the worker tests.

## Golden capture

Golden references are captured with the pinned reference tools in the capture image (`tools/golden/container/Containerfile`, pins in `tools/golden/pins.toml`), normally in CI through `golden-capture.yml`; `tools/golden/README.md` also documents local container runs (and the `local` and `model` runners of the hash harness) and is the full guide; the image holds:

- Magic 8.3.694, netgen, ngspice and xschem, built from pinned commits;
- since M6, KLayout 0.30.0 (Debian package `klayout 0.30.0-1+b1` from the APT snapshot, run as `klayout -b`) and gdstk 1.0.2 (a pinned wheel in `$HEFESTO_GDSTK_PYTHON`), reference tools of the OASIS cases only (design D18 of `openspec/changes/archive/2026-11-22-m6-lef-def-oasis`). Hefesto never links or calls them.

The M6 dispatch modes of `golden-capture.yml`:

```sh
gh workflow run golden-capture.yml --ref <branch> -f mode=ext-inputs       # Magic's .ext files (tools/golden/ext_inputs.py)
python3 tools/golden/authoring/lefdef_cases.py --sync-ext <artifact dir>   # commit them under tests/data/extflat/
gh workflow run golden-capture.yml --ref <branch> -f mode=oasis-validate   # KLayout and gdstk checks (tools/golden/oasis_validate.py)
```

## Dependencies and offline rebuilds

All third-party dependencies are pinned in `cmake/Dependencies.cmake` (zlib, nlohmann/json, the Zstandard decoder, libcurl on Linux, Catch2, and Raven for the desktop build) and `cmake/Tcl.cmake` (Tcl 8.6.16, by URL and SHA-256, with the fixes in `third_party/patches/tcl/` applied on top by `cmake/ApplyPatches.cmake`), and fetched with CMake FetchContent into `.cache/deps/<preset>`. Tcl is compiled by CMake with the preset's own toolchain and flags (no `configure` run; the platform settings are in `cmake/tcl/tclConfig.h`), statically, without Tk and without dynamic loading, and its `library/` scripts are embedded in the binary, so no Tcl file is read from the host. CI's dependency caches are keyed on both files and on the patches. Deleting `build/` keeps that cache. To rebuild without network access once the cache is populated:

```sh
cmake --preset macos-debug -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build --preset macos-debug
```

## Source layout

| Path | Contents |
|---|---|
| `src/core/` | `hefesto_core`: layout database, algorithms, command API, diagnostics; no UI, GPU or process APIs |
| `src/cli/` | `hefesto-cli`, the headless tool used by batch flows and golden tests |
| `src/platform_posix/` | `hefesto_platform_posix`: POSIX host services the core must not contain (file locks, crash-backup file host, process runner); linked by the CLI and the desktop on Linux and macOS |
| `src/platform_net/` | `hefesto_platform_net`: the in-process HTTPS Fetcher of the PDK manager (URLSession bridge in Objective-C++ on macOS, libcurl on Linux; hefesto-licensing D-13); linked by the CLI and the desktop |
| `src/simd/` | `hefesto-simd`, the confined simulation worker, and the `hefesto-ngspice-adapter` plugin (the only code that includes ngspice's API header); see [The simulation worker](#the-simulation-worker-hefesto-simd) |
| `src/render/vulkan/` | `hefesto_render_vk`, the Vulkan canvas renderer (desktop) |
| `src/shell/desktop/` | `hefesto_shell_desktop` and the `hefesto` app; the only code that uses Raven |
| `tests/unit/` | Catch2 unit tests, tagged with the capability and requirement they cover |
| `tests/fuzz/` | libFuzzer targets (one per external-input reader) and their seed corpora in `corpus/<target>/` |
| `tests/golden/` | golden-case inventories, captured cases and the documented deviations (`deviations.toml`) |
| `tools/golden/` | reference-tool container, capture runner and comparators |
| `bench/canvas/` | canvas benchmark |

Every first-party source file starts with the SPDX header:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
```

(`#` instead of `//` in CMake and Python files.)

Test labels, traceability tags, the golden suite, fuzzing and the CI checks are described in [`docs/contributing/README.md`](contributing/README.md).

## Schematic capture (M8)

The schematic editor and netlister (`src/core/src/schem/`, capabilities `schematic-*`) need no extra build option. Three things are specific to it:

- **xschem data sets.** The PDK schematic tests, the PDK netlist digests (`tests/unit/schem/net/pdk_digest_test.cpp`, design D20 of `openspec/changes/m8-schematic`) and the SCHEM golden replay read the three xschem sets: `python3 scripts/fetch_pdk.py sky130-xschem gf180mcu-xschem ihp-xschem --cli build/<preset>/src/cli/hefesto-cli` (see "xschem data sets (M8)" above). Tests whose set is missing skip with a message; in CI the sets are restored on `linux-x64-gcc-11`, `linux-arm64-gcc` and `macos-arm64-xcode-oldest`, so the digests are checked on the three platforms. After an intended netlist change, regenerate the digest table with `HEFESTO_WRITE_PDK_DIGESTS=1 build/<preset>/tests/unit/schem/net/hefesto_schem_net_tests "PDK example netlists have the committed digests"` (all three sets fetched) and review the diff of `tests/unit/schem/net/pdk_netlist_digests.txt`.
- **`CAD_ROOT` and the system library.** The clean-room system symbol library is `assets/schematic/xschem_library/devices/`, found as `${XSCHEM_SHAREDIR}/xschem_library/devices` with `XSCHEM_SHAREDIR` = `<CAD_ROOT>/assets/schematic`. When `CAD_ROOT` is not set, `hefesto-cli` and the desktop app use their install root (Q-SCHEM-45), so an installed Hefesto finds the library without it; a build tree sets `CAD_ROOT` to the source tree (or the install prefix) to use the library from there. The golden replay copies `assets/schematic` into its own `CAD_ROOT` for every SCHEM case.
- **Benchmark.** `-DHEFESTO_BUILD_SCHEM_BENCH=ON` builds `bench/schem/hefesto-bench-schem` (core only); `python3 bench/schem/schem_bench.py --bench build/<preset>/bench/schem/hefesto-bench-schem --host <name>` runs the load, save, netlist and flat workloads (a generated 10,000-instance hierarchy and the largest PDK example schematics) against `bench/schem/baseline.json`. The `schem-bench` job of `.github/workflows/perf.yml` runs it on a Release build.
