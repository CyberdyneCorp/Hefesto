# Product decisions

These are decisions about Hefesto itself. Each one is cited from `native` requirements as `docs/decisions/product.md#<id>`.

## P-1: Language and toolchain

- Status: decided (2026-11-05)
- Decision: The whole codebase SHALL be ISO C++20. CMake SHALL be the only supported build system. On iPadOS, Xcode project generation goes through CMake.

## P-2: Platforms and minimum OS versions

- Status: decided (2026-11-05), amended (2026-11-07) by D-12
- Decision: Support these three platforms:

  | Platform | Minimum version | Architectures |
  |---|---|---|
  | Linux | glibc ≥ 2.31 (Ubuntu 20.04 era) | x86_64, aarch64 |
  | macOS | 14 | Apple silicon (arm64) only |
  | iPadOS | 17 | — |
- Amendment (2026-11-07, D-12): macOS x86_64 (Intel Macs) is dropped. macOS builds, CI and the determinism matrix cover Apple silicon only, and no universal binary is shipped. Linux keeps x86_64 and aarch64.

## P-3: Rendering stack

- Status: decided (2026-11-05), revised (2026-11-06) by P-11
- Decision: All layout and 3D drawing SHALL go through one internal rendering interface, with two backends:
  - **Desktop (Linux and macOS):** Vulkan. On Linux it is native; on macOS it runs through MoltenVK, matching Raven's renderer.
  - **iPadOS:** Metal.
- No OpenGL path.

## P-4: 3D visualization scope

- Status: decided (2026-11-05)
- Decision: Implement all four levels:
  1. Parity with Magic's 3D view (layer stack from the tech file's heights and thicknesses).
  2. Interactive 3D inspection: orbit, cross-sections, layer isolation, net highlight, and touch control.
  3. 3D export to glTF 2.0 and STL.
  4. Overlays of extracted parasitics and fields (capacitance, resistance, current density).

## Cross-cutting decisions

These are tracked in `docs/decisions/cross-cutting.md`:

| ID | Topic | Status |
|---|---|---|
| X-1 | Output-ordering comparison policy | decided (2026-11-05) |
| X-2 | Replicate or fix Magic bugs | decided (2026-11-05) |
| X-3 | Scripting runtime per platform | decided (2026-11-05) |
| X-4 | iPadOS replacements for host facilities | decided (2026-11-05) |
| X-5 | Rendering fidelity details | decided (2026-11-05); font-label metrics still open |
| X-6 | Golden capture | open (deferred until the spec is complete) |

Open decisions found by the spec audit are listed in `docs/decisions/audit/DECISIONS.md`.

## P-5: Built-in LVS

- Status: decided (2026-11-05)
- Decision: Hefesto SHALL include a layout-versus-schematic comparison whose results are compatible with netgen.
- Reason: LVS must work on iPadOS without external tools.

## P-6: OASIS support

- Status: decided (2026-11-05)
- Decision: Hefesto SHALL read and write OASIS (SEMI P39) layout files, in addition to GDSII.

## P-7: Embedded SPICE simulation

- Status: decided (2026-11-05)
- Decision: Hefesto SHALL embed ngspice as a library, so that simulation works on every platform, including iPadOS.
- IRSIM support remains the legacy desktop path.

## P-8: Collaboration and diff

- Status: decided (2026-11-05)
- Decision: Hefesto SHALL provide visual and structural layout diff, a git-friendly workflow, and iCloud/Files sync.

## P-9: Hefesto's own source license

