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
| `--diagnostic-codes[=AREA]` | print a diagnostic code table (`core`, `tech`, `db`, `cell`, `display`, `view`, `pdk`, `app`, `undo`, `select`, `connect`, `ui`, `wiring`, `drc`, `script`, `pcell`, `tools`, `gui`, `hints`) |
| `--log-dir=<dir>`, `--no-log` | session log location, or none |

Example: run the sky130A rc file of the pinned data set and a Tcl script that places a generated transistor.

```sh
V=.cache/pdks/ciel/sky130/versions/5740c4a81c42f9fe4af5ef2ee258a99151053d28
build/macos-debug/src/cli/hefesto-cli --no-log --trust-pdk sky130A --pdk-root=$V \
  --magic -dnull -noconsole -rcfile $V/sky130A/libs.tech/magic/sky130A.magicrc place_nfet.tcl
# place_nfet.tcl:  load top -silent; box 0 0 0 0; magic::gencell sky130::sky130_fd_pr__nfet_01v8 nf1 w 1.0 l 0.15; writeall force; quit -noprompt
```

Outside `--magic` mode, `-c`, script files, standard input and `--interactive` are Tcl too (M4), read like a Tcl console: one complete command at a time in the project's sandboxed interpreter, and each command's non-empty result is printed (in `--magic` mode, as in Magic's batch mode, only `puts` prints). A Tcl error stops that source at its line, and a command that fails quietly still makes the run exit 1. For example `-c 'tech name'` prints the technology's name (the command's result), and `-c 'puts [tech name]'` prints it once (the `puts`; its empty result prints nothing). The golden replays run each case's `commands.tcl` with `--magic -dnull -noconsole -norc`, so no user `.magicrc` takes part.

**Scripts and trust.** Tcl runs in a sandbox (capabilities `hefesto-scripting`, `hefesto-security`): no `exec`, `socket`, `load`, `exit` or command pipelines, and the engine command `shell` is refused from every origin, typed lines included (`SCRIPT-SANDBOX-001`; Hefesto runs no external programs); file access only under the granted roots (the project folder, the PDK store, the startup files and the `.tcl` script named on the command line, the working directory and `CAD_ROOT`); a synthesized `env` with `PDK_ROOT`, `PDK` and `CAD_ROOT`. A PDK's scripts run only with consent: the desktop will ask (Don't run, Run once, Trust this version; the dialog lands with the M4 desktop screens); trusting a version records it in the PDK store (`Provenance::trusted`), so later sessions, batch ones included, run it without asking.

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
| `--no-vsync`, `--validation` | presentation without vsync; Vulkan validation layers |
| `--rects N`, `--seed N`, `--bench-frames N`, `--bench-output FILE`, `--smoke-frames N`, `--expect-window-system` | benchmark and smoke-test modes (`docs/spikes/canvas-spike.md`, CI window tests) |

**Canvas input (M2).** Pointer buttons and keys on the canvas go through Magic's macro tables of the current tool, as in Magic: with the box tool, the left button moves the box by its lower-left corner, the right button resizes it by its upper-right corner and the middle button paints by example; keys follow Magic's default bindings (for example `f` is `sideways` and `v` is `view`; `:` opens the command bar). `tool` (or the tool rail) switches between the box, wiring and pick tools. The view moves directly: Option-drag (Alt-drag on Linux) pans, the wheel and pinch zoom about the pointer, and two-finger trackpad scroll pans; the middle and right drags no longer pan, because Magic gives those buttons to the box tool. Shell commands added in M2: `panel layers|inspector|console show|hide|toggle` collapses and restores a panel, and `palette paint LAYER` sets the palette's paint layer. Key bindings can be changed with `macro` and `imacro`; a file of `macro` lines can be run with `source FILE`.

**Key-binding files (M2).** A key-binding file is a text file of Magic `macro` / `imacro` command lines, for example `macro ^Z "undo"` or `macro XK_F5 "zoom 2"`. Key names are X11 keysym names on every platform (the keysym table is built in, so `XK_*` names work on macOS too); `^X` is Control-X. Load one with `source FILE` in the command bar or a script, or put its lines in a `.magicrc` read at startup. `macro` lists the current tool's bindings; the desktop Keys tab (`panel keys show`) shows the same tables.

**Command names (M2).** Scripts, `-c` lines, macros and `source` files need exact command names, as Magic's Tcl build does: `vers` gives `invalid command name "vers"` (decision Q-UI-41). Lines typed in the desktop command bar still accept Magic's unique abbreviations (`vers` runs `version`). In the command bar, GNU Readline history expansion applies (`!!`, `!n`, `!string`, `^old^new^`, `:p`); `history` lists the typed lines.

**Undo.** Every editing command is undoable (`undo [N]`, `redo [N]`, the `u`/`U` keys, the Undo and Redo buttons; right-click either button for the undo history). `undo disable` stops recording (the top bar shows "undo off"; click it to issue `undo enable`). The number of steps kept is the setting `undo-history-limit` (default and minimum 1000, Magic's limit), e.g. `--setting undo-history-limit=5000` or in a settings file.

**Desktop panels (M2).** `panel layers|inspector|console|keys show|hide|toggle`, `panel layers|inspector dock left|right`, `panel reset`; F6 / Shift-F6 move the keyboard focus between the tool rail, Layers, canvas, Inspector, Console and command bar. `specialopen color [INDEX]` shows Magic's color window in the right dock.

**Design rule checking (M3).** Both front ends run Magic's `drc` command set (`drc check`, `catchup`, `count`, `find`, `why`, `listall`, `ignore`, `style`, `euclidean`, `printrules`, and the rest). The DRC settings are session state set by commands, as in Magic: `drc on|off` (the background checker; on by default), `drc style NAME` (the technology's DRC styles, e.g. `drc(fast)` and `drc(full)` in sky130A), `drc euclidean on|off`, `drc *halo N` and `drc *stepsize N` (a positive integer; 0, negative and non-numeric values are refused with `Step size must be a positive integer.`, decision Q-DRC-24). The background checker runs in the desktop app and in `hefesto-cli --interactive`; scripts, `-c` and piped input are batch and check only on `drc check; drc catchup`. In the desktop app the top-bar chip and the status bar show the DRC state (not run, checking, clean, N errors, off; N is `drc list count total`), and `panel drc show|hide|toggle` (or a click on the chip) opens the DRC panel: errors grouped by why-text, `#k of n` stepping (`drc find`), Re-check (`select top cell; drc check; drc catchup`), ignored rules and the style selector, each a logged command. When a long `drc catchup` ends while the window is not focused, the app requests the Dock's attention and posts a message in the window; `hefesto::notifications off` turns that off (setting `notifications`, on by default).

The desktop installs the same host services as `hefesto-cli` (`src/platform_posix`: POSIX file locks, the crash-backup file host, the process runner; `src/platform_net`: the HTTPS Fetcher): a session holds a lock on its crash-backup file, `crashbackups` saves periodically, SIGTERM and SIGHUP save a backup before exiting, a normal exit removes it, and `-r` (or `crash recover`) restores the newest backup whose session is gone.

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
# After building hefesto-cli (any preset): every PDK data set, then Magic's sources.
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
- `magic-ref`: a sparse, shallow checkout of Magic at `fc5daa9e` into `build/ref/magic` with only `scmos/` and `graphics/` (display styles, color maps, glyphs, the `.pt3` fonts, the scmos tech sources; about 4 MB). A full clone (`git clone https://github.com/RTimothyEdwards/magic build/ref/magic && git -C build/ref/magic checkout fc5daa9e`) works too and is what reading Magic's sources for porting needs.
- `scmos.tech` is generated from `build/ref/magic/scmos` at configure time (`tests/data/tech/generate_scmos.cmake`, run by `tests/unit/tech/CMakeLists.txt`), so fetch `magic-ref` before configuring, or reconfigure afterwards.
- Golden replay (`ctest -L golden`, `tools/golden/replay.py`) takes the outline fonts that `flabel` cases need from `build/ref/magic/graphics/` (`magic-sys` external inputs) and runs `hefesto-cli` with its own `CAD_ROOT`, so a `CAD_ROOT` set in your shell does not change the results; cases whose data is missing are skipped.

Run some steps only by naming them; every step is a no-op once its data is present and verified:

```sh
python3 scripts/fetch_pdk.py gf180mcu ihp --cli build/macos-debug/src/cli/hefesto-cli
python3 scripts/fetch_pdk.py magic-ref
python3 scripts/fetch_pdk.py --print-digest gf180mcu   # tree digest of an installed set
python3 scripts/fetch_pdk.py --cache-info              # the CI cache keys and paths
```

The steps are `sky130` (alias `pdk`), `gf180mcu`, `ihp` and `magic-ref`; `--root DIR` installs into another PDK root. In CI each set is its own `actions/cache` entry, keyed `pdk-<set>-<first 12 characters of the pin>-<first 16 characters of the digest>` (for example `pdk-gf180mcu-5740c4a81c42-354c55418060a14c`; the M4 keys are `pdk-sky130-5740c4a81c42-8aedf059155765ef`, `pdk-gf180mcu-5740c4a81c42-354c55418060a14c` and `pdk-ihp-5e6d592e4002-e01cd6e2db86f2d0`), and Magic's checkout is keyed `magic-ref-<first 12 characters of the commit>`. The `reference-data` job of `ci.yml` fetches the sets missing from the cache once and saves them; the build legs restore all three and only verify them, and `golden-capture.yml` restores the same entries. Changing one set's pin or digest refetches that set alone.

The PDK network test (`hefesto_pdk_network_tests`, CTest label `network`) downloads about 6.4 MB from GitHub into a temporary root, once with the in-process HTTPS Fetcher and once with the `curl`-process fallback. Leave it out with `ctest -LE network`, or set `HEFESTO_SKIP_NETWORK_TESTS=1` to make it skip; CI runs it on one leg only. A transient download failure (PDK-FETCH-001) is retried three times, after 2 s, 4 s and 6 s; when GitHub cannot be reached at all (the host cannot be resolved or connected to) or keeps answering with an HTTP 5xx server error after the last attempt, the test skips instead of failing. Any other failure still fails it.

### Native PCell generators against the PDK scripts

The native PCell generators (`src/core/src/pcell/native/`, design D10 of `openspec/changes/m4-scripting`) must produce the same cell names and the same saved `.mag` files, except the `timestamp` lines, as the PDKs' Tcl generators (decision D-29). `scripts/pcell_equivalence.py` checks that over the parameter sweeps of `tests/data/pcell/sweeps/` (about 4,600 cases for sky130A, gf180mcuD and ihp-sg13g2): it runs each sweep twice with the same `hefesto-cli`, once natively (with the PDK's shared Tcl drawing procedures replaced by failing stubs, so a silent fallback to Tcl is reported) and once with `set ::hefesto::native_generators 0`, and compares the outputs. It needs the `sky130`, `gf180mcu` and `ihp` data sets.

