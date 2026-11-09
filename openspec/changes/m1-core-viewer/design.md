# Design: M1 Core model, formats and 2D viewer

## Context

See proposal.md for motivation. M0 (openspec/changes/m0-foundations) delivered `hefesto_core` with the command API, diagnostics, platform services, compat settings and the Magic string hash table; `hefesto-cli`; the Raven desktop shell with a Vulkan canvas; the golden harness; CI. Conventions from M0 still hold: SPDX headers, Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`, diagnostic codes `AREA-TOPIC-NNN` documented and checked, cognitive complexity at most 15 per function, no UI/GPU/Raven/process headers in the core.

M1 runs as a scaffolding step (this change's interfaces and build contract) followed by six parallel workstreams and an integration step. The directory layout, public headers, ownership table and shared-file rules below are the contract that lets the workstreams work without editing each other's files.

The reference for exact behavior is Magic 8.3.694 at fc5daa9e. Read it from `build/ref/magic` (gitignored): `git clone https://github.com/RTimothyEdwards/magic build/ref/magic && git -C build/ref/magic checkout fc5daa9e`. Ported logic keeps Magic's attribution (capability `hefesto-licensing`, `THIRD_PARTY_NOTICES.md`). netgen, xschem and KLayout sources are never read (clean-room rule). Docker is not available on the development machine: golden references are captured in CI by `.github/workflows/golden-capture.yml`.

## Goals / Non-Goals

**Goals:**
- The M1 exit criteria of docs/roadmap.md: golden TECH, CELL, MODEL and the viewer-related UI cases pass; the sky130 standard-cell library round-trips byte-identical.
- A desktop viewer whose every screen element follows the forge design system and the Desktop-Layout mockup, with each unavoidable gap recorded.
- Interfaces stable enough that M2 (editing) adds behavior without reshaping them.

**Non-Goals:**
- Editing tools, selection, undo, full `cmd-*` commands: M2.
- Executing `.magicrc` files (needs Tcl, M4); DRC, extraction, mask output.

## Decisions

### D1. Layout of the M1 code

| Area | Public headers | Sources | Unit tests | Namespace |
|---|---|---|---|---|
| technology database | `src/core/include/hefesto/tech/` | `src/core/src/tech/` | `tests/unit/tech/` | `hefesto::tech` |
| layout database | `src/core/include/hefesto/db/` | `src/core/src/db/` | `tests/unit/db/` | `hefesto::db` |
| cell files | `src/core/include/hefesto/cell/` | `src/core/src/cell/` | `tests/unit/cell/` | `hefesto::cell` |
| display styles, fonts, scene | `src/core/include/hefesto/display/` | `src/core/src/display/` | `tests/unit/display/` | `hefesto::display` |
| windows and view commands | `src/core/include/hefesto/view/` | `src/core/src/view/` | `tests/unit/view/` | `hefesto::view` |
| PDK manager | `src/core/include/hefesto/pdk/` | `src/core/src/pdk/` | `tests/unit/pdk/` | `hefesto::pdk` |
| settings, project file, startup | `src/core/include/hefesto/app/` | `src/core/src/app/` | `tests/unit/app/` | `hefesto::app` |
| session state (shared) | `src/core/include/hefesto/editor/` | `src/core/src/editor/` | `tests/unit/app/` (scaffold) | `hefesto::editor` |
| Vulkan renderer | `src/render/vulkan/include/hefesto/render/` | `src/render/vulkan/src/` | `tests/unit/render/` | `hefesto::render` |
| desktop shell | `src/shell/desktop/include/hefesto/shell/` | `src/shell/desktop/src/` | `tests/unit/shell/` | `hefesto::shell` |

All M1 core code is part of `hefesto_core` (no new library targets). Test fixtures live in `tests/data/<area>/` (small hand-written files only; PDK data never). Fetched PDK data lives in `.cache/pdks/` (gitignored), pinned to an exact version and fetched minimally (for example the sky130A tech file, its display styles and the `sky130_fd_sc_hd` `.mag` library).

### D2. Ownership

