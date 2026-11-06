# Product decisions

These are decisions about Hefesto itself. Each one is cited from `native` requirements as `docs/decisions/product.md#<id>`.

## P-1: Language and toolchain

- Status: decided (2026-11-05)
- Decision: The whole codebase SHALL be ISO C++20. CMake SHALL be the only supported build system. On iPadOS, Xcode project generation goes through CMake.

## P-2: Platforms and minimum OS versions

- Status: decided (2026-11-05)
- Decision: Support these three platforms:

  | Platform | Minimum version | Architectures |
  |---|---|---|
  | Linux | glibc ≥ 2.31 (Ubuntu 20.04 era) | x86_64, aarch64 |
  | macOS | 14 | Apple silicon, x86_64 |
  | iPadOS | 17 | — |

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

## Open product decisions

These remain open and are tracked in `docs/decisions/cross-cutting.md`:

| ID | Topic |
|---|---|
| X-1 | Output-ordering comparison policy |
| X-2 | Replicate or fix Magic bugs |
| X-3 | Scripting runtime per platform |
| X-4 | iPadOS replacements for host facilities |
| X-5 | Rendering fidelity details |
| X-6 | Golden capture |

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

- Status: decided (2026-11-05)
- Decision: Apache-2.0.
- Constraints (from capability `hefesto-licensing`):
  - The license must be compatible with App Store distribution.
  - It must be compatible with the bundled permissive components: Magic, Tcl, ngspice, zlib.
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
- Risks:
  - Raven is at v0.2.x, its public API is unstable, and it has a small maintainer base.
  - Wayland support and input methods on Wayland are immature.
  - It has no drag-out.
- Mitigation:
  - Pin Raven to an exact commit.
  - Wrap Raven in a thin C++20 UI-shell layer. Raven types SHALL NOT appear outside the shell.
  - Contribute fixes upstream.
  - Re-evaluate Raven at each milestone gate.

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
