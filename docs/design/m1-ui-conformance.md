# M1 UI conformance

How the M1 desktop shell matches the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the reference mockups in [mockups/](mockups/) (`Main.dc.html` for tokens and components, `Desktop-Layout.dc.html` for the layout editor screen). Rules: `openspec/changes/m1-core-viewer/design.md` D6.

Every row names one mockup element, how the shell implements it, and any gap with its reason. Where Raven v0.2.2 cannot match the mockup, the shell does the closest faithful thing and the gap is recorded here. A gap that needs a Raven change is also reported as an open item.

Owner: SHELL. Status values:

- **done**: seen in a capture and matching the mockup within the noted tolerance.
- **partial**: built, with a gap recorded.
- **later**: present but disabled; the function arrives in the milestone named.
- **blocked**: built in the shell, but the core function it needs is not available yet.

## How this was checked

Second pass, 2026-11-08, macOS arm64 (Apple M-series, MoltenVK, Retina, 1440×900 logical), build `build/macos-bench` (Release, `-Werror`). Every capture is the product app with a real `.mag` file opened through the core `load` path (CELL), no scratch harness:

```
CAD_ROOT=<dir with magic/sys = Magic's scmos/ mos.OpenGL.* styles, graphics/*.glyphs and Free*.pt3 fonts> \
hefesto -T sky130A/libs.tech/magic/sky130A.tech sky130A/libs.ref/sky130_fd_sc_hd/mag/<cell>.mag [script.tcl] \
        --theme dark|light --width 1440 --height 900 --capture FILE.ppm --capture-frames 30
```