| Workstream (tasks.md section) | Owns (may edit) |
|---|---|
| TECH | `src/core/{include/hefesto,src}/tech/`, `tests/unit/tech/`, `tests/data/tech/`, `docs/diagnostics/codes/tech.md` |
| MODEL | `src/core/{include/hefesto,src}/db/`, `tests/unit/db/`, `tests/data/db/`, `docs/diagnostics/codes/db.md` |
| CELL | `src/core/{include/hefesto,src}/cell/`, `tests/unit/cell/`, `tests/data/cell/`, `docs/diagnostics/codes/cell.md` |
| VIEWER | `src/core/{include/hefesto,src}/display/`, `src/core/{include/hefesto,src}/view/` (core view commands of `ui-windows-and-views`), `src/render/vulkan/`, `tests/unit/{display,view,render}/`, `tests/data/{display,view}/`, `docs/diagnostics/codes/{display,view}.md` |
| SHELL | `src/shell/desktop/`, `assets/fonts/` (and other `assets/` UI resources such as icons), `tests/unit/shell/`, `docs/design/m1-ui-conformance.md` |
| PDK | `src/core/{include/hefesto,src}/pdk/`, `src/core/{include/hefesto,src}/app/` (settings layers, project file, startup), the CLI's M1 commands and options in `src/cli/`, `tests/unit/{pdk,app,cli}/`, `tests/data/{pdk,app}/`, `docs/diagnostics/codes/{pdk,app}.md` |
| INTEGRATION | everything shared: root and `src/core/CMakeLists.txt`, `CMakePresets.json`, `cmake/` (incl. `Dependencies.cmake`), `tests/unit/CMakeLists.txt`, `tests/unit/core/`, `src/core/{include/hefesto,src}/{cmd,diag,platform,settings,compat,util,editor}/`, `.github/`, `tools/golden/`, `tests/golden/`, `scripts/`, `README.md`, `docs/roadmap.md`, `docs/building.md`, `docs/contributing/`, `docs/diagnostics/codes.md`, `THIRD_PARTY_NOTICES.md`, `NOTICE`, `LICENSES/`, `.gitignore`, this change's artifacts |

Rules:
- A workstream edits only what it owns. A needed change to a shared or another workstream's file is reported as an `open_item` (what, why, the exact edit if small); INTEGRATION applies it.
- New golden case directories `tests/golden/<AREA>/cases/<ID>/` with `case.toml` and `input/` may be authored by the area owning that capability (new paths only, no `expected/`); INTEGRATION captures `expected/` in CI and edits `CASES.md`.
- The per-area `codes.cpp`, `sources.cmake` and `tests/unit/<area>/CMakeLists.txt` files are owned by the area.
- Nobody commits, merges, switches branches or touches `main`; only the integration step commits and pushes to `origin m1-core-viewer`, when its prompt says so.

### D3. Build and diagnostics contract: owners never edit shared files

- **Sources.** `src/core/CMakeLists.txt` includes `src/core/src/<area>/sources.cmake` for `tech db cell display view pdk app`. Each fragment calls `target_sources(hefesto_core PRIVATE ${CMAKE_CURRENT_LIST_DIR}/<file>.cpp ...)`. A new dependency is linked from the fragment (`target_link_libraries(hefesto_core PRIVATE <target>)`) once INTEGRATION has added it to `cmake/Dependencies.cmake`.
- **Tests.** `tests/unit/CMakeLists.txt` adds `tests/unit/<area>/` for every M1 area. Each calls `hefesto_add_area_tests(<area>)`, which globs `*_test.cpp` into `hefesto_<area>_tests`, links `hefesto::core`, and defines `HEFESTO_SOURCE_DIR` and `HEFESTO_TEST_DATA_DIR` (`tests/data/<area>`). Adding a test file needs no CMake edit (reconfigure picks it up). `render/` and `shell/` keep their explicit lists, owned by VIEWER and SHELL.
- **Diagnostic codes.** The code table is split into areas (`diag::code_areas()`): `core` (M0 codes, `src/core/src/diag/codes.cpp`, `docs/diagnostics/codes.md`) and one area per M1 directory with a fixed prefix — `TECH-`, `DB-`, `CELL-`, `DISPLAY-`, `VIEW-`, `PDK-`, `APP-` — defined in `src/core/src/<area>/codes.cpp` and documented in `docs/diagnostics/codes/<area>.md`. Regenerate an area document with `hefesto-cli --diagnostic-codes=<area>`. The unit test "each area's code table matches its document and prefix" enforces both. Code constants go in an area header (e.g. `hefesto/tech/diag_codes.hpp`, namespace `hefesto::tech::codes`).
- **Stubs.** Every declared function either works (small, obvious pieces) or returns `diag::not_implemented("<feature>")`, code `CORE-NOTIMPL-001`. Every M1 command name is registered with a stub handler (`src/core/src/cmd/stub_commands.hpp`) so help, dispatch and golden scripts see the final command set. An area replaces its stubs; INTEGRATION removes `CORE-NOTIMPL-001` and the stub helper once no area uses them (M1 exit).
- **Tests of the scaffold** carry the tag `[m1-scaffold]` and no `[cap:][req:]` tags (they cover no requirement). Owners replace them with requirement-tagged tests.