```sh
python3 scripts/pcell_equivalence.py --cli build/macos-debug/src/cli/hefesto-cli            # every sweep
python3 scripts/pcell_equivalence.py sky130 --limit 50 --keep --cli build/macos-debug/src/cli/hefesto-cli
```

`--jobs N` sets how many `hefesto-cli` sessions run at a time (default: the CPU count; a full run takes about 7 minutes with 6 jobs on an M-series Mac), `--chunk N` the cases per session, `--work DIR` keeps the two runs' files (`native/` and `tcl/` per chunk, with `console.txt`). The exit status is 0 when every case agrees, 1 on a difference, 3 when a data set is missing. The console lines of the two runs are compared too and only reported: the framework prints its own diagnostics when the `gencell` command returns, so their order may differ.

## Dependencies and offline rebuilds

All third-party dependencies are pinned in `cmake/Dependencies.cmake` (zlib, nlohmann/json, the Zstandard decoder, libcurl on Linux, Catch2, and Raven for the desktop build) and `cmake/Tcl.cmake` (Tcl 8.6.16, by URL and SHA-256), and fetched with CMake FetchContent into `.cache/deps/<preset>`. Tcl is compiled by CMake with the preset's own toolchain and flags (no `configure` run; the platform settings are in `cmake/tcl/tclConfig.h`), statically, without Tk and without dynamic loading, and its `library/` scripts are embedded in the binary, so no Tcl file is read from the host. CI's dependency caches are keyed on both files. Deleting `build/` keeps that cache. To rebuild without network access once the cache is populated:

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
