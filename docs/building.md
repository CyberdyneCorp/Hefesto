# Building Hefesto

Hefesto builds with CMake 3.25 or later and a C++20 compiler: GCC 11+, Clang 14+ or Apple Clang 15+ (capability `hefesto-platform-targets`). Builds are out of tree, under `build/<preset>`.

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

- **Linux:** `libvulkan-dev` and `libx11-dev` (plus `libwayland-dev`, `libxkbcommon-dev` and `libdecor-0-dev` for the Wayland backend). Without a GPU, `mesa-vulkan-drivers` (lavapipe) lets the GPU readback tests run instead of being skipped.
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
| `--setting <name>=<value>` | a session setting (the highest settings layer) |
| `--magic ...` | the rest is Magic's own command line (`-T`, `-d`, `-rcfile`, `-norcfile`, `-r`, cells, a `.tcl` script); commands are then read from standard input |
| `-c <commands>` | run commands, e.g. `-c "tech load sky130A; tech layers"` |
| `--diagnostics=text\|json\|magic` | diagnostic format on stderr; `magic` prints Magic's console text and each script command's result, as the golden reference scripts do (see `tools/golden/README.md`) |
| `--diagnostic-codes[=AREA]` | print a diagnostic code table (`core`, `tech`, `db`, `cell`, `display`, `view`, `pdk`, `app`) |
| `--log-dir=<dir>`, `--no-log` | session log location, or none |

### `hefesto` (desktop)

`hefesto [-T tech] [-r] [-norcfile] [cell.mag ...]` takes Magic's startup words (`-T`, `-r` crash recovery, `-norcfile`, cells), plus:

| Option | Meaning |
|---|---|
| `--asset-dir DIR` | where the bundled UI fonts and icons are read (default: the source tree's `assets/`) |
| `--theme dark\|light` | the UI theme (dark by default; the canvas stays dark in both) |
| `--width PX`, `--height PX` | the initial window size |
| `--console` | open the Console panel (command output) at startup |
| `--pdk-manager` | open the PDK manager panel at startup |
| `--capture FILE.ppm [--capture-frames N]` | render N frames (default 1), write the window to a PPM file and exit (screenshot checks of `docs/design/m1-ui-conformance.md`) |
| `--no-vsync`, `--validation` | presentation without vsync; Vulkan validation layers |
| `--rects N`, `--seed N`, `--bench-frames N`, `--bench-output FILE`, `--smoke-frames N`, `--expect-window-system` | benchmark and smoke-test modes (`docs/spikes/canvas-spike.md`, CI window tests) |

## Reference data for tests

The PDK and Magic-reference tests use pinned data that is fetched, never committed (design D8/D9 of `openspec/changes/m1-core-viewer`). Without it they skip cleanly.

```sh
# After building hefesto-cli (any preset):
python3 scripts/fetch_pdk.py --cli build/macos-debug/src/cli/hefesto-cli
```

- `pdk`: the sky130A test subset of open_pdks at ciel version `0fe599b2afb6708d281543108caf8310912f54af` (`libs.tech/magic/` and `libs.ref/sky130_fd_sc_hd/mag/`), installed into `.cache/pdks/ciel/sky130/versions/<pin>/` by `hefesto-cli`'s own `pdk install` (about 134 MB downloaded, 6.5 MB kept) and verified against a pinned tree digest. Tests use only this exact version (`tests/unit/common/reference_data.hpp`), never another installed one.
- `magic-ref`: a sparse, shallow checkout of Magic at `fc5daa9e` into `build/ref/magic` with only `scmos/` and `graphics/` (display styles, color maps, glyphs, the `.pt3` fonts, the scmos tech sources; about 4 MB). A full clone (`git clone https://github.com/RTimothyEdwards/magic build/ref/magic && git -C build/ref/magic checkout fc5daa9e`) works too and is what reading Magic's sources for porting needs.
- `scmos.tech` is generated from `build/ref/magic/scmos` at configure time (`tests/data/tech/generate_scmos.cmake`, run by `tests/unit/tech/CMakeLists.txt`), so fetch `magic-ref` before configuring, or reconfigure afterwards.

Run one step with `scripts/fetch_pdk.py pdk` or `scripts/fetch_pdk.py magic-ref`; both are no-ops once the data is present and verified. CI runs them in the `build` job behind `actions/cache`, keyed on `scripts/fetch_pdk.py` (where the pins live).

## Dependencies and offline rebuilds

All third-party dependencies are pinned in `cmake/Dependencies.cmake` (zlib, nlohmann/json, the Zstandard decoder, Catch2, and Raven for the desktop build) and fetched with CMake FetchContent into `.cache/deps/<preset>`. Deleting `build/` keeps that cache. To rebuild without network access once the cache is populated:

```sh
cmake --preset macos-debug -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build --preset macos-debug
```

## Source layout

| Path | Contents |
|---|---|
| `src/core/` | `hefesto_core`: layout database, algorithms, command API, diagnostics; no UI, GPU or process APIs |
| `src/cli/` | `hefesto-cli`, the headless tool used by batch flows and golden tests |
| `src/render/vulkan/` | `hefesto_render_vk`, the Vulkan canvas renderer (desktop) |
| `src/shell/desktop/` | `hefesto_shell_desktop` and the `hefesto` app; the only code that uses Raven |
| `tests/unit/` | Catch2 unit tests, tagged with the capability and requirement they cover |
| `tests/fuzz/` | libFuzzer targets (one per external-input reader) and their seed corpora in `corpus/<target>/` |
| `tests/golden/` | golden-case inventories and captured cases |
| `tools/golden/` | reference-tool container, capture runner and comparators |
| `bench/canvas/` | canvas benchmark |

Every first-party source file starts with the SPDX header:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
```

(`#` instead of `//` in CMake and Python files.)

Test labels, traceability tags, the golden suite, fuzzing and the CI checks are described in [`docs/contributing/README.md`](contributing/README.md).
