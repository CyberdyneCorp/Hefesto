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

Both front ends run the same Magic-compatible startup (`app::run_startup`, capability `app-startup`): settings layers, startup files (located and reported; executed only once the Tcl binding exists, M4), the technology, then the layout files named on the command line, then the box. `CAD_ROOT` (environment) is the Magic installation root: its `magic/sys` directories are searched for technology, display-style and font files (FreeSans/FreeSerif/FreeMono `.pt3`, needed for `flabel` labels); without it there are no system files and the session starts on the built-in `minimum` technology.

### `hefesto-cli`

| Option | Meaning |
|---|---|
| `-T <tech>` | technology to load at startup (a name resolved through the PDK store, or a `.tech` path) |
| `<cell>.mag`, `.lef`, `.def`, `.cif`, `.gds` | layout files loaded at startup, in command-line order; any other word is a script |
| `--project=<dir>` | project folder (default: `.` when it holds `hefesto.project`); its PDK pin selects the technology |
| `--pdk-root=<dir>` | PDK root (default: setting `pdk-root`, then `$PDK_ROOT`, then `~/.ciel`); the session's PDK store resolves through the PDK manager's installs there |
| `--setting <name>=<value>` | a session setting (the highest settings layer); e.g. `undo-history-limit=2000` (undo steps kept, at least Magic's 1000) |
| `--interactive` | after the scripts, read commands from standard input at a prompt (`%` idle, `]` while background DRC is busy; the background DRC worker runs only here); implied by `--magic` on a terminal without `-c` or scripts; give it before `--magic` |
| `--magic ...` | the rest is Magic's own command line (`-T`, `-d`, `-rcfile`, `-norcfile`, `-r`, cells, a `.tcl` script); commands are then read from standard input (at the interactive prompt when it is a terminal) |
| `-c <commands>` | run commands, e.g. `-c "tech load sky130A; tech layers"` |
| `--diagnostics=text\|json\|magic` | diagnostic format on stderr; `magic` prints Magic's console text and each script command's result, as the golden reference scripts do (see `tools/golden/README.md`) |
| `--diagnostic-codes[=AREA]` | print a diagnostic code table (`core`, `tech`, `db`, `cell`, `display`, `view`, `pdk`, `app`, `undo`, `select`, `connect`, `ui`, `wiring`) |
| `--log-dir=<dir>`, `--no-log` | session log location, or none |

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
| `--no-vsync`, `--validation` | presentation without vsync; Vulkan validation layers |
| `--rects N`, `--seed N`, `--bench-frames N`, `--bench-output FILE`, `--smoke-frames N`, `--expect-window-system` | benchmark and smoke-test modes (`docs/spikes/canvas-spike.md`, CI window tests) |

**Canvas input (M2).** Pointer buttons and keys on the canvas go through Magic's macro tables of the current tool, as in Magic: with the box tool, the left button moves the box by its lower-left corner, the right button resizes it by its upper-right corner and the middle button paints by example; keys follow Magic's default bindings (for example `f` is `sideways` and `v` is `view`; `:` opens the command bar). `tool` (or the tool rail) switches between the box, wiring and pick tools. The view moves directly: Option-drag (Alt-drag on Linux) pans, the wheel and pinch zoom about the pointer, and two-finger trackpad scroll pans; the middle and right drags no longer pan, because Magic gives those buttons to the box tool. Shell commands added in M2: `panel layers|inspector|console show|hide|toggle` collapses and restores a panel, and `palette paint LAYER` sets the palette's paint layer. Key bindings can be changed with `macro` and `imacro`; a file of `macro` lines can be run with `source FILE`.

**Key-binding files (M2).** A key-binding file is a text file of Magic `macro` / `imacro` command lines, for example `macro ^Z "undo"` or `macro XK_F5 "zoom 2"`. Key names are X11 keysym names on every platform (the keysym table is built in, so `XK_*` names work on macOS too); `^X` is Control-X. Load one with `source FILE` in the command bar or a script, or put its lines in a `.magicrc` read at startup. `macro` lists the current tool's bindings; the desktop Keys tab (`panel keys show`) shows the same tables.

**Command names (M2).** Scripts, `-c` lines, macros and `source` files need exact command names, as Magic's Tcl build does: `vers` gives `invalid command name "vers"` (decision Q-UI-41). Lines typed in the desktop command bar still accept Magic's unique abbreviations (`vers` runs `version`). In the command bar, GNU Readline history expansion applies (`!!`, `!n`, `!string`, `^old^new^`, `:p`); `history` lists the typed lines.

**Undo.** Every editing command is undoable (`undo [N]`, `redo [N]`, the `u`/`U` keys, the Undo and Redo buttons; right-click either button for the undo history). `undo disable` stops recording (the top bar shows "undo off"; click it to issue `undo enable`). The number of steps kept is the setting `undo-history-limit` (default and minimum 1000, Magic's limit), e.g. `--setting undo-history-limit=5000` or in a settings file.

**Desktop panels (M2).** `panel layers|inspector|console|keys show|hide|toggle`, `panel layers|inspector dock left|right`, `panel reset`; F6 / Shift-F6 move the keyboard focus between the tool rail, Layers, canvas, Inspector, Console and command bar. `specialopen color [INDEX]` shows Magic's color window in the right dock.

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

The PDK and Magic-reference tests use pinned data that is fetched, never committed (design D8/D9 of `openspec/changes/m1-core-viewer`, D15 of `openspec/changes/m3-drc`). Without it they skip cleanly.

```sh
# After building hefesto-cli (any preset): every PDK data set, then Magic's sources.
python3 scripts/fetch_pdk.py --cli build/macos-debug/src/cli/hefesto-cli
```

Each PDK data set is a subset of one ciel release, installed into `.cache/pdks/ciel/<family>/versions/<pin>/` by `hefesto-cli`'s own `pdk install … -only …` and verified against a pinned tree digest (SHA-256 over the sorted `path NUL sha256 LF` records of the subset's files). Tests use only these exact versions (`tests/unit/common/reference_data.hpp`), never another installed one.

| Set | Release (pin) | Subset kept | Downloaded | Kept |
|---|---|---|---|---|
| `sky130` | `sky130-0fe599b2afb6708d281543108caf8310912f54af` (open_pdks) | `sky130A/libs.tech/magic/`, `sky130A/libs.ref/sky130_fd_sc_hd/mag/` | 134 MB | 6.5 MB, 485 files |
| `gf180mcu` | `gf180mcu-0fe599b2afb6708d281543108caf8310912f54af` (open_pdks) | `gf180mcuD/libs.tech/magic/`, `gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag/` | 569 MB (`common` 389 MB, `gf180mcu_fd_sc_mcu7t5v0` 180 MB) | 3.0 MB, 235 files (229 cells) |
| `ihp` | `ihp-sg13g2-ddb601a4a4473163e1ed6df416b885df18b4ac03` (IHP-Open-PDK) | `ihp-sg13g2/libs.tech/magic/` | 332 MB (`common` 332 MB, `sg13g2_pr` 0.2 MB) | 0.4 MB, 9 files |

- A cold cache downloads about 1.0 GB in all (0.9 GB for `gf180mcu` and `ihp`). `pdk install` keeps no archive after the install, but it decompresses each archive in full before filtering, so the `gf180mcu` install needs about 5.3 GB of free disk space for a moment (`ihp` about 0.9 GB).
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

The steps are `sky130` (alias `pdk`), `gf180mcu`, `ihp` and `magic-ref`; `--root DIR` installs into another PDK root. In CI each set is its own `actions/cache` entry, keyed `pdk-<set>-<first 12 characters of the pin>-<first 16 characters of the digest>` (for example `pdk-gf180mcu-0fe599b2afb6-eb33076819a8e4b2`), and Magic's checkout is keyed `magic-ref-<first 12 characters of the commit>`. The `reference-data` job of `ci.yml` fetches the sets missing from the cache once and saves them; the build legs restore all three and only verify them, and `golden-capture.yml` restores the same entries. Changing one set's pin or digest refetches that set alone.

The PDK network test (`hefesto_pdk_network_tests`, CTest label `network`) downloads about 6.4 MB from GitHub into a temporary root, once with the in-process HTTPS Fetcher and once with the `curl`-process fallback. Leave it out with `ctest -LE network`, or set `HEFESTO_SKIP_NETWORK_TESTS=1` to make it skip; CI runs it on one leg only. A transient download failure (PDK-FETCH-001) is retried three times, after 2 s, 4 s and 6 s; when GitHub cannot be reached at all (the host cannot be resolved or connected to) or keeps answering with an HTTP 5xx server error after the last attempt, the test skips instead of failing. Any other failure still fails it.

## Dependencies and offline rebuilds

All third-party dependencies are pinned in `cmake/Dependencies.cmake` (zlib, nlohmann/json, the Zstandard decoder, libcurl on Linux, Catch2, and Raven for the desktop build) and fetched with CMake FetchContent into `.cache/deps/<preset>`. Deleting `build/` keeps that cache. To rebuild without network access once the cache is populated:

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
