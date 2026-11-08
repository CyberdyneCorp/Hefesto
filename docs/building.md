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
| `HEFESTO_BUILD_FUZZ` | OFF | libFuzzer targets |
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

## Dependencies and offline rebuilds

All third-party dependencies are pinned in `cmake/Dependencies.cmake` (zlib, nlohmann/json, Catch2, and Raven for the desktop build) and fetched with CMake FetchContent into `.cache/deps/<preset>`. Deleting `build/` keeps that cache. To rebuild without network access once the cache is populated:

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
| `tests/fuzz/` | libFuzzer targets |
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