- Status: decided (2026-11-05), component list corrected (2026-11-07) by D-13
- Decision: Apache-2.0.
- Constraints (from capability `hefesto-licensing`):
  - The license must be compatible with App Store distribution.
  - It must be compatible with the bundled third-party components, as listed in `hefesto-licensing`:
    - Magic (UC Berkeley permissive notice) and Tcl (BSD-style)
    - ngspice (modified BSD), with its separately licensed linked parts: KLU and numparam (LGPLv2), OSDI (MPL-2.0) and sparse; the GPL parts (`table` code models, FFTW3) are excluded, and the linking form of the LGPL parts is open in D-28 (Q-SIM-1)
    - zlib (zlib license) and Zstandard (BSD-3-Clause)
    - nlohmann/json (MIT) and a glTF writer, cgltf or tinygltf (MIT)
    - on Linux: libsodium (ISC) or Monocypher (BSD-2-Clause or CC0-1.0), and libcurl (curl license) with its TLS library (OpenSSL 3.x, Apache-2.0). On Apple platforms the system CryptoKit and URLSession frameworks are used instead.
    - desktop: Raven (Apache-2.0) and the code it bundles (stb_truetype, stb_image, nanosvg); macOS: MoltenVK (Apache-2.0)
    - fonts: Inter and JetBrains Mono (SIL OFL 1.1), Roboto and Roboto Mono (bundled with Raven, upstream licenses), and FreeSans where bundled for font labels (GPL with the font exception, kept as a separate data file)
    - Eigen (MPL-2.0), only if it is used as the IR-drop sparse solver (D-39, Q-RELIABILITY-9)
    - PDK data, under each PDK's upstream license
  - Hefesto must contain no GPL code.

## P-10: AI layout agent

- Status: decided (2026-11-05)
- Decision: Hefesto SHALL include an AI layout agent with four abilities:
  - natural-language commands
  - device and cell generation
  - a DRC/LVS fix loop
  - layout driven from a schematic
- The agent acts only through the command API.
- AI models are reached through a pluggable provider interface:
  - cloud API
  - self-hosted OpenAI-compatible gateway
  - offline / on-device
- Provider choice is per project. The default is "disabled": no layout data leaves the device until the user enables a provider.
- Autonomy: the agent proposes, and a human approves. The agent works in a scratch copy, verifies its result with DRC and LVS, and presents a diff. Nothing changes the user's cells until the user accepts. Accepting is one undo step.

## P-11: Desktop-first delivery with Raven; iPad app later in Swift

- Status: decided (2026-11-06)
- Decision:
  - **Desktop app (Linux and macOS) ships first.** Its UI is built with Raven (github.com/oscar-soirey/Raven, Apache-2.0, a C11 GUI library with C++ bindings) on top of the C++20 core:
    - Raven renders through Vulkan, using MoltenVK on macOS.
    - Hefesto draws the layout canvas and the 3D view itself, in Raven viewports.
  - **iPad app comes after the desktop app.** It is built in Swift with SwiftUI and Metal, on the same C++20 core through Swift/C++ interoperability.
  - **iPad starts only after the desktop core meets the roadmap's release criteria.** These criteria are a working core that is well tested and passes the golden compatibility tests.
- Raven version: the minimum is v0.2.2 (released 2026-11-06), pinned to an exact commit. v0.2.2 fixed a macOS crash on modifier keys, a Wayland freeze of hidden windows on NVIDIA, swapchain flicker and menu shortcuts, and added the platform features used by `hefesto-app-ux` (Table, system notifications, Dock and launcher progress, the macOS menu bar, SVG images).
- Risks:
  - Raven's public API is unstable until its 1.0, which plans versioned structures. Its maintainer base is small.
  - Input methods for Chinese, Japanese and Korean are not connected on macOS, Windows or Wayland yet. ASCII names are unaffected; non-Latin label text is.
  - Wayland is tested only against Raven's own test compositor, not GNOME, KDE or wlroots. Linux CI therefore covers both X11 and Wayland.
  - It has no drag-out to other applications.
