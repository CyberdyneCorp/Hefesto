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

- Status: decided (2026-11-05)
- Decision: All drawing (2D and 3D) SHALL go through one internal rendering interface, with two backends:
  - Metal on macOS and iPadOS.
  - Vulkan on Linux.
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
