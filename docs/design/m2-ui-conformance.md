# M2 UI conformance

How the M2 desktop editor matches the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the M2 boards of the reference design artifact, drafted under [mockups/](mockups/) (`openspec/changes/m2-interactive-editing/design.md` D8). Same format and rules as [m1-ui-conformance.md](m1-ui-conformance.md), which stays the record for the M1 screens.

Every row names one board element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally).

Owner: SHELL (seeded by the M2 scaffold, task 1.5; completed in task 10.9). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **later**: present but disabled; the function arrives in the milestone named.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-002` stub during M2).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1.

## How this was checked

First SHELL pass, 2026-11-10, macOS 27 arm64 (Apple M3 Pro, MoltenVK 1.4.2, Retina 2×, 1600×1000 logical), build `build/macos-bench` (Debug, `-Werror`, desktop on), branch `m2-round2`. Every capture is the product app on `sky130_fd_sc_hd__dfxtp_1.mag` opened through the core `load` path, driven by `--run` so that every state is reached through the command API:

```
CAD_ROOT=<dir with magic/sys styles> \
hefesto -T sky130A/libs.tech/magic/sky130A.tech sky130A/libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__dfxtp_1.mag \
        --width 1600 --height 1000 --run '<commands>' --capture out.ppm --capture-frames 6