Cells: `sky130_fd_sc_hd__inv_1` (dark and light), `sky130_fd_sc_hd__dfxtp_1` (dark; light after `snap internal`, `see no dnwell`, `zoom 0.5`), `sky130_fd_sc_hd__macro_sparecell` (hierarchical: 7 instances; captured after expanding one `inv_2` and one `nand2_2` with the hierarchy row's commands), the inverter without `CAD_ROOT` (light; failure path) and the PDK manager against a copy of the local `.cache/pdks/ciel` root. Each PNG was read and compared by eye with `Desktop-Layout.dc.html` (sizes and colors read from its inline styles). Nothing below is claimed from code reading alone, except where a row says "not captured".

The first pass (earlier the same day) used a scratch harness because `load` was still a stub; its findings are superseded by this pass.

### UI review findings fixed in this pass

| Finding | Fix | Verified by |
|---|---|---|
| Canvas empty-state hint used the light theme's `text_secondary` (#4E5663) on the always-dark canvas (#0B0C0E), about 2.6:1 | everything the canvas overlay draws (rulers, hint, zoom control, minimap, labels) uses `canvas_overlay_tokens()` = the dark tokens, in both themes | unit test "text drawn on the canvas keeps 4.5:1 in both themes"; light capture without display styles shows the hint light-on-dark |
| Failures outside the command bar were invisible (only appended to the collapsed Diagnostics tab; status stayed "ready") | `ProblemSink` on the Dispatcher: warnings and errors set the status message to shape + text (`▲ …` / `■ …`), a count chip (`■ 2 errors · ▲ 1 warning`) sits left of the DRC state, and an error outside the command bar opens the Console area on the Diagnostics tab | unit test "problems outside the command bar are counted and shown with their shape"; capture without `CAD_ROOT` (status `■ error: DISPLAY-CMAP-001 …`, chip `■ 2 errors`, Diagnostics tab open) and PDK capture (`■ 1 error`) |
| Left-ruler labels (`-500`) overflowed the 20 px ruler onto the canvas | both rulers are clipped to their band; left labels use a compact form (`.5`, `-1.5k`) and are stacked one character per line when they still do not fit (2 characters of JetBrains Mono 11 in 16 px) | unit test "left ruler labels are compact and never wider than the ruler"; captures (`-2`, `-5` fit) |
| (found in this pass) The overlay painted outside the canvas: the box dashes, contact crosses and labels of a zoomed-in cell crossed the tool rail and left panel (Raven does not clip a painted widget to its bounds) | the overlay pushes a clip to the canvas bounds | dfxtp_1 light capture at 200 % before and after; no automated test (painter-level; Raven has no offscreen painter in unit tests) |
| (found in this pass) Leaf instances could not be expanded from the hierarchy (only instances with children had ▸), although Magic shows an unexpanded leaf as its bounding box only | every instance row and every grouped row (`▸ inv_2 ×2`) is expandable | unit tests on `hierarchy_rows`; sparecell capture |

### Second UI review (contrast, clipping, accessibility)

These fixes are unit-tested; they were made after the captures above and were **not re-captured** (the PNGs listed above predate them).

| Finding | Fix | Verified by |
|---|---|---|
| Light status chips below 4.5:1 (DRC/LVS info text #1F6FB8 on its 14 % tint: 4.19:1; success 4.24, danger 4.00) | light `*_text` tokens darkened: success #12693F, warning #7A5400, danger #B2302D, info #1A5E9E (5.3-5.6:1 on chips, 4.8-5.4:1 on the selected row); `status_text_color()` and `kChipTintAlpha` shared by every painter | unit test "status text keeps 4.5:1 on chips and selected rows" (every status in both themes, on the chip tint over the bar, `row_selected`, `input` and `surface1`) |
| PDK selected row: dark subtitle `text_tertiary` on `row_selected` 4.24:1; light state labels 4.1-4.5:1 | the subtitle uses `text_secondary` on a selected or hovered row; the darker light status tokens above | same unit test (`text_secondary` on `row_selected`) |
| PDK checkbox outline 1.9:1 (dark) / 1.6:1 (light), below WCAG 1.4.11 | unchecked boxes are stroked with `text_tertiary` (5.8-6.0:1 on `input`) | unit test "checkbox outline meets 3:1 non-text contrast". **Design-artifact item:** the mockup's control-border shade is not usable as a checkbox outline. |
| Status bar and Diagnostics cut errors with no way to read them | the status message is ellipsized; the bar's tooltip holds the full message and clicking (or Enter on) the bar opens the Diagnostics tab; Diagnostics lines wrap (continuation indented 4) at the console's width for the minimum window size; the PDK root is ellipsized with the full path in the header tooltip, and a root error gets its own wrapped line in `danger_text` with its ■ shape | unit test "ellipsize and wrap keep whole code points"; painting not captured |
| Status bar invisible to screen readers | the status bar's accessible name follows its text (`Status: <message>. <problems>. <DRC>`); errors are announced assertively and warnings politely (`Raccessibility_announce`), both outside the command bar and for a failed command line | unit test "accessible names carry state and problems are announced"; not verified with VoiceOver |
| Pressed / checked / expanded states never reached assistive technology | Raven 0.2.2 has no accessible-state API, so the state is in the name: "Box tool, pressed", "Console and diagnostics, pressed / not pressed" (refreshed on toggle), PDK toggles use the checkbox role and read "…, checked / not checked", hierarchy rows end with ", expanded / collapsed / partly expanded" | same unit test and the hierarchy tests; not verified with VoiceOver. **Open:** move the state to a Raven accessible-state API when one exists. |
| Hierarchy clipping hid ×N and instance ids, so groups read as single instances | `hierarchy_row_layout()`: only the name part is ellipsized and the suffix (` ×2`, ` (C1) [4×2]`) is drawn right after it | unit test "hierarchy rows keep their suffix visible" |
| A partly expanded group showed ▸ like a collapsed one | a group with some instances open reads `▸ inv_2 ×2 · 1 open` (accessible: "partly expanded"); clicking it expands the rest | unit test "a partly expanded group says so" |
| Layers panel said "Load a technology with display styles" when the technology was loaded but its styles were missing | `layer_empty_hint()`: "No display styles found for sky130A (see Diagnostics)"; the old text stays for the minimum technology | unit test "layer panel empty text names the cause" |
| Inspector title/subtitle ran past the 14 px padding | ellipsized to the padded width; the full title and subtitle are the panel's tooltip | painting not captured |

## Tokens and foundations (Main.dc.html)

| Mockup element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Color tokens, graphite dark theme by default | `theme_tokens.hpp` → `apply_tokens()` sets every Raven color role (`raven_ui.cpp`), plus the painted widgets | done | Captured in dark. |
| Light theme | `theme light` command; `--theme light` | done | Captured. Panels, bars and painted widgets re-tint; Raven's own widgets (inputs, tabs, list panels) follow the theme roles. |
| Single ember accent | `ThemeTokens::accent`; focus ring, active tool, primary button, `%` prompt | done | |
| Canvas `#0B0C0E` in both themes | `ThemeTokens::canvas`; the renderer clear color, the dot grid and every overlay element (`canvas_overlay_tokens()`) use the dark values | done | Captured in light: canvas, rulers, zoom control, minimap and hint stay dark-themed; contrast unit-tested for both themes. |
| Semantic colors success/warning/danger/info | `ThemeTokens` | done | |
| Accent hover `#FFA866` | `ThemeTokens::accent_hover` (dark: mockup value; light: derived) | partial | **spec/mockup difference:** not in the spec's token table; the pressed shade is derived. |
| Extra surface shades (`#15181C` bars, `#2A2F37` selected row, `#3D434E` control border, `#0F1114` inputs/status bar, `#8A919D` tertiary text, `#C9CED6` icons, `#FFB27A` soft accent, chip text colors) | `ThemeTokens::bar`, `row_selected`, `control_border`, `input`, `text_tertiary`, `icon`, `accent_soft`, `*_text` | partial | **spec/mockup difference:** used by the mockup, not named in the spec's token table. Dark values are the mockup's; light values are derived to keep WCAG 4.5:1, including the status text on its 14 % chip tint and on the selected row (unit-tested). To be added to the design artifact and the spec. |
| No color literal outside the token table | unit test "the shell's drawing code has no color literals" scans the Raven layer and the painters | done | Layer colors come only from the PDK display styles; white selection/box is the spec's canvas rule. |
| Type scale (display 28/700 … mono 12) | `design_tokens.hpp` `type_spec()`, `text_spec(TextRole)` for every role on the screen | done | Unit-tested; captured. |
| Inter + JetBrains Mono bundled as data files (OFL-1.1) | `assets/fonts/` (Inter 4.1: 400/500/600/700; JetBrains Mono 2.304: 400/500, OFL texts beside them), loaded at startup by `Runtime::load_assets` from `assets/` next to the executable, `../Resources/assets`, `../share/hefesto/assets`, `--asset-dir`, or the source tree | done | Captured: UI text in Inter, command bar, hierarchy, inspector values, status bar and rulers in JetBrains Mono. `--font-dir` is gone. Raven still compiles Roboto into its library (M0 go/no-go finding); it is only a fallback when the files are missing. The notices entry is still in "to be added" (open item). |
| Label tracking 0.06–0.08 em, caps | `Painter::text` draws caps glyph by glyph with 0.08 em | partial | Raven has no letter-spacing; per-glyph drawing loses kerning in caps labels (not visible at 11 px in the captures). |
| Spacing 4/8/12/16/24/32 | `kSpacing`; Raven metrics padding 8/4, spacing 8, indent 16; panel paddings 14 px as in the mockup | done | 14 px panel padding is the mockup's value (not on the 4 px scale; mockup wins for screen structure). |
| Radius 4 px controls / 8 px panels | `kRadiusControl`, `kRadiusPanel`; Raven `CORNER_RADIUS` 4, `POPUP_RADIUS` 8 | partial | **spec/mockup difference:** the mockups draw 6 px control radii; the spec value (4 px) is used. |
| Panels: 1 px border, no shadow | painted 1 px separators in `border` | done | |
| Popovers: shadow 8–12 px offset, 24–32 px blur, 45–55 % black | Raven `R_COLOR_SHADOW` 50 % black; the minimap draws a stacked-rectangle approximation | partial | Raven's painter has no blur: the minimap shadow is four translucent offset rectangles. Raven's own popups (tooltips, menus) use Raven's shadow. |
| Controls 28–32 px tall | inputs 30 px, command input 28 px, buttons 32 px, tool buttons 36 px (mockup), chips 26 px (mockup) | done | |
| Motion 120 / 200 ms, none with reduced motion | `Raccessibility_set_reduce_motion(system setting)` | partial | The shell has no animations of its own; Raven's animations follow the system reduce-motion setting. Not captured (static captures). |
| Status shapes ● ▲ ■ ◆ always with text | `StatusChip::label()`, `diagnostic_line()`; chips, status bar DRC, Diagnostics panel lines, Console failures, PDK states, hidden layers ("hidden") | done | Unit-tested and captured. |
| Focus ring in ember | `R_COLOR_FOCUS` = accent; painted widgets stroke 2 px accent when focused | partial | Not captured (no keyboard focus in a capture). |
| Icons: 1.6 px stroke, 24 px grid | `assets/icons/*.svg` rasterized by Raven at 3× | partial | Raven rasterizes SVG to a texture; icons are tinted per state. Some icons are drawn by Hefesto (not the mockup's exact glyphs). |

## Desktop layout editor (Desktop-Layout.dc.html)

| Mockup element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Top bar 48 px, `bar` background, bottom border | Raven border panel + painted separator | done | |
| App mark 26 px | painted from the mockup's 44-unit drawing | partial | Drawn with rectangles and lines (no SVG path rendering in the painter); the shape is simplified. |
| Cell name (13/600) + `PDK · version · magic-compat` (11, tertiary) | `header_text()` from the window root / edit cell, the technology name and `version` statement, `ProjectSettings::compat.magic_compat` | done | Captured: "inverter_demo / sky130A · 1.0.493 · magic-compat on". The PDK name is the technology name; the version comes from the tech file's `version` line, not yet from the project pin (PDK workstream). |
| Layout / Schematic / 3D / Waveforms segmented control | painted; Layout selected, others in tertiary text with tooltips naming M8 / M10 | later | Only Layout works in M1. The other tabs are not separately focusable (one painted widget). |
| DRC / LVS chips with shape | `drc_chip_m1()` / `lvs_chip_m1()`: "◆ DRC not run", "◆ LVS not run" | later | DRC arrives in M3, LVS in M9; the mockup's "● DRC clean" state is shown once DRC exists. |
| Extract, Simulate, Ask agent | painted buttons, disabled (45 % opacity), tooltips naming M7 / M9 / M11; Ask agent primary | later | |
| Open, PDKs buttons | painted secondary buttons with icons (not in the mockup) | partial | **mockup gap:** the mockup has no file-open or PDK entry point; added to the right of the chips. Open runs the system dialog, then `load <path>` through the Dispatcher. To be added to the design artifact. |
| Tool rail 52 px, 36 px buttons, 6 px gap, active tool ember-tinted | painted tool buttons from `tool_rail()`; Box active; Zoom to fit runs `view`; Select, Paint, Wire, Label, Measure, Place device disabled (M2) | done | Every button has an accessible name and tooltip. The rail adds "Zoom to fit" at the end (not in the mockup). |
| Left panel 240 px, surface1, right border | `LeftPanel` | done | |
| "LAYERS" heading and "Filter layers" input (30 px) | painted section label; Raven text input filtering by name or plane (`layer_rows`) | done | The input is Raven's text input with the forge roles (6 px mockup radius → 4 px spec radius). |
| Layer row 32 px: swatch 18×14 (PDK color, stipple, outline), name, plane | painted rows from `display::layer_palette` → `layer_rows()`; fill at reduced opacity, outline in the PDK color, stipple hatching, dashed outline for outline-only styles, cross for contacts | done | Captured with sky130A: colors from `mos.OpenGL.std.cmap`/`.dstyle`. Swatch stipples are drawn as diagonal hatching, not the style's exact 8×8 stipple bits. |
| Hidden layer row ("hidden", tertiary text) | `LayerRow::meta()`; clicking a row toggles it with `see` / `see no` through the Dispatcher | done | Captured (dfxtp_1 light after `see no dnwell`: "dnwell  ⊘ hidden", tertiary text). |
| Active paint layer (selected row, ember outline, "paint") | — | later | The paint layer arrives with editing (M2). |
| Lock icon per layer | — | later | Active (locked) layers UI in M2. |
| "HIERARCHY" tree (JetBrains Mono 12, ▾ / ▸, ids, ×N groups, arrays) | `hierarchy_rows()` from the window root; clicking a row runs `box values <instance bbox>i; expand surround` (or `unexpand surround`), once per instance of a grouped row, through the Dispatcher (origin `gui`, logged) | partial | Captured with `sky130_fd_sc_hd__macro_sparecell`. **mockup difference:** the mockup shows grouped leaf rows without ▸ ("via1_2x1 ×4"); in Magic an unexpanded leaf shows only its bounding box, so every instance row can expand and carries ▸ / ▾. Gaps: the box moves to the instance (Magic's `expand` acts on the box; there is no M1 command naming one instance, and `select cell` arrives in M2); `expand surround` also expands every instance lying wholly inside that box at any depth (Magic's DBExpandAll), so a hierarchy row expands its whole subtree; long names are ellipsized while the ×N count or instance id stays visible (the full text is in the tooltip); a group shows ▾ only when all of its instances are expanded and `· N open` when only some are. |
| Canvas: Vulkan viewport with PDK layer styles | `LayoutCanvas`: `display::build_scene` → `render::to_batches` → `CanvasRenderer`, redisplay order from the scene; contact crosses (`cross` styles) drawn by the overlay from `LayoutScene::layers` in the style's color | partial | Captured with sky130 inv_1, dfxtp_1, macro_sparecell: fills, stipples, outlines, subcell boxes from the renderer; contact crosses from the overlay; pale styles for expanded instances outside the edit cell. Not drawn: non-Manhattan triangles and outline dash patterns (renderer bridge, VIEWER). Crosses are 1 px overlay lines over the GPU layers, so they sit above layers Magic would draw after the contact. |
| Dot grid (20 px, `#2A2F37`) | painted overlay, fixed to world coordinates | done | |
| Rulers on the top and left edges (20 px, JetBrains Mono 10) | painted, 1-2-5 steps in microns (`ruler_ticks`), each ruler clipped to its band, left labels compact and stacked when needed | partial | Captured. Labels use the 11 px mono role (the spec scale has no 10 px), so a 3-character left label ("2.7" in the mockup) is stacked vertically; the mockup's 10 px fits 3 characters on one line. Steps are at least 120 px apart, so a standard cell shows whole microns rather than the mockup's 0.69 steps. |
| Labels on the canvas | label rectangles and port lines from the renderer; text in the overlay from `LayoutScene::labels` in the label's PDK style: plain labels placed by their justification as Magic's DBWdisplay/GrPutText do (anchor on the named side, TEXTOFFSET 5 px), font labels (`flabel … FreeSans`) filled from their glyph outlines with the nonzero rule | partial | Captured (inv_1: `A`, `Y`, `VPWR`, `VGND` font labels; `VPWR`/`VGND` port text; the `inv_1` comment label). **mockup difference:** the mockup draws labels in white 13/500 JetBrains Mono; the spec's rule "layer colors come from the PDK" and Magic faithfulness win: label colors are the PDK label styles. Overlapping flabels of the PDK cells (`VPWR`/`VPB` at one spot) overlap as in Magic. Plain-label text uses JetBrains Mono 12 instead of Magic's small/medium/large raster fonts; font labels are not rotated (`rotate` ignored) and are filled pixel row by row. |
| Subcell outlines and names | outlines from the renderer (`cell_box` batches); definition name at 2/3 and instance id at 1/3 of the height in the PDK cell style (15), as DBWdisplay | partial | Captured (macro_sparecell): outlines drawn; captions are omitted when they do not fit the cell (Magic's GrPutText with `adjust`), which is the case for every sky130 cell at fit zoom. Captions use JetBrains Mono 11, not Magic's large raster font. |
| Selection: white outline + dashed bbox | — | later | Selection arrives in M2. |
| Box: white 1 px dashes (4/4) | painted from `editor.box` | done | Captured. |
| Hover info card ("locali · net Y") | — | later | Needs selection and nets (M2, M7). |
| Minimap 168×112, bottom-right, 16 px inset, view rectangle in white | painted overlay (`minimap_geometry`); click centers the view | partial | Captured. Shows the cell's bounding box, not a thumbnail of its layers (no offscreen render in M1). |
| Zoom control (− / 400% / +), bottom-left at 36 px | painted in the dark tokens; − / + run the core `zoom 2` / `zoom 0.5`, the percentage (relative to fit) runs `view` | done | Captured (100 %, 200 %). |
| Keys and gestures | `CanvasController`: keys issue `pan`, the core `zoom` (`zoom 0.5` in, `zoom 2` out, Magic's direction) and the core `view` (origin `key`); drag and wheel move the camera, written back to the window's `ViewMapping` | partial | The shell placeholders are gone: `register_all_commands` registers the core `view` and `zoom` (design.md D5) and the canvas follows the window's mapping. Unit-tested; `zoom 0.5` captured (200 %). Keys and gestures themselves are not captured (interactive). `pan` (framebuffer pixels) and `theme` stay shell commands. |
| Inspector 284 px, surface1, left border | `InspectorPanel` | done | |
| Inspector content: section, title (15/600), subtitle, key/value grid (JetBrains Mono values) | `inspector_view()`: the box ("BOX", width, height, corners in microns) or the edit cell ("CELL") | partial | Captured ("1.760 um", "-0.190, -0.240"). Selection details (M2), the Layer select and "Select net" / "Show in 3D" buttons (M2, M10) are not shown. |
| "Design rules nearby" card | painted card: "◆ Arrives with DRC (M3)" | later | |
| Command bar 40 px: `%` prompt (ember, JetBrains Mono 12/500), input (JetBrains Mono 12, 28 px), hint text | painted prompt, Raven text input with history (Up/Down), painted hint "⏎ run · ↑ history" | partial | Tab completion is not implemented, so the hint omits "tab complete". A "Console" toggle is added at the right (not in the mockup). |
| Status bar 26 px, JetBrains Mono 11: cursor x/y, box size, edit cell, snap/units, DRC state with shape | painted from `status_bar_text()`: units in microns from the technology's first `cifoutput` style (`scalefactor / (100 × expander)`; sky130A: 0.01 um per lambda), the snap mode read from the `snap` command's session setting, then the message, the problem count and the DRC state | partial | Captured ("box 1.760 × 3.200", "snap: internal · units: um" after `snap internal`). The message always carries its shape (`◆ ready`, `● <result>`, `▲ <warning>`, `■ <error>`) and the shape's text color; it and the problem chip are not in the mockup (**mockup gap**). Technologies without `cifoutput` stay in lambda. The problem count is cumulative for the session (no reset yet). The cursor field was "x – y –" in every capture (no pointer in a capture). |
| Console and Diagnostics panels (M0 additions) | Raven tab view under the canvas (160 px), collapsible; JetBrains Mono; diagnostics and failures carry their shape | partial | **mockup gap:** not in the mockup. Raven's tab and list widgets with the forge roles. Captured. |
| PDK manager panel | `PdkPanel` replaces the Inspector (320 px): catalog variants with state (shape + text), PDK root, version hash input, license / unverified acceptance, Browse versions, Install, Enable, Pin to project, Uninstall, Refresh, each running a `pdk` command through the Dispatcher | partial | Captured with `PDK_ROOT` = a copy of `.cache/pdks/ciel` and a script running `pdk list` and `pdk enable sky130 0fe599b2…`: every variant shows "◆ not installed", because that cache was fetched file by file (`tests/data/pdk/fetch-test-pdk.sh`) and has no install record, and `pdk enable` fails with `PDK-INSTALL-004`, now visible in the status bar and the Diagnostics tab. **mockup gap:** no mockup for this panel. Open: no install was run end to end (needs network or a local release-asset source), installs show no progress bar (the command reports when done), the panel's buttons were not clicked in a capture (they call the same `ctx.run` path as the command bar). |
| Resizable / dockable panels | — | partial | The M0 dock area is replaced by the mockup's fixed panel widths; panels cannot be resized or undocked in M1. |
| Window title, minimum size | "<cell> — Hefesto", 1024×640 minimum | done | |

## Startup flow (app-startup, UI parts)

| Element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Magic-compatible words (`-T`, `-r`, `-norcfile`, `-d`, `-m`, `-rcfile`, cell files) | passed by `parse_shell_options` to `app::parse_startup_options`; a bad option prints Magic's text and exits 1 before any window | done | Unit-tested (options). |
| Startup sequence | `app::run_startup` on the window's session after the window opens; problems go to the Diagnostics panel | done | Captured with `-T sky130A.tech`: technology and layer palette loaded. |
| Display styles | loaded by the shell when the technology changes (`display::load_style_set`, system path `$CAD_ROOT/magic/sys` and the tech file's folder) | partial | Magic's `mos.*` style files, `color.glyphs` and the `Free*.pt3` fonts are not bundled; `CAD_ROOT/magic/sys` must hold them. Without them the palette says "No display styles found for <technology> (see Diagnostics)" and the failure now shows in the status bar (open item: where these files come from). The style failure is reported twice (once per technology object the startup creates). |

## Accessibility

| Element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Icon-only buttons carry accessible names | every tool and top-bar button sets a name and tooltip; unit test on `tool_rail()` | done | |
| Accessible roles | painted widgets declare roles (button, toggle, checkbox, list item, tree item, heading, status bar, pane) | partial | Not verified with VoiceOver. |
| Accessible state (pressed, checked, expanded) | in the accessible name, refreshed on change (`toggle_accessible_name`, `checkbox_accessible_name`, `HierarchyRow::accessible_name`) | partial | Raven 0.2.2 has no accessible-state API (only name, description, role, hidden), so the state is spoken as part of the name. Not verified with VoiceOver. |
| Problems announced | status bar name follows its text; errors assertive, warnings polite (`Raccessibility_announce`) | partial | Unit-tested text; not verified with VoiceOver. |
| Layer rows read "metal1, plane metal1, visible" | `LayerRow::accessible_name()` | done | Unit-tested. |

## Remaining gaps (open items)

- Hierarchy expansion moves the box and expands the whole subtree under it; a one-instance core command (or `select cell` in M2) is needed for a one-level toggle.
- Overlay clipping has no automated test (verified by capture only).
- Labels: no rotation for font labels; plain labels use JetBrains Mono, not Magic's raster fonts; contact crosses are drawn above all GPU layers.
- Subcell captions are hidden whenever they do not fit (most standard cells at fit zoom).
- Problem count never resets during a session.
- Display styles, glyphs and fonts must come from a Magic installation through `CAD_ROOT`.
- PDK manager: no end-to-end install, no progress bar, buttons not exercised in a capture.
- Raven has no accessible-state API: pressed / checked / expanded states are carried in accessible names until it does.
- The second UI review's fixes (contrast tokens, ellipsis, wrapping, accessible names) are unit-tested but not re-captured.
- Not captured: keyboard focus ring, hover states, reduced motion, cursor coordinates, VoiceOver.
- Radii (4 px spec vs 6 px mockup), label tracking, popover blur, icon glyphs: as in the rows above.
- `src/core/include/hefesto/view/commands.hpp` (VIEWER) still says the shell registers `view`/`zoom`; its comment needs updating by its owner.
