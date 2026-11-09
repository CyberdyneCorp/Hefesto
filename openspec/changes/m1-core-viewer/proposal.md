# Proposal: M1 Core model, formats and 2D viewer

## Why

M0 left a buildable core with a command API, diagnostics and a Vulkan canvas inside a Raven shell, but Hefesto cannot yet read a technology file, hold a layout or open a cell. M1 (docs/roadmap.md, "M1") makes Hefesto open real PDK layouts, such as the sky130 standard cells, display them with Magic-identical styles, and save them byte-identical. Everything after M1 (editing, DRC, extraction, mask output) builds on the technology database, the layout database and the `.mag` format delivered here.

The user's explicit priority for the desktop shell is that every screen follows the forge design system (`hefesto-design-system`, decision P-23) and its reference mockups (`docs/design/mockups/`). The design system is formally M2, so M1 pulls forward the parts the viewer needs.

## What Changes

- **Technology files (TECH).** Read Magic technology files (format, include/repeat, sections), the layer, plane, contact and stacking model, compose/decompose and connectivity tables, the styles section, loading, scaling and the `tech` command, including the sky130A tech file.
- **Layout database (MODEL).** A corner-stitched plane that reproduces Magic's maximal-horizontal-strip decomposition and tile enumeration order (X-1); paint and erase through the technology's result tables; cell definitions, instances, arrays, hierarchy, labels, ports and typed properties; the minimal editing commands the golden cases need.
- **Cell files (CELL).** Read and write `.mag` with hierarchy, labels and properties; library path resolution and PDK library discovery; I/O safety (locks, atomic save, crash backups); timestamps; the reserved Hefesto property namespace. Round-tripping the sky130 standard-cell library is byte-identical.
- **Viewer (VIEWER).** `.dstyle`/`.cmap`/glyph files (OpenGL variant, X-5), outline-font labels, a core scene builder that turns a cell plus display styles into the renderer's input, the Vulkan renderer drawing it, and the core view commands (zoom, view, scroll, expand, unexpand, see, grid, ...).
- **Desktop shell (SHELL).** The Desktop-Layout screen of the mockups (top bar, tool rail, layer palette and hierarchy, canvas with rulers and minimap, inspector, command bar, status bar), built on the forge tokens (dark theme default), Inter and JetBrains Mono bundled under the OFL, the spacing/radius/density scale, status shapes, and layer colors taken from the PDK display styles. Gaps that Raven v0.2.2 cannot close are recorded in `docs/design/m1-ui-conformance.md`.
- **PDKs, settings and startup (PDK).** The PDK manager (catalog, download, verification, safe unpacking, ciel-compatible install, enable, per-project pin) for sky130, gf180mcu and IHP sg13g2; layered settings and the `hefesto.project` file; the Magic-compatible startup sequence shared by `hefesto-cli` and the desktop app (startup files are located but executed only once M4 adds Tcl).
- **Integration (INTEGRATION).** Shared build files, dependencies, CI, golden capture of the TECH, CELL, MODEL and viewer-related UI cases, notices, README and roadmap.

### Non-goals

- Interactive editing (selection, move, copy, undo, the box/wiring tools), the full `cmd-*` capabilities and the rest of `hefesto-design-system` (icons set, other screens): M2. MODEL implements only the command subset the golden cases need.
- Executing `.magicrc` startup files: needs the Tcl binding (X-3, M4).
- DRC, extraction, GDS/CIF, LEF/DEF: later milestones. `tech drc` reports the DRC section as unavailable.
- iPadOS: the core stays iOS-buildable; no iPad shell.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None. M1 implements the existing requirements of the 25 capabilities listed in docs/roadmap.md ("M1") and pulls forward requirements of `hefesto-design-system` without changing them. Open questions cited by these specs are already resolved (each has a Resolution line in `docs/decisions/open-questions/{TECH,CELL,MODEL,UI,MISC}.md`). The change therefore sets `skip_specs: true`, following the M0 precedent (m0-foundations design D10). A workstream that finds a requirement that must change raises it as an open item; INTEGRATION then adds a spec delta to this change.

## Impact

- **New code:** `src/core/{include/hefesto,src}/{tech,db,cell,display,view,pdk,app,editor}/`, `src/render/vulkan/` (layout scene bridge), `src/shell/desktop/` (Desktop-Layout screen), `assets/fonts/`, `tests/unit/{tech,db,cell,display,view,pdk,app}/`, `tests/data/<area>/`.
- **Shared changes made by the scaffold:** per-area diagnostic code tables and documents (`docs/diagnostics/codes/<area>.md`), `editor::EditorState` reachable from every command handler, `cmd::register_all_commands()` used by the CLI and the desktop shell, per-area CMake fragments.
- **Dependencies:** likely zstd (PDK archives, `.tar.zst`) and Inter/JetBrains Mono font files (OFL-1.1); added by INTEGRATION with pins and notices.
- **Data:** PDK data is fetched into `.cache/pdks/` (gitignored), pinned to exact versions, never committed.
- **Docs:** `docs/design/m1-ui-conformance.md`, diagnostic code documents, building and contributing updates, roadmap status.
