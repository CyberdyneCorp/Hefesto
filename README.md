# Hefesto

A VLSI layout editor in C++20 for Linux, macOS and iPadOS, data-compatible with
[Magic VLSI](https://github.com/RTimothyEdwards/magic): Magic technology files,
`.mag` cells, DRC, GDS/CIF/LEF/DEF, extraction and SPICE netlists behave the same,
so existing open PDKs work unchanged.

## Repository layout

- `openspec/specs/` — normative specifications (OpenSpec), one folder per capability
- `docs/specs-index.md` — index of all capabilities with counts
- `docs/roadmap.md` — milestones M0–M14 (desktop first with Raven, then iPadOS)
- `docs/decisions/product.md` — decided product facts (C++20, CMake, platforms, Metal/Vulkan, 3D scope)
- `docs/decisions/cross-cutting.md` — decisions to resolve before implementation
- `docs/spec-conventions.md` — how specs are written (compatibility classes, traceability)
- `docs/reference/magic/` — non-normative overview of Magic 8.3.694
- `docs/decisions/open-questions/` — Magic quirks/bugs and platform decisions to resolve
- `tests/golden/` — inventory of compatibility test cases to capture from Magic

Validate specs locally with `openspec validate --all --strict`.