### D4. Session state and command registration

- `editor::EditorState` (`hefesto/editor/editor_state.hpp`) aggregates the session: `technology` (`std::shared_ptr<const tech::Technology>`, never null; starts as `tech::builtin_technology()`), `grid` (`tech::GridScale`), `cells` (`db::CellLibrary`), `edit_cell`, `box`, `locked_types`, `search_paths`, `path_variables`, `locks` (`cell::FileLockService`, a pure host mechanism), `cell_session` (`cell::CellSession`: the `locking` setting, file host, timestamp log, crash backups and their timer, recovery; it outlives a replacement of `locks`), `styles` (optional `display::StyleSet`, absent headless), `fonts`, `windows` (`view::WindowManager`), `settings` (`app::SettingsStack`). The M0 typed project settings (`magic-compat`, `netgen-compat`, `script-strict`) stay in `settings::ProjectSettings` via `CommandContext::project()`.
- Handlers reach it through `cmd::CommandContext::editor()`; `cmd::require_editor(ctx)` returns it or `CMD-SESSION-001`. `cmd::Dispatcher::set_editor()` attaches it; `hefesto-cli` and the desktop `CoreSession` own one `EditorState` each and attach it.
- `cmd::register_all_commands(registry)` registers the M0 built-ins plus `tech::register_tech_commands`, `db::register_db_commands`, `cell::register_cell_commands`, `view::register_view_commands`, `pdk::register_pdk_commands`. The CLI and the shell call only this function. A new area command is added inside the area's `commands.cpp`.
- A member an area needs in `EditorState` (e.g. an undo log for M2) is requested as an open item. Types of members are owned by their areas, so changing a member's *type* stays inside the area.

### D5. View commands: core vs shell

Magic's view commands belong to the core (`ui-windows-and-views`), so they work headless and are logged and replayed like any command. The M0 shell registered placeholders `view`, `zoom`, `pan` and `theme` itself (`src/shell/desktop/src/view_commands.cpp`). To avoid `CMD-REGISTRY-001` during the parallel phase, the scaffold's `view::register_view_commands` registers every view command except `view` and `zoom`. Hand-over, within M1:
1. VIEWER implements `view`, `zoom` and the rest against `view::LayoutWindow` (`ViewMapping`, expansion, visibility, grid) and notifies `view::ViewObserver`s; VIEWER adds `view` and `zoom` to the core registration in the same change in which SHELL removes its placeholders.
2. SHELL observes `editor.windows` (a `ViewObserver`) and maps the window's `ViewMapping` to the Vulkan camera; canvas keys and gestures issue core commands (`zoom`, `scroll`, `view`, `findbox`), with origin `key`/`gui`. `pan` (a pixel pan, not a Magic command) and `theme` stay shell commands.
3. INTEGRATION checks with a unit test that `register_all_commands` plus the shell's registrations produce no duplicate.

### D6. UI and UX: the forge design system in M1 (user priority)

Every UI the desktop shell shows follows `openspec/specs/hefesto-design-system/spec.md` and the reference mockups in `docs/design/mockups/` (`Main.dc.html`: tokens, type scale, components; `Desktop-Layout.dc.html`: the layout editor screen; `README.md` there). M1 pulls forward from M2:

