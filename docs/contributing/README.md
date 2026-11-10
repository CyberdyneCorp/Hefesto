# Contributing to Hefesto

This page covers the day-to-day workflow: building, running each kind of test, and the rules every pull request follows. The build itself (presets, options, offline rebuilds, source layout) is described in [docs/building.md](../building.md).

## Before you start

- Behavior is defined by the specs in `openspec/specs/`. Medium or large changes, and any change to a requirement, go through an OpenSpec change (`openspec/changes/<id>/`) with a proposal, design and tasks. Run `openspec validate --all --strict` before pushing.
- Hefesto is licensed under Apache-2.0 (decision P-9). Read the [clean-room rule](#clean-room-rule) before touching LVS, schematic or OASIS code.
- Supported compilers: GCC 11+, Clang 14+, Apple Clang 15+ (Xcode 15). CMake 3.25+ and Ninja.

## Build

```sh
cmake --list-presets                # presets available on this host
cmake --preset macos-debug          # or linux-debug
cmake --build --preset macos-debug
```

| Preset | Use it for |
|---|---|
| `linux-debug`, `macos-debug` | everyday development |
| `linux-release`, `macos-release` | release builds, profiling |
| `linux-asan`, `macos-asan` | AddressSanitizer + UndefinedBehaviorSanitizer runs |
| `linux-fuzz` | libFuzzer targets (Clang on Linux only) |
| `macos-xcode` | working in Xcode |
| `ipados` | checking that the core configures and builds for iPadOS (configure only) |
| `ci-linux`, `ci-linux-desktop`, `ci-macos` | reproducing a CI leg locally (warnings are errors) |

The desktop app (`hefesto`) is built only when a Vulkan SDK is found. Core and CLI never need it.

## Run the tests

```sh
ctest --preset macos-debug                 # everything registered with CTest
ctest --preset macos-debug -L unit         # Catch2 unit tests only
ctest --preset macos-debug -L golden       # golden replay of captured cases
ctest --preset macos-debug -L golden-tools # unit tests of the golden comparators
ctest --preset macos-debug -LE network     # everything except the PDK download test
ctest --preset macos-debug -R 'Magic string hash'   # by test name
```

### Unit tests and traceability

Unit tests use Catch2 v3 and live in `tests/unit/<area>/`. Every test names the capability and the requirement it covers in its tags, using the exact `### Requirement:` heading from the spec:

```cpp
TEST_CASE("bucket of short key",
          "[cap:hefesto-output-determinism][req:Magic string hash table]") { ... }
```

Every `exact` and `behavioral` requirement needs at least one such test (capability `hefesto-testing-strategy`, requirement "Requirement-traceable tests"). CI publishes the uncovered requirements in the job summary of the `lint` job. Run the report locally:

```sh
python3 scripts/check_requirement_coverage.py --milestone M0        # markdown to stdout
python3 scripts/check_requirement_coverage.py --format json --output coverage.json
python3 scripts/check_requirement_coverage.py --milestone M0 --classes exact,behavioral,native
```

The report also lists tags that name no existing requirement, which usually means a typo in the requirement name.

Each area (M1: `tech`, `db`, `cell`, `display`, `view`, `pdk`, `app`; M2: `undo`, `select`, `connect`, `ui`, `wiring`) builds one executable `hefesto_<area>_tests` from every `*_test.cpp` in `tests/unit/<area>/`, so a new test file needs no CMake edit (reconfigure picks it up). Header-only helpers shared by several areas live in `tests/unit/common/` (for example `reference_data.hpp`, which locates the pinned PDK and Magic data of `scripts/fetch_pdk.py`). Tests that need that data skip when it is absent. The PDK install test over the network is a separate executable, `hefesto_pdk_network_tests`, with the label `network` instead of `unit`; `HEFESTO_SKIP_NETWORK_TESTS=1` makes it skip, and CI runs it on one leg only.

### Diagnostic codes

Each area owns its codes (`AREA-TOPIC-NNN`) in `src/core/src/<area>/codes.cpp` and documents them in `docs/diagnostics/codes/<area>.md`; the M0 codes are the `core` area in `src/core/src/diag/codes.cpp` and `docs/diagnostics/codes.md`. The M2 areas use the prefixes `UNDO`, `SELECT`, `CONNECT`, `UI` and `WIRE` (the `wiring` area's document is `codes/wire.md`). After adding a code, regenerate the table between the document's `BEGIN`/`END DIAGNOSTIC CODES` markers with `hefesto-cli --diagnostic-codes=<area>` (replace only that block; the prose around it is hand-written). A released code is never renumbered or reused; a retired code keeps its row, marked "Retired". The unit test "each area's code table matches its document and prefix" fails when a table and its document disagree.

### Golden suite

Golden cases compare `hefesto-cli` output with output captured from the pinned reference tools (Magic, netgen, ngspice). The inventory of planned cases is `tests/golden/<AREA>/CASES.md`; a captured case lives in `tests/golden/<AREA>/cases/<case-id>/` with its manifest, `input/` and `expected/` (with `provenance.json`). See `tools/golden/` for the capture runner, the comparators and the reference container.

- Replaying never needs the reference tools: `ctest -L golden` runs `hefesto-cli` on each captured case and compares with the canonical comparators.
- Capturing (regenerating `expected/`) runs the reference tools in the pinned container, built only in CI by the capture workflow (`.github/workflows/golden-capture.yml`). On a pull request that adds or edits a `case.toml` or its `input/`, the workflow verifies the captured cases and captures every case without `expected/`; the new `expected/` directories come back as the `golden-expected` artifact, to review and commit. A manual run (`workflow_dispatch`) takes `mode` (`both`, `verify`, `capture`) and an optional list of cases. Do not edit files in `expected/` by hand.
- Cases compared with Magic's console (`tool = "magic"`) run `hefesto-cli --diagnostics=magic`, and each command line of `commands.hef` matches one `puts` line of `commands.tcl`; the `magic-console` comparator drops Magic's startup preamble (`tools/golden/compare/magic_console.py`).
- Masks (`tools/golden/compare/masks.py`) remove fields that never match by design: time stamps, and Magic output of features outside the milestone (`magic-tech-sections`: Magic's cifinput/cifoutput/extract messages at `tech load`; `mag-checkpaint`: the DRC check areas Magic writes into `.mag` files). The authoring generator adds both to every M1 case.
- A case whose Magic output shows behavior Hefesto deliberately does not reproduce (a Magic bug fixed under decision X-2, or another resolved open question) is listed in `tests/golden/deviations.toml` with its decisions and the outputs allowed to differ. Replay reports it as a skipped DEVIATION; it fails if any other output differs, or if it starts to pass (then remove the entry). Never list a case there for a Hefesto bug.
- A case whose Magic output needs a feature of a later milestone (a command or engine not built yet) is listed with `deferred = "<milestone>"` and a reason naming the missing feature (for example CMDEDIT-LBL-07 waits for M3's DRC check plane). The milestone that brings the feature removes the entry; see `tools/golden/README.md`.
- Case inputs are generated by the authoring modules in `tools/golden/authoring/`: the M1 generators, plus `cmdedit_cases.py` (CMDEDIT editing commands), `ui_dispatch_cases.py` (UI command dispatch, macros, tools) and `m2_cases.py` (the other M2 MODEL/TECH/UI cases; `--not-authored` lists the cases left for later milestones). Edit the generator, not the generated `case.toml`/`input/`, and let the capture workflow produce `expected/`.
- CI runs the golden suite on every leg and fails if any platform's output differs from Linux x86_64, except for documented time-dependent fields (requirement "Cross-platform determinism checks").

### Fuzzing

Every reader of external input has a libFuzzer target in `tests/fuzz/` (requirement "Parser fuzzing"): the command-script reader, technology files, `.mag` cells, `.dstyle`, `.cmap`, glyph files, outline fonts, `hefesto.project`, the PDK catalog and manifests, tar unpacking, offline pack manifests and install progress text, crash backup and archive files, the recent-projects list, key-binding (macro) files, the technology `wiring` section and `wire segment` coordinate files. libFuzzer needs LLVM Clang on Linux; Apple Clang does not ship it, so on macOS use the `macos-asan` preset for sanitizer coverage instead. Without `HEFESTO_BUILD_FUZZ`, every target is still built with a replay driver (`tests/fuzz/standalone_main.cpp`) and its seed corpus runs once as the CTest `fuzz-seeds.<target>` (label `fuzz-seeds`), on every platform.

```sh
cmake --preset linux-fuzz
cmake --build --preset linux-fuzz
mkdir -p /tmp/corpus
build/linux-fuzz/tests/fuzz/fuzz_command_script /tmp/corpus -max_total_time=60
```

CI runs each target for 60 seconds on every pull request, and fails if any target with a seed corpus was not built. Seed corpora go in `tests/fuzz/corpus/<target>/`; a new target is added to `HEFESTO_FUZZ_TARGETS` in `tests/fuzz/CMakeLists.txt`, which refuses a target without a corpus. A crash found by fuzzing is fixed together with a regression test that replays the crashing input.

## Checks that run in CI

| Check | Command |
|---|---|
| SPDX header on first-party files | `python3 scripts/check_license_headers.py` |
| Pinned dependencies listed in `THIRD_PARTY_NOTICES.md` | `python3 scripts/check_third_party_notices.py` |
| No UI, GPU, Raven or process headers in the core | `python3 scripts/check_core_includes.py` |
| Requirement coverage report (report-only) | `python3 scripts/check_requirement_coverage.py` |
| Unit tests of the scripts above | `python3 -m unittest discover -s scripts/tests` |
| Specs, roadmap and specs index | `.github/workflows/openspec-validate.yml` |
| Build and test matrix, ASan, fuzz, golden determinism | `.github/workflows/ci.yml` |

The build matrix covers the oldest and newest supported compilers: GCC 11 and the newest GCC on Ubuntu 22.04/24.04, Clang 14 and the newest Clang, Linux aarch64, and macOS arm64 with Xcode 15 and the newest Xcode (requirement "Supported toolchains"). The newest-Clang leg also builds the desktop shell and runs its tests under X11 (Xvfb) and Wayland (headless Weston). The job `gpu-readback-lavapipe` builds only `hefesto_render_tests` and runs it on Mesa's software Vulkan driver (lavapipe, loaded alone through `VK_DRIVER_FILES`) with `HEFESTO_REQUIRE_VULKAN_DEVICE=llvmpipe`, so the GPU readback tests (`tests/unit/render/headless_gpu_test.cpp`) must run there: without that variable they skip when no Vulkan device exists; with it a missing device, or a device whose name does not contain the value, fails the test.

Every first-party source file starts with:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
```

with `#` instead of `//` in CMake, Python and shell files.

## Clean-room rule

Hefesto reproduces the behavior of GPL-licensed tools but never contains their code (capability `hefesto-licensing`, requirement "Clean-room rule for GPL-sourced behavior").

- Do not copy, translate or closely paraphrase code from GPL projects, including **netgen**, **KLayout** and **xschem**.
- Implement behavior that must match those tools only from the Hefesto specifications. If a spec is not precise enough, raise it as a spec change; do not read the GPL source to fill the gap.
- This applies in particular to the netgen-compatible **LVS**, the **OASIS** features and the xschem-compatible **schematic** (`.sch`, `.sym`) editor.
- KLayout may only be run as a separate, user-installed program on desktop (decision P-18). It is never linked or bundled.
- Magic (UC Berkeley notice) and ngspice (modified BSD, GPL parts excluded) are permissively licensed. Ported logic keeps their notices, recorded in `THIRD_PARTY_NOTICES.md`.

The pull request template asks you to confirm this for every pull request that touches these areas.

## Pull requests

- One logical change per pull request, with a description of what changed and why.
- Every bug fix comes with a regression test.
- Update the specs (or the OpenSpec change) and the docs when behavior changes.
- Keep functions simple: the cognitive-complexity target is 15 per function for backend code; flag a function that genuinely cannot meet it instead of contorting it.
- New third-party dependencies are pinned to an exact version in `cmake/Dependencies.cmake` and added to `THIRD_PARTY_NOTICES.md` in the same pull request.
