# Proposal: M0 Foundations and Raven go/no-go

## Why

The repository holds only specifications. Every later milestone needs a reproducible C++20 build, a headless core with the command API and structured diagnostics, CI that enforces the specs, and a way to capture reference outputs from Magic, netgen and ngspice. Two risks have to be settled before M1 starts: whether Raven can host a 10-million-rectangle layout canvas (decision P-11), and whether the X-6 golden-capture approach works at all (roadmap, "Early de-risking schedule").

## What Changes

- **Repository and build.** CMake ≥ 3.25 build with presets for Linux, macOS (Ninja and Xcode) and a configure-only iPadOS preset. All third-party dependencies are pinned to exact versions or commits through FetchContent and can be rebuilt offline from a populated cache: Catch2 v3.7.1, nlohmann/json v3.11.3, zlib v1.3.1, and Raven at the commit of tag v0.2.2 (desktop only).
- **Core and CLI skeletons.** A static library `hefesto_core` (namespace `hefesto`) with no UI, GPU or process API, containing the command-API dispatcher, structured diagnostics, the platform-services interfaces, the `magic-compat` / `netgen-compat` settings and the Magic string hash table. A `hefesto-cli` executable that runs command scripts headless, prints diagnostics as text or JSON lines, and exits with a status code.
- **Desktop shell skeleton.** `hefesto_shell_desktop`, a C++20 wrapper over Raven's C API (the only place Raven appears), and the `hefesto` app with a main window, dock area, panels, command bar and a canvas viewport.
- **Canvas spike.** `hefesto_render_vk`, Hefesto's own Vulkan renderer, drawing 10M rectangles inside a Raven viewport on Linux and on macOS through MoltenVK, plus a benchmark that measures it against `hefesto-performance`. The result feeds the Raven go/no-go decision.
- **Golden capture setup.** A pinned reference-tool container (Magic @ fc5daa9e, netgen @ 93dcda14, ngspice @ 7aa47164), a capture runner, canonical comparators, and one end-to-end golden case passing.
- **CI, licensing and traceability.** A CI matrix (Linux x86_64 GCC and Clang, Linux aarch64, macOS arm64), SPDX Apache-2.0 headers checked in CI, a third-party notices file, and a requirement-traceability report built from test tags.

### Non-goals

- No technology-file, `.mag`, GDS or other format work, no DRC, extraction or netlisting. Those start in M1 and later milestones.
- No iPadOS shell. The `ipados` preset only proves that the core configures for iPadOS.
- No packaging, signing or notarization (capability `hefesto-build-and-distribution`, requirement "Packaging per platform", is completed in M13).
- No Tcl interpreter (decision X-3). The M0 CLI reads a minimal command-script syntax that the Tcl binding later replaces.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None. M0 implements the existing requirements of `hefesto-platform-targets`, `hefesto-architecture`, `hefesto-build-and-distribution`, `hefesto-licensing`, `hefesto-diagnostics`, `hefesto-testing-strategy`, `hefesto-output-determinism` and `hefesto-magic-compat-mode` without changing them. The change therefore sets `skip_specs: true` in `.openspec.yaml` (see design.md, decision D10).

## Impact

- **New code:** `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `src/core/`, `src/cli/`, `src/render/vulkan/`, `src/shell/desktop/`, `tools/golden/`, `tests/unit/`, `tests/fuzz/`, `bench/canvas/`, `tests/golden/<AREA>/cases/`.
- **CI:** a new build-and-test workflow beside the existing `openspec-validate.yml`.
- **Dependencies:** Catch2, nlohmann/json, zlib and Raven (desktop only); a Vulkan SDK (with MoltenVK on macOS) is needed only for the desktop shell and the canvas spike.
- **Docs:** `docs/building.md`, the diagnostic-code catalogue, the canvas spike report and the Raven go/no-go record.