- **Tokens and themes.** The color tokens of "Color tokens and themes", dark theme the default, light theme selectable (`theme dark|light`); the canvas stays `#0B0C0E` in both. Values live in `hefesto/shell/theme_tokens.hpp` (colors) and `hefesto/shell/design_tokens.hpp` (type scale, spacing, radius, elevation, motion, density, status). Raven's theme is built only from these values; no hard-coded color in the Raven layer.
- **Typography.** Inter for UI text, JetBrains Mono for coordinates, dimensions, cell/net names, commands and code; the type scale of "Typography". Both fonts are bundled as data files under `assets/fonts/` (OFL-1.1, license texts beside them), with entries in `THIRD_PARTY_NOTICES.md` (INTEGRATION) and loaded through Raven's font API.
- **Spacing, radius, density.** 4 px spacing scale; 4 px control radius, 8 px panel radius; panels have a 1 px border and no shadow; popovers use the specified shadow; controls 28–32 px tall; 120 ms / 200 ms motion, none under reduced motion.
- **Status is never color only.** Every status indicator pairs a shape (● success, ▲ warning, ■ error, ◆ info) and text with its color: DRC/LVS chips in the top bar, diagnostics panel rows, console results, PDK install/verify state.
- **Layer colors from the PDK.** The canvas and the layer palette swatches use `display::layer_palette()` / the scene's resolved styles (color, opacity, stipple, outline) from the PDK display styles; the theme never recolors layers. Selection and box are white (solid / dashed); the ember accent is for actions, focus and the active tool only, never canvas geometry.
- **Screen structure (Desktop-Layout.dc.html).** From top to bottom: top bar (48 px: app mark, cell name, PDK · version · magic-compat chip, view tabs Layout/Schematic/3D/Waveforms, DRC/LVS status chips with shapes, actions); a row with the tool rail (52 px, 36 px buttons), the left panel (240 px: Layers with filter, swatch + name + visibility per layer; Hierarchy tree of instances), the canvas (rulers on two edges, zoom control, minimap 168×112 bottom-right, white selection/box), the inspector (284 px: selection details, layer, actions, nearby rules); the command bar (40 px, JetBrains Mono 12 input, hint text); the status bar (26 px, JetBrains Mono 11: cursor x/y in microns, box size, edit cell, snap and units, DRC state with shape). M1 fills these with M1 data (hierarchy from `db`, layers from the PDK styles, cursor coordinates and box from `view`/`editor`); elements whose function comes later (Schematic/3D tabs, Extract/Simulate/Ask agent, nearby rules) are present and disabled, with a tooltip naming the milestone, so the structure matches the mockup.
- **Conformance record.** `docs/design/m1-ui-conformance.md` is a table `mockup element → implemented as → gap/why` covering every element above. Where Raven v0.2.2 cannot match the mockup, SHELL does the closest faithful thing and records the gap; a gap that needs a Raven change is also an open item. Known spec/mockup differences are recorded there too (e.g. the mockup draws 6 px control radii and a few extra surface shades not in the token table); the spec value wins until the design artifact and the spec are reconciled (design changes go to the artifact first, per "Screen patterns follow the reference mockups").
- **Accessibility.** Icon-only buttons carry accessible labels; keyboard focus uses the ember focus ring.

### D7. Technology, layout and file behavior

- **Compatibility.** Behavior follows the resolved open questions (each cited entry in `docs/decisions/open-questions/{TECH,CELL,MODEL,UI,MISC}.md` has a Resolution line). Behaviors resolved as "compat" depend on the `magic-compat` setting (`settings::ProjectSettings::compat`, passed down as `magic_compat` flags in the option structs).
- **Tile order (X-1).** `db::Plane::enumerate` reproduces Magic's area-enumeration order exactly; the `.mag` writer enumerates every user type in technology order over the whole plane. MODEL may store geometry in any structure (spec: "Geometry data structure is informative") but the public plane is corner-stitched per decision X-1.
- **Hash-table order.** Wherever Magic iterates a string hash table observably (cell namespace, properties), the order comes from `util::MagicHashTable` (M0).
- **Tech → display.** A technology loads headless without display files (Q-TECH-24). TECH keeps the styles section raw (`tech::StylesSection`); VIEWER resolves it against the `.dstyle` with `display::resolve_type_styles`.
- **Host mechanisms.** The core has no sockets, locks or symlinks: `cell::FileLockService` (advisory locks, read-only detection), `pdk::Fetcher` (downloads) and `pdk::InstallHost` (symlinks, environment, home) are interfaces the CLI/desktop implement; portable defaults exist for tests and headless runs. The front ends' `Fetcher` is the in-process HTTPS client of hefesto-licensing D-13 in `src/platform_net` (URLSession through an Objective-C++ bridge on macOS, the pinned libcurl on Linux); the core's curl-process `Fetcher` remains only for hosts that supply none.

### D8. Golden cases and exit criteria

