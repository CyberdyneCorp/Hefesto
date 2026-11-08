# Design: M0 Foundations and Raven go/no-go

## Context

See proposal.md for motivation. The repository has specs, docs and a few Python scripts, but no code. The local development machine is macOS arm64 (Apple Clang 21, CMake 4.x, Ninja); Linux legs are verified in CI only. M0 is split into a scaffolding step followed by four parallel workstreams, so the directory layout, target names, preset names and public interfaces below are a contract: each workstream edits only the directories it owns (D2).

The governing requirements are in the eight M0 capabilities listed in proposal.md, plus `hefesto-performance` (canvas targets) and decisions P-1, P-9, P-11 and X-1 … X-6.

## Goals / Non-Goals

**Goals:**
- A build that configures and tests core + CLI with no GPU, no Vulkan and no Raven on every platform.
- Interfaces (command API, diagnostics, platform services) stable enough that M1 adds commands without reshaping them.
- A measured, documented Raven go/no-go decision.
- A golden-capture pipeline proven end to end on one case.

**Non-Goals:**
- Performance work beyond what the canvas spike needs to answer go/no-go.
- A Tcl binding, the full command-line splitting rules of `ui-command-dispatch`, or command-name abbreviation. M0 commands are matched by exact name; the registry keeps names so that abbreviation lookup can be added in M1 without changing callers.

## Decisions

### D1. Repository layout and targets

| Path | Target(s) | Kind | Links |
|---|---|---|---|
| `src/core/` | `hefesto_core` (alias `hefesto::core`) | static library, namespace `hefesto` | `hefesto::zlib` and `nlohmann_json::nlohmann_json` (both PRIVATE unless a public header needs them) |
| `src/cli/` | `hefesto-cli` | executable (Linux, macOS; not built for iOS) | `hefesto::core` |
| `src/render/vulkan/` | `hefesto_render_vk` (alias `hefesto::render_vk`) | static library, desktop only | `hefesto::core`, `Vulkan::Vulkan` |
| `src/shell/desktop/` | `hefesto_shell_desktop` (alias `hefesto::shell_desktop`) and executable `hefesto` | desktop only | `hefesto::core`, `hefesto::render_vk`, `raven::raven` (PRIVATE) |
| `tests/unit/` | one Catch2 executable per area, created with `hefesto_add_unit_test()` | tests | `Catch2::Catch2WithMain` + the area library |
| `tests/fuzz/` | one libFuzzer executable per reader, created with `hefesto_add_fuzz_target()` | fuzz (Clang with libFuzzer only) | `hefesto::core` |
| `tests/ios/` | `hefesto_ios_link_probe` | iOS app bundle, `ipados` preset only; never shipped | `hefesto::core` |
| `bench/canvas/` | `hefesto-bench-canvas` | executable, desktop only | `hefesto::render_vk` (and `hefesto::shell_desktop` for the windowed mode) |
| `tools/golden/` | Python 3 package + CTest registrations | tooling | runs `$<TARGET_FILE:hefesto-cli>` |

Public core headers live under `src/core/include/hefesto/<module>/…` and are included as `#include "hefesto/<module>/<file>.hpp"`. Implementation files live under `src/core/src/<module>/`. Only `hefesto_shell_desktop` may include Raven headers or name Raven types; the renderer receives Vulkan handles, never Raven types.

Every first-party file starts with the SPDX header (`// SPDX-License-Identifier: Apache-2.0` and `// Copyright 2026 The Hefesto Authors`, or `#` for Python and CMake).

### D2. Ownership contract between workstreams