- iPad and Raven: Raven may gain an iOS and Metal backend (upstream proposal #37, not merged as of 2026-11-07). The iPad app stays Swift and SwiftUI regardless (decided 2026-11-07).
- Mitigation:
  - Pin Raven to an exact commit.
  - Wrap Raven in a thin C++20 UI-shell layer. Raven types SHALL NOT appear outside the shell.
  - Contribute fixes upstream.
  - Re-evaluate Raven at each milestone gate.
- M0 go/no-go (follow-up): in progress. Provisional recommendation "GO, conditional" (2026-11-07), pending the M1 Mac and Linux frame-rate runs, live input, Inter / JetBrains Mono and screen-reader checks. Record: `docs/decisions/raven-go-no-go.md`; the final decision is logged here at the M0 gate.

## P-12: Built-in schematic editor, xschem-compatible

- Status: decided (2026-11-06)
- Decision:
  - Hefesto SHALL include a schematic editor that reads and writes xschem `.sch`/`.sym` files and xschem symbol libraries, so that the PDK libraries for sky130, gf180mcu and IHP sg13g2 work unchanged.
  - Its SPICE netlists SHALL be compatible with xschem's. Schematics drive simulation and LVS.
- Licensing: xschem is GPL, so the implementation SHALL be clean-room from the specs (capability `hefesto-licensing`).

## P-13: iPad v1 feature scope

- Status: decided (2026-11-06)
- Decision: The first iPadOS release SHALL include:
  - viewing, layout editing, DRC and PCells
  - schematic editing and ngspice simulation
  - extraction, LVS and 3D
  - GDS/OASIS export
  - the AI agent
- These are desktop-only at first:
  - the digital flow integration
  - the legacy tools: channel and gate-array routers, plow, netlist window, IRSIM
  - shell commands and git integration
- The authoritative list is capability `hefesto-ipad-feature-scope`.

## P-14: In-app PDK manager

- Status: decided (2026-11-06)
- Decision: Hefesto SHALL let users browse, download, install, update and pin open PDKs inside the app on every platform. This is the only way to get a PDK on iPadOS.

## P-15: Tapeout readiness

- Status: decided (2026-11-06)
- Decision: Hefesto SHALL support taking a design to fabrication:
  - fill generation and density checks
  - seal rings
  - a sign-off checklist
  - export packages for open shuttle programs

## P-16: Onboarding and help

- Status: decided (2026-11-06)
- Decision: Hefesto SHALL provide:
  - a new-project wizard
  - templates and example designs
  - interactive tutorials
  - in-app help for every command

## P-17: Digital flow integration (desktop-only)

- Status: decided (2026-11-06)
- Decision: On the desktop, Hefesto SHALL integrate with the open digital flow (OpenROAD/LibreLane). It SHALL import hardened blocks and MAY run the flow as an external process.

## P-18: KLayout-based sign-off runs externally on desktop

- Status: decided (2026-11-06). Resolves Q-TAPEOUT-1.
- Decision:
  - **Desktop:** where a PDK's sign-off depends on KLayout rule decks (gf180mcu and IHP DRC, fill and density; the KLayout checks in sky130 prechecks), Hefesto SHALL run the user's installed `klayout` as an external process and import its results. KLayout is GPL and SHALL NOT be linked or bundled.
  - **iPadOS:** designs for such PDKs can be built, checked with Magic-compatible DRC, and exported. They SHALL NOT be marked "sign-off ready" until the KLayout checks have passed on a desktop.
  - **sky130:** sign-off based on Magic DRC works on every platform. Its KLayout prechecks follow the desktop rule above.

## P-22: Product operations

- Status: decided (2026-11-06)
- Decision: Hefesto SHALL provide:
  - opt-in crash reporting and usage telemetry, with no layout data collected
  - desktop update channels: stable and beta, with signed updates
  - published minimum system requirements
- Capability: `hefesto-product-operations`.

## P-23: Visual design system

- Status: decided (2026-11-06)
- Decision: Hefesto's look is defined by a design system:
  - design tokens: color, typography, spacing, radius, elevation, motion
  - an icon set and component patterns
  - light and dark themes
- Desktop: implemented through Raven's theme system. iPadOS: implemented in SwiftUI.
- Layout layer colors come from the PDK display styles. They are not part of the UI theme.
- Capability: `hefesto-design-system`. Screen mockups are kept as design artifacts and linked from it.

## P-19: Advanced analog simulation

- Status: decided (2026-11-06)
- Decision: On top of the embedded ngspice engine (P-7), Hefesto SHALL provide:
  - parameter, temperature and supply sweeps, nested sweeps and design-of-experiments sampling
  - corner analysis built from the corner definitions in the PDK store, with a corner matrix view, "run all" and pass/fail per corner
  - Monte Carlo analysis of process variation and device mismatch through the PDKs' own statistical models, with seeds that make every sample reproducible on every platform
  - measurements, specifications with limits, and statistics: histograms, mean, sigma, yield, Cpk, correlation, and result export
  - optional hooks that let the AI agent (P-10) run optimisation loops through the same evaluation path
- Platforms: all of them, including iPadOS within the iPad scope (P-13). On Linux and macOS, run sets are spread over worker processes. On iPadOS they run one simulation at a time, in process, and give the same results.
- Capabilities: `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`, `simulation-measurements-and-statistics`, `simulation-optimization-hooks`. Open questions: `docs/decisions/open-questions/SIMADV.md`.

## P-20: Plugin and extension API

- Status: decided (2026-11-06)
- Decision: Hefesto SHALL be extensible by third-party plugins. A plugin can:
  - register commands in the single command API (X-3), so every plugin action is scriptable, logged and replayable
  - add panels and views, described declaratively and rendered by each shell (no Raven or SwiftUI types cross the boundary)
  - add file importers and exporters, DRC and analysis checks, 2D/3D overlays, and PDK adapters
- Binary interface: a **stable, versioned C ABI** (`hefesto/plugin.h`) is the only contract between host and native plugins. A **header-only C++20 wrapper** (`hefesto/plugin.hpp`) gives RAII, spans and error types on top of it. C++ types, exceptions and allocators never cross the boundary. This keeps plugins working across compilers, standard libraries and Hefesto releases. Versioning: `major.minor`; minors only append; a major is supported for at least two host major releases or 24 months after its successor ships, whichever is longer.
- Plugin kinds:
  - **native** (shared library): Linux and macOS only. Runs in process, so its permissions are declarative and enforced at the API boundary only. The consent UI says so.
  - **script** (sandboxed Tcl, the X-3 sandbox): every platform, with capability-based permissions enforced by the sandbox.
  - **built-in**: native plugins compiled into the app at build time, registered statically. This is the only native form on iPadOS.
- iPadOS: no native code is loaded at runtime (App Store guideline 2.5.2). User-installed script plugins are allowed, subject to the same App Review risk as PDK scripts (X-3). If App Review rejects them, iPadOS falls back to built-in and curated, bundled script plugins only.
- Distribution: `.hfplugin` packages with a JSON manifest, Ed25519 signatures, SemVer dependencies and an optional signed catalog. Updates are never applied without consent, and any permission escalation needs fresh consent.
- SDK: headers, CMake package, packaging and signing tool, sample plugins, a headless test and conformance harness, and generated documentation. SDK headers are Apache-2.0.
- Capabilities: `plugin-api`, `plugin-packaging-and-distribution`, `plugin-security-and-platforms`, `plugin-sdk`. Open questions: `docs/decisions/open-questions/PLUGIN.md`.

## P-21: Electromigration and IR-drop analysis

- Status: decided (2026-11-06)
- Decision: Hefesto SHALL provide layout reliability analysis on every platform, including iPadOS:
  - **EM rules as PDK data:** current-density limits per layer, via and contact, by width range, temperature, current metric (average, RMS, peak) and direction, stored as a rule file in the PDK store with provenance. Hefesto bundles what the PDKs publish: IHP sg13g2 (process spec §2.15) and gf180mcu (design manual §14.2). sky130 publishes no EM limits, so its rule file is empty until the user supplies one.
  - **IR drop:** static and dynamic IR drop on power and ground nets. The grid comes from the `extresist` resistance network with reduction disabled. Load currents come from embedded-simulation results, or from a co-simulation of the extracted grid. Hefesto solves the grid with its own sparse linear solver.
  - **EM check:** per-segment and per-via current (average, absolute average, RMS, peak) against the rule limits, with a deterministic report, violations as diagnostics, and an optional sign-off checklist item.
  - **Visualization:** 2D and 3D heat maps through the `hefesto-3d-visualization` overlays, a violation browser, and cross-probing to layout, waveforms and schematics.
- Hefesto makes no lifetime prediction beyond comparing currents to the PDK limits. Thermal (self-heating) analysis is out of scope.
- Capabilities: `reliability-em-rules`, `reliability-ir-drop`, `reliability-em-check`, `reliability-visualization`. Open questions: `docs/decisions/open-questions/RELIABILITY.md`.