- Cases: `tests/golden/{TECH,CELL,MODEL,UI}/CASES.md`. The viewer-related UI cases are those of `display-style-files`, `ui-windows-and-views` and `font-labels`.
- Golden scripts run through `hefesto-cli` with the full command set, so each area implements the commands its cases use (MODEL: `box`, `snap`, `paint`, `erase`, `splitpaint`, `getcell`, `flatten`, `label`, `setlabel`, `port`, `property`, `cellname`, `instance`, `array`; CELL: `load`, `save`, `writeall`, `path`, `addpath`, `locking`, `crashbackups`; TECH: `tech`, `scalegrid`; VIEWER: view commands). The command subsets follow Magic's syntax for the forms the cases use.
- Expected outputs are captured only in CI (`golden-capture.yml`); locally `ctest -L golden` replays committed cases.
- Byte-identical round trip of `sky130_fd_sc_hd` from `.cache/pdks/` is a CTest registered by INTEGRATION that skips when the PDK cache is absent and runs in CI after fetching the pinned PDK.

### D9. Dependencies and data

- Allowed new third-party code: zstd (PDK `.tar.zst`), pinned in `cmake/Dependencies.cmake` with a notice; tar parsing and SHA-256 are implemented in the core (small, testable, no new dependency). Fonts are data, not code.
- Linux GCC pitfalls seen in M0 CI: no casts between `size_t`/`uint64_t`/`uintmax_t` (`-Wuseless-cast`; use implicit conversions), include `<mutex>` for `std::unique_lock`, no unsequenced calls in one expression.

## Area contracts

The headers are the contract; each is documented. Key types and functions per area:

### TECH — `hefesto/tech/`
- `types.hpp`: `TileType` (uint16), `PlaneId` (uint8), `TypeMask` (256-bit), `PlaneMask` (uint64), built-in type/plane constants, `NameForm`.
- `technology.hpp`: `Technology` (immutable, shared): planes, types, `find_type`/`find_plane` (`Lookup<Id>` with unknown/ambiguous), `parse_type_list`, contacts (`ContactInfo`, `residue_on`), `paint_result`/`erase_result`/`write_result`, `paint_planes`/`erase_planes`, `connects_to`, raw `styles()`; `TechnologyBuilder` (programmatic build used by the loader and by other areas' tests); `GridScale`.
- `tech_file.hpp`: `read_tech_text`/`read_tech_file` → `TechDocument` (sections of token lines with locations).
- `loader.hpp`: `LoadOptions`, `resolve_tech_file`, `load_technology`, `builtin_technology`, `check_grid_limit`, `reduce`.
- `commands.hpp`: `tech`, `scalegrid`.

### MODEL — `hefesto/db/`
- `geometry.hpp` (constexpr): `Coord`, `Point`, `Rect`, Magic's predicates, `whole_plane`, `include`, `in_usable_plane`.
- `transform.hpp`: `Transform` (Magic a..f), `apply`, `compose`, `invert`, `transform_position`, `transform_angle`.
- `plane.hpp`: `Plane` (corner-stitched, opaque), `paint(Rect, PaintRow)`, `paint_triangle`, `enumerate(area, TypeMask, visitor)` in Magic order, `type_at`, `bounding_box`; `TileView`, `Diagonal`.
- `cell.hpp`: `CellDef` (planes, labels, properties, uses, flags, bboxes, file path, timestamp), `CellUse`, `ArrayInfo`, `CellFlags`, `CellLibrary` (create/find/rename/remove/for_each in hash order).
- `label.hpp`, `property.hpp`: `Label` (with port and font fields), `PropertyStore`/`PropertyValue`.
- `paint.hpp`: `paint`, `erase`, `paint_triangle`, `copy_paint` with `PaintOptions`.
- `hierarchy.hpp`: `place`, `remove_use`, `array_element_name`, `update_bounding_boxes`, `flatten_to_new`, `flatten_in_place`, `walk_hierarchy`.
- `commands.hpp`: the golden-case command subset (D8).

### CELL — `hefesto/cell/`
- `search_path.hpp`: `SearchPaths`, `PathVariables`, `split_search_path`, `expand_entry`, `find_cell_file` → `FoundFile`, `instance_path_for_write`, `discover_library_paths`.
- `mag_io.hpp`: `CellIoContext`, `read_mag`, `load_cell`, `write_mag`, `save_cell`, `kExtensionPrefix`.
- `io_safety.hpp`: `FileLockService`, `make_in_process_lock_service`, crash backup write/read.
- `session.hpp`: `CellSession` (per-session CELL state, held by `EditorState::cell_session`), `BackupTimerHost`.
- `commands.hpp`: `load`, `save`, `writeall`, `path`, `addpath`, `locking`, `crashbackups`.

### VIEWER — `hefesto/display/`, `hefesto/view/`, `hefesto/render/layout_scene.hpp`
- `display/styles.hpp`: `ColorMap`, `parse_color_map`/`write_color_map`, `DisplayStyle`, `StyleTable` (fixed slots, layout/pale indices), `Stipple`, `parse_dstyle`, `find_style_files`, `parse_glyphs`, `TypeStyleMap`, `resolve_type_styles`, `StyleSet`, `load_style_set`.
- `display/font.hpp`: `OutlineFont`, `parse_outline_font`, `FontRegistry`, `font_label_corners`, `font_label_polygons`.
- `display/scene.hpp`: `ResolvedStyle`, `LayoutScene` (layers in redisplay order, cells, labels), `SceneOptions`, `build_scene`, `LayerSwatch`, `layer_palette`.
- `view/window.hpp`: `ViewMapping` (Magic's integer view transform), `LayoutWindow` (root use, caption, expansion, visibility, grid, crosshair), `WindowManager` (ids, front-to-back, current window, observers), `ViewObserver`, `Change`.
- `view/commands.hpp`: core view commands (D5).
- `render/layout_scene.hpp`: `to_batches(LayoutScene)` → `LayoutBatches` (GPU rect batches + layer styles).

### SHELL — `hefesto/shell/`
- `theme_tokens.hpp` (M0) and `design_tokens.hpp`: type scale (`type_spec`), font files, spacing, radius, elevation, motion, density, `StatusKind` with `status_glyph`/`status_color`.
- The screen is built in the shell's Raven layer (`src/raven_ui.cpp` stays the only Raven includer) from core data: `editor::EditorState` (windows, cells, technology, styles) and `display::layer_palette`.

### PDK — `hefesto/pdk/`, `hefesto/app/`
- `pdk/catalog.hpp`: `Family`, `Variant`, `Selection`, `PdkVersion` (`short_form`, `sort_versions`), `Asset`, `Catalog` (`builtin`, `resolve`, `resolve_libraries`), manifest parsers.
- `pdk/manager.hpp`: `Fetcher`, `InstallHost`, `sha256_hex`, `InstalledVersion`, `PdkManager` (`resolve_root`, `remote_versions`, `installed`, `install`, `enable`, `uninstall`, `tech_file`), `make_managed_pdk_store`.
- `pdk/commands.hpp`: `pdk ...`.
- `app/settings_stack.hpp`: `SettingLayer`, `SettingsStack` (`effective` with the supplying layer).
- `app/project_file.hpp`: `ProjectFile`, `PdkPin`, `to_text`/`parse_project`/`load_project`/`save_project`.
- `app/startup.hpp`: `StartupOptions`, `parse_startup_options`, `StartupEnvironment`, `startup_files`, `run_startup`.

### Shared (INTEGRATION)
- `editor/editor_state.hpp` (`EditorState`), `cmd/all_commands.hpp` (`register_all_commands`, `require_editor`), `diag/codes.hpp` (`CodeArea`, `code_areas`, `render_code_table_markdown(area)`, `not_implemented`).

## Risks / Trade-offs

- **Interfaces may need reshaping** once implementations land (e.g. MODEL may need undo hooks, CELL may need more `CellDef` fields). → Changes to an area's own headers are free; changes that ripple into other areas go through INTEGRATION with an open item naming the callers.
- **Golden expectations need CI capture.** → Areas develop against unit tests derived from the spec scenarios and Magic's source; INTEGRATION captures and replays.
- **Raven v0.2.2 limits** (fonts, SVG icons, shadows, rulers) may block exact mockup fidelity. → Closest faithful rendering plus a recorded gap (D6); Raven changes are proposed upstream, not patched locally.
- **Stub registration hides missing behavior.** → Every stub fails loudly with `CORE-NOTIMPL-001`; INTEGRATION's exit check greps for remaining uses.

## Migration Plan

No user data exists yet. The CLI's `-T` warning `CLI-TECH-001` is replaced by real technology loading (PDK workstream, through `app::run_startup`); INTEGRATION retires the code in the code table (kept as retired, never reused).

## Open Questions

None blocking. Spec/mockup differences found while building the UI are recorded in `docs/design/m1-ui-conformance.md` and reported as open items.