| Workstream (tasks.md section) | May edit |
|---|---|
| 1 Repo & build (scaffolding, done first) | root `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `.gitignore`, `docs/building.md` |
| 2 Core & CLI | `src/core/`, `src/cli/`, `tests/unit/core/`, `tests/unit/cli/`, `tests/fuzz/`, `docs/diagnostics/` |
| 3 Desktop shell & canvas spike | `src/render/vulkan/`, `src/shell/desktop/`, `tests/unit/shell/`, `tests/unit/render/`, `bench/canvas/`, `docs/spikes/` |
| 4 Golden capture | `tools/golden/`, `tests/golden/<AREA>/cases/` (not the existing `CASES.md` files except to mark a case as captured) |
| 5 CI, licensing, traceability | `.github/workflows/` (new workflows; the existing `openspec-validate.yml` may gain steps), `scripts/` (new scripts only), `THIRD_PARTY_NOTICES.md`, `NOTICE`, `LICENSES/` |
| 6 Integration & docs (after 2–5) | anything, to reconcile; `README.md`, `docs/roadmap.md`, `docs/decisions/` |

Shared files owned by workstream 1 (`CMakeLists.txt`, `CMakePresets.json`, `cmake/*`, `tests/unit/CMakeLists.txt`) are not edited by workstreams 2–5. A workstream that needs a change there (a new option, dependency or preset) reports it, and workstream 6 applies it. Per-directory `CMakeLists.txt` files are owned by the workstream that owns the directory.

### D3. Build system, options and presets

- `cmake_minimum_required(VERSION 3.25)`, languages C and CXX, `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`. In-source builds are rejected.
- Options:

  | Option | Default | Effect |
  |---|---|---|
  | `HEFESTO_BUILD_DESKTOP` | ON if `find_package(Vulkan)` succeeds, else OFF with a status message; always OFF for iOS | `src/render/vulkan`, `src/shell/desktop`, Raven |
  | `HEFESTO_BUILD_TESTS` | ON | `tests/unit`, `tools/golden` CTest registrations |
  | `HEFESTO_BUILD_FUZZ` | OFF | `tests/fuzz` (skipped with a warning if the compiler has no libFuzzer, e.g. Apple Clang) |
  | `HEFESTO_BUILD_BENCH` | OFF | `bench/canvas` (requires `HEFESTO_BUILD_DESKTOP`) |
  | `HEFESTO_WARNINGS_AS_ERRORS` | OFF (ON in `ci-*` presets) | `-Werror` on first-party targets |
  | `HEFESTO_SANITIZERS` | empty | comma list passed to `-fsanitize=`, applied to every target including dependencies |
  | `HEFESTO_TEST_DESKTOP_WINDOW` | OFF (ON in `ci-linux-desktop`) | registers the window smoke tests, which open a window through Raven and need a display |

- `cmake/CompilerWarnings.cmake` defines `hefesto_set_warnings(<target>)`; every first-party target calls it. Third-party targets are compiled without Hefesto's warning flags.
- `cmake/Sanitizers.cmake` applies `HEFESTO_SANITIZERS` globally.
- Presets (binary dir `build/<preset>`, Ninja unless noted):

  | Configure preset | Host | Notes |
  |---|---|---|
  | `linux-debug`, `linux-release` | Linux | |
  | `macos-debug`, `macos-release` | macOS | arm64 only, deployment target 14.0 |
  | `macos-xcode` | macOS | Xcode generator (spec: "generate Xcode projects for macOS") |
  | `ipados` | macOS | Xcode generator, `CMAKE_SYSTEM_NAME=iOS`, deployment target 17.0, iPad device family, core only, configure only |
  | `linux-asan`, `macos-asan` | Linux / macOS | Debug, `HEFESTO_SANITIZERS=address,undefined` |
  | `linux-fuzz` | Linux | Clang, `HEFESTO_BUILD_FUZZ=ON`, `HEFESTO_SANITIZERS=address,undefined,fuzzer-no-link`, desktop OFF |
  | `ci-linux`, `ci-macos` | Linux / macOS | Release with assertions (`RelWithDebInfo`), warnings as errors; the compiler comes from `CC`/`CXX` in the CI job |
  | `ci-linux-desktop` | Linux | `ci-linux` with `HEFESTO_BUILD_DESKTOP=ON` and `HEFESTO_BUILD_BENCH=ON` forced, so a missing Vulkan SDK fails CI instead of silently skipping, and `HEFESTO_TEST_DESKTOP_WINDOW=ON` |
  | `ci-macos-desktop` | macOS | `ci-macos` with `HEFESTO_BUILD_DESKTOP=ON` forced (MoltenVK) |

  Each configure preset except `ipados` has a matching build preset and test preset of the same name.

### D4. Dependency pins (`cmake/Dependencies.cmake`)

| Dependency | Pin | Source | Used by |
|---|---|---|---|
| zlib | v1.3.1, SHA-256 `9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23` | release tarball | core (`hefesto::zlib`) |
| nlohmann/json | v3.11.3, SHA-256 `d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d` | release asset `json.tar.xz` | core, CLI (`nlohmann_json::nlohmann_json`) |
| Catch2 | v3.7.1, commit `fa43b77429ba76c462b1898d6cd2f2d7a9416b14` | git | unit tests |
| Raven | v0.2.2, commit `4a05d6f11c79e5a6eae77d7fe26f702f5fea4331` | git | desktop shell only |

- zlib and nlohmann/json are fetched without running their own CMake files (both declare `cmake_minimum_required` below 3.5, which CMake 4 rejects); Hefesto defines `hefesto::zlib` (static, from the zlib sources) and an interface target for the JSON headers. This also keeps zlib's shared library, examples and install rules out of the build.
- Raven is configured with its examples, tests, tools and web pages off and `RAVEN_FETCH_VULKAN_HEADERS=OFF`, so the desktop build uses the Vulkan SDK that turned the option on and never downloads anything else.
- Presets set `FETCHCONTENT_BASE_DIR=${sourceDir}/.cache/deps/<preset>`. Once a configure has populated it, `cmake --preset <p> -DFETCHCONTENT_FULLY_DISCONNECTED=ON` rebuilds with networking disabled, even after `build/` is deleted ("Offline rebuild" scenario). CI caches `.cache/deps`.
- The reference tools for golden capture are pinned separately, in the container definition (D8).

Alternatives considered: vendoring sources (heavier diffs, no offline benefit over a cache); git pins for zlib and JSON (slower clones, and the JSON repository is large); system packages (not reproducible, unavailable on iPadOS).

### D5. Core / shell boundary

- `hefesto_core` depends only on the C++20 standard library, zlib and nlohmann/json. It includes no windowing, GPU, OS-UI or process-spawning header. Workstream 5 adds a CI check that greps core sources for forbidden includes (`<vulkan/`, `Raven`, `<unistd.h>` process calls, `<spawn.h>`, `<windows.h>`, Apple frameworks).
- The `ipados` preset configuring successfully, plus `cmake --build build/ipados --target hefesto_ios_link_probe --config Debug -- CODE_SIGNING_ALLOWED=NO`, is the evidence for "Core builds without UI" on iPadOS. Archiving the static `hefesto_core` alone runs no link step, so the probe (`tests/ios/`, an unsigned app that runs a command script through the Dispatcher) links the core into an iOS executable; unresolved or iOS-unavailable symbols fail the build. A full iPadOS app build starts with M14.
- Raven appears only inside `src/shell/desktop/`, behind C++20 wrapper classes (`hefesto::shell::Window`, `Dock`, `Panel`, `CommandBar`, `Viewport`). The shell turns UI events into command invocations (D6) and nothing else.
- The renderer (`hefesto_render_vk`) knows Vulkan and the core's geometry types, but not Raven. The shell hands it a Vulkan device, queue, command buffer and viewport rectangle from Raven's viewport draw callback.

### D6. Command API

Headers under `hefesto/cmd/`:

- `Invocation { std::string name; std::vector<std::string> args; Origin origin; }`, where `Origin` is `cli`, `script`, `gui`, `key`, `touch` or `plugin`. Every front end builds an `Invocation`; nothing calls algorithms directly (requirement "Single command API").
- `CommandResult { Status status; Value value; std::vector<diag::Diagnostic> diagnostics; }`, `Status` is `ok` or `failed`. `Value` is a small variant (none, integer, real, string, list of values) so that results map onto Tcl results later.
- `CommandRegistry` maps a name to a `CommandSpec { name, synopsis, help, handler }`. Handlers have the signature `CommandResult(CommandContext&, const Invocation&)`. Registration of a duplicate name is a programming error (assert in debug, diagnostic in release).
- `Dispatcher::execute(const Invocation&)` looks up the command, runs it, attaches the diagnostics emitted while it ran, and notifies `CommandObserver`s (command log, history, replay). An unknown command is a dispatch failure: status `failed`, one `error` diagnostic with code `CMD-DISPATCH-001`.
- `CommandContext` gives a handler the platform services (D7), the diagnostic sink and the project settings (`magic-compat`, `netgen-compat`, `script-strict`), and later the layout database.
- M0 built-in commands are limited to what the skeleton needs and can test: `quit`, `version` and `hefesto::strict` (requirement "Script strict mode switch" — the per-interpreter state lives in the script session; M0 has one session per CLI run). Further commands arrive with their capabilities.
- A handler can mark a failure `CommandResult::raises`: the script runner aborts on it in both modes, like a dispatch failure. `hefesto::strict` uses it for a bad argument ("Any other argument SHALL raise a Tcl error with a usage message"); ordinary failures still abort only in strict mode.
- Desktop key bindings are commands: the canvas keys issue `pan <dx> <dy>`, `zoom 2` / `zoom 0.5` and `view` through the Dispatcher with origin `key`, and the command bar issues its lines with origin `gui`; both are recorded by the session's `CommandLog`. **M0 deviation:** continuous gestures (middle/right drag, wheel, trackpad) move the camera directly, without a command per input event, which would flood the log with view-only state. Whether gestures become coalesced commands (e.g. one `pan` per gesture) is decided with the view commands of `hefesto-app-ux` in M2.
- The M0 command-script reader splits on `;` and newlines into words separated by blanks, with `"…"` and `{…}` grouping and `#` comments at the start of a command. It is a placeholder for the Tcl binding (X-3) and is fuzzed (tests/fuzz).

### D7. Diagnostics and platform services

Diagnostics (`hefesto/diag/`):

- `Diagnostic { Severity severity; std::string code; std::string text; Location location; std::optional<ObjectRef> related; }`, with `Severity` in `error | warning | info | debug`, `Location` a variant of none, `SourceLocation { file, line }` and `LayoutLocation { cell, x, y }` (internal units), and `ObjectRef { kind: cell | instance | net | label | rule, name }`.
- Codes have the form `<AREA>-<TOPIC>-<NNN>` (e.g. `CMD-DISPATCH-001`). They are declared once in a code table in the core (code, default severity, one-line summary), and `docs/diagnostics/codes.md` is generated from or checked against that table by a unit test, so codes stay documented and stable.
- `DiagnosticSink` is the interface every emitter writes to; a collecting sink backs `CommandResult::diagnostics`, and the logging service records every diagnostic.
- Rendering: text form `<severity>: <code>: <text> [at <file>:<line> | in <cell> (<x>, <y>)]`; JSON-lines form with keys in the fixed order `severity, code, text, location, related`, so output is byte-stable across platforms. The CLI prints either form to stderr (`--diagnostics=text|json`).

Platform services (`hefesto/platform/`), one abstract interface each, bundled in `PlatformServices`:

| Interface | Purpose | Core default implementation |
|---|---|---|
| `FileSystem` | read, atomic write, exists, list, remove; documents and storage roots | `std::filesystem` + `<fstream>` |
| `PdkStore` | locate technology and PDK roots | directory list from settings |
| `AutosaveService` | write, list and discard recovery snapshots | files under the app storage root |
| `Clock` | wall time and monotonic time | `std::chrono`; a fixed clock for tests |
| `Logger` | session log, rotation | rotating file through `FileSystem` |
| `TaskExecutor` | background work off the command thread | `std::jthread` pool; an inline executor for deterministic tests |
| `ProcessRunner` | optional external processes | `UnsupportedProcessRunner` (always reports `unsupported`) |

The core ships the portable defaults above because they use only the standard library. The CLI supplies a POSIX `ProcessRunner`; the desktop shell supplies its own where it needs host behavior. The per-user app storage directory (`platform::default_storage_dir()`: `~/Library/Application Support/Hefesto` on macOS, `$XDG_STATE_HOME/hefesto` or `~/.local/state/hefesto` on Linux) and the session log (`platform::install_session_log()`, a `RotatingFileLogger` under `<storage>/logs`) are shared by both hosts: `hefesto-cli` writes `hefesto-cli.log`, the desktop app `hefesto.log`, and both attach a `CommandLog` to their Dispatcher.

Diagnostic presentation ("Diagnostic presentation and logging"): the CLI prints to stderr in text or JSON lines; each session keeps a rotating log; the desktop main window has a **minimal** Diagnostics panel that lists every diagnostic of the session as text. **M0 deviation:** the panel is not filterable and does not link to or navigate to objects; that needs the layout database (M1) and the panel design of the M2 UI, and is deferred to M2. A request on an unsupported service returns a failure with diagnostic `PLATFORM-UNSUPPORTED-001`, never a crash (scenario "No process execution").

Compat settings (`hefesto/compat/`): a `CompatSettings` value with `magic_compat` and `netgen_compat`, both ON by default, changed independently, and a helper that renders the header tag (`magic-compat=on|off`) for netlist and `.ext` writers. The Magic string hash table of `hefesto-output-determinism` lives in `hefesto/util/magic_hash_table.hpp` with the spec's scenarios as unit tests.

### D8. Golden-capture architecture

- **Container.** `tools/golden/container/Containerfile` builds Magic @ fc5daa9e (Tcl build, no X11/OpenGL, as the spec requires the Tcl build to be normative), netgen @ 93dcda14 and ngspice @ 7aa47164 (shared library and batch binary) on a pinned base image digest. Tool commits and the base digest are recorded in `tools/golden/pins.toml`. The image is built in CI (a manual and scheduled workflow), not on developer machines.
- **Case layout.** `tests/golden/<AREA>/cases/<case-id>/` holds `case.toml` (capability, requirements, tool, comparator per output, time-dependent fields to mask), `input/` (tech, layout, `commands.tcl` for the reference tool and `commands.hef` for `hefesto-cli` when they differ), and `expected/` (captured outputs plus `provenance.json`: tool, commit, image digest, capture date).
- **Capture runner.** `tools/golden/capture.py <case-dir>…` runs the reference tool headless in the container, writes `expected/` and provenance. It is deterministic (fixed environment, `TZ=UTC`, `LC_ALL=C`).
- **Replay and compare.** `tools/golden/replay.py` runs `hefesto-cli` on the case input into a temporary directory, then `tools/golden/compare.py` applies the per-file comparator: `bytes` (byte-for-byte after masking listed fields, for tile-order and name-keyed hash-order records) or a format canonicalizer (for the deterministic-order records of X-1). Each canonicalizer is documented next to its code and has its own Python unit tests.
- **CTest.** `tools/golden/CMakeLists.txt` registers one CTest per case that has `expected/` (label `golden`), and the comparator unit tests (label `golden-tools`).
- **The M0 end-to-end case.** One case whose Hefesto side needs only M0 functionality, chosen by workstream 4 with this preference: a case whose output order is a Magic string hash table (requirement "Magic string hash table"), so the pipeline is proven on an `exact` requirement owned by an M0 capability. If no such case can be expressed with M0 commands, the case is captured and compared with a temporary Hefesto-side command documented as M0-only, and the choice is recorded in the go/no-go record.

### D9. Canvas spike and Raven go/no-go

Method:
- **Workload.** A deterministic synthetic layout: 10,000,000 axis-aligned rectangles over 8 layers, generated from a fixed seed with a size and density mix modeled on a sky130 standard-cell region (many small rectangles, a few long routes). The generator is part of the benchmark so every platform draws the same data.
- **Renderer.** Instanced rectangles from a GPU-resident buffer (one instance per rectangle: integer coordinates and layer), with per-layer color and alpha from a style table, drawn into the Raven viewport through Raven's viewport draw callback. A texture-mode variant (render to an offscreen image that Raven composites) is measured as a fallback.
- **Measurement.** A scripted 10-second pan and a zoom sweep from full-chip to 1-µm scale, recording per-frame CPU time and GPU time (timestamp queries); report median, p95 and p99 frame time, peak GPU memory and the time to upload the dataset. Results are written as JSON by `hefesto-bench-canvas` and summarized in `docs/spikes/canvas-spike.md`.
- **Platforms.** Linux (X11 and Wayland) on a mid-range discrete GPU and macOS on an M1-class Mac through MoltenVK, the reference devices of `hefesto-performance`.

Go/no-go criteria (all must hold for "go"):

| Criterion | Pass condition |
|---|---|
| Canvas frame rate | median frame time ≤ 16.7 ms during the pan, on both platforms, with Raven's own UI drawn in the same frame |
| Viewport embedding | the canvas resizes, receives mouse, scroll and keyboard input, and coexists with docked panels without flicker or extra frame latency |
| Text and theming | the command bar and panels render Inter / JetBrains Mono text and can follow the design-system tokens (light and dark) |
| Accessibility hooks | Raven exposes the platform accessibility tree for standard controls on macOS and Linux (screen reader announces panel controls and the command bar) |
| API stability risk | the wrapper layer isolates every Raven call; the count of Raven API symbols used and any breaking changes between v0.2.0 and v0.2.2 are recorded, and judged acceptable |

**M0 deviation (fonts):** Raven v0.2.2 compiles Roboto and Roboto Mono into its code, and the shell falls back to them without `--font-dir`. `hefesto-licensing` says fonts SHALL be data files, never compiled into code. Raven has no option to omit them, so M0 records the conflict in the go/no-go record (criterion "Text and theming") and plans to close it before the first distributed build: Inter and JetBrains Mono bundled as data files and a Raven build without embedded fonts (upstream option or pinned patch), or a spec exception.

The decision, with measurements and the reasoning for each criterion, is recorded in `docs/decisions/raven-go-no-go.md` and logged as a decision in `docs/decisions/product.md` (P-11 follow-up). A no-go replaces only the desktop shell; core, CLI and renderer are unaffected.

### D10. No spec deltas (`skip_specs: true`)

M0 implements existing requirements; none of their observable behavior changes. Implementation choices (FetchContent, Catch2 tags, preset names, comparator formats) are deliberately kept out of the specs, which state behavior. OpenSpec rejects a change with zero deltas unless `.openspec.yaml` sets `skip_specs: true`, which is the documented marker for exactly this case. Inventing a requirement only to satisfy validation was rejected. If a workstream finds that a requirement must change (for example, the golden-suite requirement needs the reference-tool pins), it adds a MODIFIED delta under `specs/` and removes the marker.

### D11. Tests and traceability

- Unit tests use Catch2 v3 and carry tags naming capability and requirement: `TEST_CASE("bucket of short key", "[cap:hefesto-output-determinism][req:Magic string hash table]")`. Tests are discovered at test time (`catch_discover_tests(... DISCOVERY_MODE PRE_TEST)`), so building never runs test binaries.
- The traceability script (workstream 5) collects tags from the Catch2 sources (and `case.toml` of golden cases), joins them with the `exact` and `behavioral` requirements in `openspec/specs/`, and publishes the list of uncovered requirements as a CI artifact (requirement "Requirement-traceable tests").
- Fuzz targets are built only with Clang's libFuzzer; CI runs each for a bounded time (60 s) on pull requests, starting from its seed corpus in `tests/fuzz/corpus/<target>/`. The CI job fails when `fuzz_command_script` was not built, so a failed libFuzzer probe cannot turn the check into a no-op.

### D12. CI matrix

| Job | Runner | Compiler | Preset | Notes |
|---|---|---|---|---|
| linux-x64-gcc-11 | ubuntu-22.04 | GCC 11 (oldest) | `ci-linux` | core, CLI, unit tests |
| linux-x64-gcc-latest | ubuntu-24.04 | newest GCC | `ci-linux-desktop` | desktop shell and renderer built with GCC; tests as for clang-latest |
| linux-x64-clang-14 | ubuntu-22.04 | Clang 14 (oldest) | `ci-linux` | |
| linux-x64-clang-latest | ubuntu-24.04 | newest Clang | `ci-linux-desktop` | Vulkan (Mesa lavapipe) + X11/Wayland dev packages; all tests under Xvfb, unit tests again under a headless Weston, and the window smoke test (`hefesto --smoke-frames`, starts the app through Raven and checks the window system) under each |
| linux-arm64-gcc | ubuntu-24.04-arm | GCC | `ci-linux` | |
| linux-arm64-clang | ubuntu-24.04-arm | Clang 18 | `ci-linux` | roadmap: Linux aarch64 with GCC and Clang |
| macos-arm64-xcode-oldest | macos-15 (oldest installed Xcode; GitHub retired macos-14, so Xcode 15 is no longer available on hosted runners — deviation from "Supported toolchains") | Apple Clang (oldest available) | `ci-macos` | plus `ipados` configure and iOS link probe |
| macos-arm64-latest | latest macOS | Apple Clang | `ci-macos-desktop` | Homebrew MoltenVK + Vulkan loader/headers; desktop shell built against MoltenVK; plus `ipados` and iOS link probe |
| asan | ubuntu-24.04 | Clang | `linux-asan` | |
| fuzz | ubuntu-24.04 | Clang | `linux-fuzz` | 60 s per target |
| lint | ubuntu-24.04 | — | — | SPDX check, core forbidden-include check, traceability report, `openspec validate --all --strict` (existing workflow) |
| golden | ubuntu-24.04 | — | `ci-linux` | replays captured golden cases; the reference image is rebuilt only by the manual capture workflow |
| determinism | ubuntu-24.04 | — | — | byte-compares every leg's golden outputs with linux-x64-gcc-latest; `replay.py --output-dir` applies each case's documented masks (`case.toml`) first, including binary GDS dates |

Desktop coverage in M0: the shell and renderer compile on Linux x86_64 with the newest GCC and Clang and on the newest macOS (MoltenVK). They are not compiled with the oldest toolchains (GCC 11, Clang 14, Xcode 15) or on aarch64; those legs cover core and CLI. Running the window tests on macOS CI is not attempted in M0.

The canvas benchmark needs a real GPU and is run on the reference devices manually in M0; dedicated benchmark runners are a later item of `hefesto-testing-strategy`.

## Risks / Trade-offs

- [Raven API churn before 1.0] → exact-commit pin, a thin wrapper, a recorded symbol list, and re-evaluation at each milestone gate (P-11).
- [Apple Clang has no libFuzzer] → fuzzing runs on Linux only; macOS developers use the `macos-asan` preset.
- [The Linux legs are not built locally] → CI is the source of truth for Linux; scaffolding keeps platform-specific CMake minimal.
- [Vulkan SDK absent on a developer machine] → the desktop option turns itself off with a message, and CI has a leg that forces it on.
- [Reference container built only in CI] → captured outputs are committed with provenance, so replay never needs the container.
- [Minimal M0 script syntax diverges from Tcl] → it covers only what M0 tests use and is replaced by the Tcl binding; it is fuzzed meanwhile.
- [FetchContent on first configure needs network] → the `.cache/deps` cache and `FETCHCONTENT_FULLY_DISCONNECTED` give offline rebuilds; CI caches the directory.

## Migration Plan

Not applicable: there is no existing code or user data. Rollback is reverting the branch.

## Open Questions

- Exact CI runner for a mid-range discrete GPU (needed for automated benchmarks after M0); M0 benchmarks run manually.