```

| Capture | `--run` | Board |
|---|---|---|
| 01 highlights | `box 0 0 50 40; select area; box 100 100 300 200` | Selection, Tool states (box) |
| 02 select | `box 0 0 50 40; select area; tool pick` | Tool states (Select active), Selection (mixed inspector), Layer palette (locked rows) |
| 03 wiring | `tool wiring; setpoint 976 568; trackwire pick; setpoint 976 1100; wire show` | Wiring, Tool states (Wire active) |
| 04 multi-line | `help box` | Command bar (status summary) |
| 05 flush dialog | `box 0 0 10 10; paint m1; flush` | Prompt dialogs |
| 06 light | `--theme light`, `box 0 0 50 40; select area; panel layers hide` | every board, light theme; Panels (collapsed Layers) |

Pointer and key input cannot be driven by `--run`: it is covered by unit tests that feed the canvas's translated Raven events through a real core session (`tests/unit/shell/input_router_test.cpp`); a live session with OS events (RAVEN-CHECK's tools, task 10.1) is still to be repeated on this build. The captures are kept in the session scratchpad (`m2-shell/`), not in the repository.

## M2 boards (DESIGN)

Owner of this section: DESIGN (task 2.1). The boards are drafted in [mockups/](mockups/) (`M2-*.dc.html`, listed in [mockups/README.md](mockups/README.md)) and rendered in both themes to `docs/images/ui/` (`<board>.png` dark, `<board>-light.png` light). The user copies them into the reference design artifact before the usability review (task 2.3). Compare captures with the board named in each section below; a board's "Notes for SHELL" say which command each control issues.

| Board | File | Renders | SHELL tasks | Sections of this record |
|---|---|---|---|---|
| Editing tools — rail and canvas states | [M2-Tool-States.dc.html](mockups/M2-Tool-States.dc.html) | [dark](../images/ui/m2-tool-states.png) · [light](../images/ui/m2-tool-states-light.png) | 10.2, 10.3 | Editing tools on the rail and canvas |
| Selection highlight and inspector | [M2-Selection.dc.html](mockups/M2-Selection.dc.html) | [dark](../images/ui/m2-selection.png) · [light](../images/ui/m2-selection-light.png) | 10.5 | Selection and inspector |
| Layer palette — paint layer, visibility, lock | [M2-Layer-Palette.dc.html](mockups/M2-Layer-Palette.dc.html) | [dark](../images/ui/m2-layer-palette.png) · [light](../images/ui/m2-layer-palette-light.png) | 10.5 | Layer palette |
| Command bar — completion, history, multi-line results | [M2-Command-Bar.dc.html](mockups/M2-Command-Bar.dc.html) | [dark](../images/ui/m2-command-bar.png) · [light](../images/ui/m2-command-bar-light.png) | 10.4, 10.8 | Command bar; M1 gaps (multi-line results) |
| Undo and redo | [M2-Undo-Redo.dc.html](mockups/M2-Undo-Redo.dc.html) | [dark](../images/ui/m2-undo-redo.png) · [light](../images/ui/m2-undo-redo-light.png) | 10.6 | Undo, color window, … (undo/redo) |
| Prompt dialogs | [M2-Dialogs.dc.html](mockups/M2-Dialogs.dc.html) | [dark](../images/ui/m2-dialogs.png) · [light](../images/ui/m2-dialogs-light.png) | 10.6, 10.8 | dialogs; M1 gaps (crash recovery) |
| Color window | [M2-Color-Window.dc.html](mockups/M2-Color-Window.dc.html) | [dark](../images/ui/m2-color-window.png) · [light](../images/ui/m2-color-window-light.png) | 10.6 | color window |
| Feedback list | [M2-Feedback.dc.html](mockups/M2-Feedback.dc.html) | [dark](../images/ui/m2-feedback.png) · [light](../images/ui/m2-feedback-light.png) | 10.6, 10.8 | feedback list; M1 gaps (problem count) |
| Wiring tool tracking | [M2-Wiring.dc.html](mockups/M2-Wiring.dc.html) | [dark](../images/ui/m2-wiring.png) · [light](../images/ui/m2-wiring-light.png) | 10.2, 10.3 | Editing tools (wire) |
| Dockable and collapsible panels | [M2-Panels.dc.html](mockups/M2-Panels.dc.html) | [dark](../images/ui/m2-panels.png) · [light](../images/ui/m2-panels-light.png) | 10.7 | panels |
| Macro and key-binding reference | [M2-Key-Bindings.dc.html](mockups/M2-Key-Bindings.dc.html) | [dark](../images/ui/m2-key-bindings.png) · [light](../images/ui/m2-key-bindings-light.png) | 10.6 (D8 list) | macro / key-binding reference |

### Coverage of the SHELL screen states (task 2.1 check)

Every M2 screen state of tasks.md section 10 maps to a board:

| SHELL task | Screen states | Board |
|---|---|---|
| 10.2 | box/selection drags and move/copy previews as highlights; select, wiring and pick tools | Editing tools (tiles Box, Select), Wiring tool |
| 10.3 | live rail with active tool, cursor glyphs, SVG icons, disabled place device | Editing tools (rail, button states, glyph strip) |
| 10.4 | history popover, Tab completion of commands/cells/layers, multi-line results with Console expander | Command bar |
| 10.5 | inspector for paint, label, port, instance, mixed; palette paint layer and lock toggles; hierarchy through `expand selection` | Selection and inspector, Layer palette (hierarchy: no new board; the M1 board's hierarchy panel applies, the Expand action is on the instance inspector) |
| 10.6 | undo/redo affordances, color window, feedback list, PromptHost dialogs (crash recovery, `flush`, unsaved color map, `quit` with modified cells) | Undo and redo, Color window, Feedback list, Prompt dialogs |
| 10.7 | dockable and collapsible panels, light/dark on every new control, accessible names | Dockable panels; every board in both themes |
| 10.8 | problem count reset, multi-line status, crash dialog | Feedback list (count resets with the list), Command bar (status bar expander), Prompt dialogs |
| 10.8 | minimap outline/cross styles and cell boxes, top ruler in JetBrains Mono 10 | no new board: the M1 `Desktop-Layout.dc.html` minimap and ruler apply |

### Board elements that have no row yet

SHELL adds a row for each when it builds the screen (task 10.9):

- Tool rail: tooltip with the tool's key (`Space` cycles, `^Space` wiring), the five button states (default, hover, active, focus, disabled).
- Paint, Label and Measure tools as box-tool modes (canvas tiles 3, 5, 6 of the tools board).
- Wiring tracking: preview leg outline with 18 % layer tint, HUD next to the cursor, "Wiring · tracking" mode chip, Wire inspector (type, width ±, technology layer stack).
- Layer palette row context menu showing the command of each item.
- Undo history popover (`undo N` / `redo N`), "undo off" chip, status bar "Undid …" message with Redo.
- Color window chrome: caption `COLOR = 0<octal>`, index stepper (`color last` / `color next`), Load…/Save…, value strip.
- Key-binding sheet: key capture, conflict warning, Reset table, Load file….
- Floating panel header, drop-zone highlight while dragging a panel, collapsed edge strip.

### Board decisions that need confirmation

The boards propose behavior that no spec states yet; each is also an open item of the DESIGN round:

- Paint tool: drag a box, release paints the active palette layer, Option-release erases it (`box values …` then `paint`/`erase`).
- Label tool: click or drag, type in a canvas popover, Enter issues `label TEXT POS LAYER`; Measure tool: drag issues `measure` on release, Esc `unmeasure`.
- Dialog buttons are verbs mapped to the PromptHost answers by index (for example "Discard" = `yes` for `flush`), the default answer has focus, Esc returns the default answer.
- Panel layout (dock, size, collapsed, floating) saved per user in settings and a Reset layout command; F6 cycles focus across docks.
- Undo history popover on press-and-hold or right-click of Undo.

## Editing tools on the rail and canvas

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Tool rail: box, select, paint, wire, label, measure live; place device disabled until M4 | `tool_rail()` (all live but Place device, tooltip "arrives in M4"); a click runs `rail_action()`: `tool box`, `tool pick`, `tool wiring`, `view`; Paint, Label and Measure are modes of the box tool (`InputRouter::set_mode`) | partial | Paint, Label and Measure follow the board decisions still awaiting confirmation (see DESIGN's list). The rail is built from painted widgets, so VoiceOver cannot press it (RAVEN-CHECK gap 1). |
| Active tool in ember | `active_rail_icon(tool, mode)` from `ui::ToolRegistry`; the active button gets the ember tint and its accessible name says "pressed" | done | Captures 02 (Select) and 03 (Wire) match the board's active state. |
| Rail tooltip with the tool's key | tooltips name Shift-Space, Control-Space, Space and what each button does | partial | Raven tooltips are plain text: the key is not drawn as a key cap (board). |
| Cursor glyph per tool (`ui::ToolRegistry`, `cursor`) | `cursor_shape(glyph, mode)` maps Magic's glyph to a system pointer: corner glyphs 2-9 to the diagonal resize / move pointers, wiring and nettool to the crosshair, the Paint and Measure modes to the crosshair, Label to the I-beam | partial | Raven has 10 system pointers and no custom cursor images, so Magic's glyph bitmaps (box corner, pick arrow with box) cannot be drawn. Raven gap. |
| SVG icon set (1.6 px stroke, 24 px grid) under `assets/icons/` | drawn by Raven's SVG rasterizer since M1; `redo.svg` added for the top bar | done | |
| Box drawn as a highlight | the box client of `view::HighlightLayer` in the highlight pass (a second `render::CanvasRenderer` over the layout, `render::highlight_batches`); the M1 painted box overlay is retired | partial | The batch draws the box as a 1-framebuffer-pixel outline: Magic's 2-pixel "thick" sides (`display::box_drawing`) need a pixel-width outline in `render::highlight_batches` (VIEW open item). |
| Selection highlight (white solid outlines) | the selection client in the same pass; highlight text and point crosses are drawn by the overlay | done | Captures 01-03. |
| Move/copy previews | the pick tool's drag runs its `*bypass select move` motion command on each pointer move (tools.tcl's binding), `copy center 0` / `select keep` on the next press, as in Magic | partial | Live drag not yet exercised with OS events. |
| Paint / Label / Measure sweep preview | dashed white rectangle in the overlay while the button is held; the release issues `box values … ; paint L` (Option: `erase L`), opens the command bar with `label `, or `box values … ; measure` | partial | Board decisions pending confirmation; the label canvas popover is the command bar instead. |
| Wiring tracking: preview leg, HUD, mode chip, Wire inspector | the preview leg is the wiring area's selection (capture 03); pointer motion runs `*bypass wire show` while `wire.tracking` | partial | No HUD next to the cursor, no "Wiring · tracking" chip and no Wire inspector yet. |
| `*watch` tile overlay | `view::tile_overlay_scene` appended to the highlight pass (style 19, labels by the overlay) | partial | Not captured yet. |

## Selection and inspector

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Inspector for paint, label, port, instance and mixed selections | `selection_inspector_view()`: SELECTION section, kind title, layers with tile counts, label text/layer/position/port index, instance cell/id/orientation/array, bounding box | partial | Capture 02 (mixed). The board's per-row editable fields are read-only rows; edits go through the action buttons. |
| Inspector actions | buttons that run `select net`, `what`, `delete`, `clockwise 90`, `expand selection`, `edit`, `port make`; those needing an argument (`setlabel text `, `identify `, `port index `) put the command in the command bar | partial | `orient` and `array` setters are not on buttons yet. |

## Layer palette

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Active paint layer | a click on a row runs `palette paint LAYER` (new shell command, logged); ember outline and meta "paint"; footer "Paint layer NAME" with Paint / Erase buttons (`paint L` / `erase L` on the box) | partial | The footer buttons read "Paint" / "Erase": the board's "Paint on box" / "Erase on box" do not fit side by side in the 240 px panel. |
| Visibility toggle | the eye zone of the row runs `see L` / `see no L`; shown on hover and while hidden | done | |
| Lock toggle (locked types) | the lock zone runs `tech lock L` / `tech unlock L`; meta "locked" / "paint · locked" | done | Capture 02: sky130A's inactive types show as locked. |
| Row context menu | — | todo | Raven 0.2.2 menus not wired yet. |
| Keyboard: ↑/↓ moves, Space selects | Tab/Shift-Tab move between rows, Space/Return select the paint layer | partial | Arrow-key navigation inside the list is not implemented; focus returns to the window after a row toggles (RAVEN-CHECK item 3). |

## Command bar

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| History | Up/Down walk the history (M1) | partial | No history popover: Raven 0.2.2 has no anchored popover; the Console keeps the full history. |
| Tab completion of commands, cells and layers (`ui::complete`) | `complete_command_line()`: a unique match completes and adds a space; several extend to the common prefix and are listed in the status bar and the Console | partial | The board's completion popover is a status-bar list (no anchored popover in Raven). Text input arrow keys insert stray characters on macOS (RAVEN-CHECK live-input bug). |
| Multi-line results: first line in the status bar, expander to the Console | `result_line()`: "<first line> (+N lines, see Console)"; clicking the status bar opens the Console tab for a multi-line result, Diagnostics otherwise; `CommandOutcome::printed` collects what commands print (`help`, `what`) | done | |

## Undo, color window, feedback, dialogs, panels

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Undo/redo affordances | Undo and Redo ghost buttons in the top bar (icons `undo`, `redo`), enabled from `undo_affordance()` (log position), tooltips with the step counts; they run `undo` / `redo` | partial | No history popover, no "undo off" chip, no "Undid …" status message yet. |
| Color window screen | — | todo | Not built in this pass (`specialopen color` works in the core). |
| Feedback list | a Feedback tab next to Console and Diagnostics: one row per area (`feedback_rows()`: index, corners, text) | partial | Rows cannot be clicked yet (Raven list rows have no activation callback in the wrapper); `feedback find N` must be typed. |
| Confirmation dialogs through `cmd::PromptHost` (crash recovery, `flush`, unsaved color map, `quit` with modified cells) | `MainWindow::DialogPromptHost` installs in `EditorState::prompt`; `prompt_dialog()` turns a question into a Raven dialog with one Raven button per answer, verbs for known questions ("Discard changes", "Overwrite", "Recover", "Quit without saving", "Cancel"), the default answer primary and focused; Escape gives the default; crash recovery through `CellSession::confirm_recovery` | done | Capture 05 (flush). In a scripted capture the dialog is shown without waiting and the command takes the default. |
| Macro / key-binding reference | — | todo | `macro` lists bindings in the Console; the board's sheet is not built. |
| Docked, undocked and collapsed panels | `panel layers\|inspector\|console show\|hide\|toggle` (new shell command) collapses and restores the panels | partial | Docking and floating panels (board M2-Panels) are not built: the window keeps the fixed Desktop-Layout arrangement. |

## M1 gaps closed in M2 (design.md D10)

| M1 gap | Implemented as | Status | Gap / why |
|---|---|---|---|
| Crash recovery confirmation dialog | `CellSession::confirm_recovery` asks "Recover from backup file …?" through the prompt dialog (default: recover, as headless Magic) | partial | Not captured: needs a crash backup in the session store. |
| Problem count never resets | "Clear problems" in the command bar clears the Diagnostics list and the count; loading another root cell resets the count | done | |
| Status bar shows only the first line of a multi-line result | see Command bar | done | Capture 04. |
| Minimap: outline-only and cross styles, labels and cell boxes; incremental rebuild | `minimap_thumbnail()` draws outline-only styles along their edges, cross styles at half strength, label points and unexpanded cell boxes. A partial redisplay of a scene over 1 M rectangles defers the thumbnail until edits pause (750 ms) | partial | Measured: the thumbnail of the expanded 100 × 100 dfxtp_1 array (13.7 M rectangles) takes 543 ms in a Debug build. Deferral keeps it off the edit path but it is still a full rebuild; a per-area update is the remaining work. |
| Top ruler in JetBrains Mono 10 | `kTopRulerLabelSize` = 10 px mono | done | |
| Hierarchy toggle on overlapping siblings | rows still issue `expand instance PATH` (M1) | todo | Not re-checked against `expand selection`. |

## Accessibility and keyboard (10.7)

| Element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Rail buttons, Undo/Redo, palette rows, inspector actions | accessible names with their state in the name ("Select, pressed", "Undo, unavailable", "metal2, plane metal2, visible, paint layer, locked") | partial | Raven 0.2.2 has no accessible state API and cannot press custom-drawn widgets (RAVEN-CHECK gap 1). |
| Dialogs | Raven buttons (pressable by VoiceOver), the dialog named by its title | done | |
| Canvas keys | every key goes through the macro tables when the canvas has focus (click to focus) | partial | F6 focus cycling across docks (board) is not built. |

## Performance (hefesto-performance interactive editing targets)

UNDO's benchmark (task 4.6), `tests/unit/undo/benchmark_test.cpp` (`[req:Edit latency]`): 10,000 rectangles painted, erased, undone and redone over the 100 × 100 `sky130_fd_sc_hd__dfxtp_1` array.

| Run | Machine | paint 10 k (unrecorded) | erase 10 k (unrecorded) | undo erase | redo erase | undo erase + paint |
|---|---|---|---|---|---|---|
| UNDO round, Debug | M-series dev machine under load | — | 13-34 ms (6) | 17-34 ms | 17-35 ms | — |
| SHELL, Debug, 3 runs, 2026-11-10 | M3 Pro, load average 44-46 (other agents' builds) | 627-2396 ms (1159-2721) | 5.5-32.8 ms (2.4-2.5) | 11.1-45.3 ms | 11.3-77.9 ms | 27.9-74.3 ms |

Erase, undo and redo meet the 50 ms edit-latency target in the quietest run and exceed it under the heaviest load (redo 77.9 ms). Painting 10,000 rectangles in one command is not an interactive edit; its cost is the same with recording off, so it is the database, not the undo log. A Release run on an idle machine (CI, task 11.4) is still needed before the target is claimed; the test asserts the 50 ms limit only in optimized builds.

## Remaining gaps (open items)

- Raven: custom cursor images (Magic's glyphs); anchored popovers (completion, history, undo history, palette menu); accessible states and press actions for custom widgets; arrow keys inserting U+F700-U+F8FF text in text fields (RAVEN-CHECK); list-row activation in `RListView`. To be proposed upstream (design.md R1), not patched.
- VIEW: a pixel-width outline for the box in `render::highlight_batches` (Magic's 2-pixel sides).
- Not built in this pass: color window screen, key-binding sheet, docking and floating panels, wiring HUD and Wire inspector, undo history popover, palette context menu, feedback row activation.
