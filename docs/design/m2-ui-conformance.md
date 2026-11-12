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
Round 3 added rows for the layer row menu, the undo history, the color window chrome and the key-binding sheet (sections below). Still without a row because they are not built:

- Floating panel header, drop-zone highlight while dragging a panel, collapsed edge strip (Docked panels row).

### Board decisions (confirmed by the user 2026-11-10)

The boards proposed behavior that no spec stated; each was an open item of the DESIGN round. The user confirmed them on 2026-11-10, and they are now requirements, worded as built in M2, with the parts not built recorded as deferred to M4 (Desktop Alpha, `docs/roadmap.md` "Carried from M2"):

- Paint, Label and Measure tools: `hefesto-input-and-touch` "Desktop Paint, Label and Measure tools". As built, Label opens the command bar prefilled with `label ` (the canvas popover is the M4 target) and Escape cancels a sweep in every mode (Escape issuing `unmeasure` after a measure is M4).
- Dialog buttons: `hefesto-app-ux` "Prompt dialogs".
- Panel layout, Reset layout and F6: `hefesto-app-ux` "Panel layout and focus cycling". Saving the layout per user is not built and moves to M4 with floating panels and drag-to-dock.
- Undo history: `hefesto-app-ux` "Undo history" (right click or the Menu key; press-and-hold and steps named after the user command are M4).

The board proposals as they were put to the user:

- Paint tool: drag a box, release paints the active palette layer, Option-release erases it (`box values …` then `paint`/`erase`).
- Label tool: click or drag, type in a canvas popover, Enter issues `label TEXT POS LAYER`; Measure tool: drag issues `measure` on release, Esc `unmeasure`.
- Dialog buttons are verbs mapped to the PromptHost answers by index (for example "Discard" = `yes` for `flush`), the default answer has focus, Esc returns the default answer.
- Panel layout (dock, size, collapsed, floating) saved per user in settings and a Reset layout command; F6 cycles focus across docks.
- Undo history popover on press-and-hold or right-click of Undo.

## Editing tools on the rail and canvas

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Tool rail: box, select, paint, wire, label, measure live; place device disabled until M4 | `tool_rail()` (all live but Place device, tooltip "arrives in M4"); a click runs `rail_action()`: `tool box`, `tool pick`, `tool wiring`, `view`; Paint, Label and Measure are modes of the box tool (`InputRouter::set_mode`) | partial | Paint, Label and Measure follow the board decisions confirmed by the user on 2026-11-10 (see DESIGN's list). The rail is built from painted widgets, so VoiceOver cannot press it (RAVEN-CHECK gap 1). |
| Active tool in ember | `active_rail_icon(tool, mode)` from `ui::ToolRegistry`; the active button gets the ember tint and its accessible name says "pressed" | done | Captures 02 (Select) and 03 (Wire) match the board's active state. |
| Rail tooltip with the tool's key | tooltips name Shift-Space, Control-Space, Space and what each button does | partial | Raven tooltips are plain text: the key is not drawn as a key cap (board). |
| Cursor glyph per tool (`ui::ToolRegistry`, `cursor`) | `cursor_shape(glyph, mode)` maps Magic's glyph to a system pointer: corner glyphs 2-9 to the diagonal resize / move pointers, wiring and nettool to the crosshair, the Paint and Measure modes to the crosshair, Label to the I-beam | partial | Raven has 10 system pointers and no custom cursor images, so Magic's glyph bitmaps (box corner, pick arrow with box) cannot be drawn. Raven gap. |
| SVG icon set (1.6 px stroke, 24 px grid) under `assets/icons/` | drawn by Raven's SVG rasterizer since M1; `redo.svg` added for the top bar | done | |
| Box drawn as a highlight | the box client of `view::HighlightLayer` in the highlight pass (a second `render::CanvasRenderer` over the layout, `render::highlight_batches`); the M1 painted box overlay is retired | partial | The batch draws the box as a 1-framebuffer-pixel outline: Magic's 2-pixel "thick" sides (`display::box_drawing`) need a pixel-width outline in `render::highlight_batches` (VIEW open item). |
| Selection highlight (white solid outlines) | the selection client in the same pass; highlight text and point crosses are drawn by the overlay | done | Captures 01-03. |
| Move/copy previews | the pick tool's drag runs its `*bypass select move` motion command on each pointer move (tools.tcl's binding), `copy center 0` / `select keep` on the next press, as in Magic | partial | Live drag not yet exercised with OS events. |
| Paint / Label / Measure sweep preview | dashed white rectangle in the overlay while the button is held; the release issues `box values … ; paint L` (Option: `erase L`), opens the command bar with `label `, or `box values … ; measure` | partial | Board decisions confirmed by the user on 2026-11-10 (spec `hefesto-input-and-touch`); the label canvas popover is the command bar instead (popover moved to M4). |
| Wiring tracking: preview leg, HUD, mode chip, Wire inspector | the preview leg is the wiring area's selection (capture 03); pointer motion runs `*bypass wire show` while `wire.tracking` | partial | No HUD next to the cursor, no "Wiring · tracking" chip and no Wire inspector yet. |
| `*watch` tile overlay | `view::tile_overlay_scene` appended to the highlight pass (style 19, labels by the overlay) | partial | Not captured yet. |

## Selection and inspector

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Inspector for paint, label, port, instance and mixed selections | `selection_inspector_view()`: SELECTION section, kind title, layers with tile counts, label text/layer/position/port index, instance cell/id/orientation/array, bounding box | partial | Capture 02 (mixed). The board's per-row editable fields are read-only rows; edits go through the action buttons. |
| Inspector actions | buttons that run `select net`, `what`, `delete`, `clockwise 90`, `expand selection`, `edit`, `port make`; those needing an argument (`setlabel text `, `identify `, `port index `, `array `) put the command in the command bar; "Orient…" opens a menu of the eight orientations (`orient N` … `orient FW`, the current one checked) | done | Round 3: `orient_menu()`, `InspectorAction::menu`; tests in `screen_models_test.cpp`. |

## Layer palette

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Active paint layer | a click on a row runs `palette paint LAYER` (new shell command, logged); ember outline and meta "paint"; footer "Paint layer NAME" with Paint / Erase buttons (`paint L` / `erase L` on the box) | partial | The footer buttons read "Paint" / "Erase": the board's "Paint on box" / "Erase on box" do not fit side by side in the 240 px panel. |
| Visibility toggle | the eye zone of the row runs `see L` / `see no L`; shown on hover and while hidden | done | |
| Lock toggle (locked types) | the lock zone runs `tech lock L` / `tech unlock L`; meta "locked" / "paint · locked" | done | Capture 02: sky130A's inactive types show as locked. |
| Row context menu | right click (or the Menu key / Shift-F10) on a row opens `layer_menu()`: Paint layer, Paint on box, Erase on box, Select on box, Hide/Show, Lock/Unlock, each label naming its command (`palette paint L`, `paint L`, `erase L`, `select area L`, `see no L`, `tech lock L`) | partial | A Raven context menu: themed by Raven's colors, not the board's popover card. No "show only this layer": `see no *` is not "all layers" in Magic either. |
| Keyboard: ↑/↓ moves, Space selects | Tab/Shift-Tab move between rows, Space/Return select the paint layer | partial | Arrow-key navigation inside the list is not implemented; focus returns to the window after a row toggles (RAVEN-CHECK item 3). |

## Command bar

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| History | Up/Down walk the history (M1); the clock button and ^R in the command bar open the history popover (`history_entries()`: the newest 12, numbered like `history`, choosing one puts it in the command bar) above the bar | partial | The popover is a Raven menu (no anchored popover in Raven 0.2.2): no filter field and no "↩ run · → edit" hints; choosing edits, Return runs. Checked with real OS events (round 3). |
| Tab completion of commands, cells and layers (`ui::complete`) | `complete_command_line()`: a unique match completes and adds a space; several extend to the common prefix, are listed in the status bar and the Console, and open the completion popover above the bar (`completion_entries()`: 12 candidates then "… N more") | partial | A Raven menu: no "Layers matching m · 7" header or kind column. Arrow keys no longer insert U+F7xx text and held keys repeat (`TextKeyFilter`, round 3, checked with real OS events). Typed lines may abbreviate command names (Q-UI-41, DISPATCH); scripts need exact names. |
| Multi-line results: first line in the status bar, expander to the Console | `result_line()`: "<first line> (+N lines, see Console)"; clicking the status bar opens the Console tab for a multi-line result, Diagnostics otherwise; `CommandOutcome::printed` collects what commands print (`help`, `what`) | done | |

## Undo, color window, feedback, dialogs, panels

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Undo/redo affordances | Undo and Redo ghost buttons in the top bar (icons `undo`, `redo`), enabled from `undo_affordance()` (log position), tooltips with the step counts and the next step; they run `undo` / `redo`. Right click (or the Menu key) opens the undo history (`undo_history()`: the redo tail, then the undoable steps newest first, `redo N` / `undo N`; only redo while recording is off). "▲ undo off" chip after `undo disable` (click: `undo enable`). After `undo` / `redo` the status bar reads "Undid STEP · N more steps" | partial | Steps are named by their newest event's description (`undo print`), not the user command ("paint metal1"): UndoLog keeps no command per step (open item UNDO). No press-and-hold, no "Current state" divider or per-step detail column, no "Redo ▸" link in the status bar. |
| Color window screen | when `specialopen color` opens a color window, `ColorPanel` takes the right dock: caption `COLOR = 047` and "▲ map modified", ‹ › stepper (`color N`), "Color Being Edited" swatch, the six bars with − / + pumps and click-to-set (`setpoint X Y ID; pushbutton left` at Magic's pump or bar point, the `color` client's own path), the value strip (R G B, H S V, hex), Load… / Save… (`load ` / `save ` in the command bar), Close (`closewindow ID`) | partial | Captures `m3-shell/01-color`, `02-color-light`. The bars are drawn by the shell in the forge layout, not Magic's fixed surface through the renderer; pumps take the left button only (no M/R 5× step); no dock/undock button (the panel lives in the right dock). |
| Feedback list | a Feedback tab next to Console and Diagnostics: one row per area (`feedback_rows()`: index, corners, text); clicking a row (or arrows / Enter) runs `feedback find N` | done | Round 3: `ListPanel::on_select`. |
| Confirmation dialogs through `cmd::PromptHost` (crash recovery, `flush`, unsaved color map, `quit` with modified cells) | `MainWindow::DialogPromptHost` installs in `EditorState::prompt`; `prompt_dialog()` turns a question into a Raven dialog with one Raven button per answer, verbs for known questions ("Discard changes", "Overwrite", "Recover", "Quit without saving", "Cancel"), the default answer primary and focused; Escape gives the default; crash recovery through `CellSession::confirm_recovery` | done | Capture 05 (flush). In a scripted capture the dialog is shown without waiting and the command takes the default. |
| Macro / key-binding reference | a Keys tab (`panel keys show`): every macro table, the current tool's first, headed "Layout (box tool) · box · N bindings", one row per binding (key display name, command, "imacro:" for interactive macros); choosing a row puts `macro TABLE KEY "TEXT"` in the command bar to edit (`key_binding_rows()`, round-trips through `macro`) | partial | Capture `m3-shell/03-keys`. A list tab, not the board's modal sheet: no key capture, no conflict warning with Replace, no Reset table / Load file… buttons (`macro` and `source` in the command bar do these). |
| Docked, undocked and collapsed panels | `panel layers\|inspector\|console\|keys show\|hide\|toggle` collapses and restores the panels; `panel layers\|inspector dock left\|right` docks a side panel on either side of the canvas; `panel reset` restores the M1 arrangement | partial | Capture `m3-shell/04-dock`. No floating panels, drag-to-dock, panel headers or collapsed edge strips (Raven's `Rdock_area` would need the shell rebuilt on it); the layout is not saved per user. |

## M1 gaps closed in M2 (design.md D10)

| M1 gap | Implemented as | Status | Gap / why |
|---|---|---|---|
| Crash recovery confirmation dialog | `CellSession::confirm_recovery` asks "Recover from backup file …?" through the prompt dialog (default: recover, as headless Magic) | done | Captured in M4 (see "M4 closing notes"). |
| Problem count never resets | "Clear problems" in the command bar clears the Diagnostics list and the count; loading another root cell resets the count | done | |
| Status bar shows only the first line of a multi-line result | see Command bar | done | Capture 04. |
| Minimap: outline-only and cross styles, labels and cell boxes; incremental rebuild | `minimap_thumbnail()` draws outline-only styles along their edges, cross styles at half strength, label points and unexpanded cell boxes. A partial redisplay of a scene over 1 M rectangles defers the thumbnail until edits pause (750 ms) | done | Measured: the thumbnail of the expanded 100 × 100 dfxtp_1 array (13.7 M rectangles) takes 543 ms in a Debug build. An edit now updates only the thumbnail cells under its area (M4 SHELL closing pass, see "M4 closing notes"); only a scene that grew past its bounds is rebuilt, deferred as before. |
| Top ruler in JetBrains Mono 10 | `kTopRulerLabelSize` = 10 px mono | done | |
| Hierarchy toggle on overlapping siblings | rows issue `expand instance PATH` / `unexpand instance PATH` on their own instance (no box involved) | done | Regression test "a hierarchy row toggles its instance only, not an overlapping sibling" (`view_commands_test.cpp`). |

## Accessibility and keyboard (10.7)

| Element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Rail buttons, Undo/Redo, palette rows, inspector actions | accessible names with their state in the name ("Select, pressed", "Undo, unavailable", "metal2, plane metal2, visible, paint layer, locked") | partial | Raven 0.2.2 has no accessible state API and cannot press custom-drawn widgets (RAVEN-CHECK gap 1). |
| Dialogs | Raven buttons (pressable by VoiceOver), the dialog named by its title | done | |
| Canvas keys | every key goes through the macro tables when the canvas has focus (click to focus) | done | |
| F6 focus cycling | F6 / Shift-F6 (window key hook) cycle Tools → Layers (filter) → Canvas → Inspector (first action) → Console → Command, skipping hidden panels (`next_focus_region()`) | done | Checked with real OS events (round 3). |
| Popovers and menus | Raven menus: keyboard (arrows, Return, Escape) and screen readers (menu items are pressable); opened from the keyboard with the Menu key or Shift-F10 on a focused row or button | done | |

## Performance (hefesto-performance interactive editing targets)

UNDO's benchmark (task 4.6), `tests/unit/undo/benchmark_test.cpp` (`[req:Edit latency]`): 10,000 rectangles painted, erased, undone and redone over the 100 × 100 `sky130_fd_sc_hd__dfxtp_1` array.

| Run | Machine | paint 10 k (unrecorded) | erase 10 k (unrecorded) | undo erase | redo erase | undo erase + paint |
|---|---|---|---|---|---|---|
| UNDO round, Debug | M-series dev machine under load | — | 13-34 ms (6) | 17-34 ms | 17-35 ms | — |
| SHELL, Debug, 3 runs, 2026-11-10 | M3 Pro, load average 44-46 (other agents' builds) | 627-2396 ms (1159-2721) | 5.5-32.8 ms (2.4-2.5) | 11.1-45.3 ms | 11.3-77.9 ms | 27.9-74.3 ms |

Erase, undo and redo meet the 50 ms edit-latency target in the quietest run and exceed it under the heaviest load (redo 77.9 ms). Painting 10,000 rectangles in one command is not an interactive edit; its cost is the same with recording off, so it is the database, not the undo log. The test asserts the 50 ms limit only in optimized builds; the Release run below covers that.

### Release run (task 4.6)

CORE, 2026-11-10: `build/m3-rel` (macos-release preset, headless, `-DHEFESTO_WARNINGS_AS_ERRORS=ON`), `hefesto_undo_tests "[req:Edit latency]"`, 5 runs on an Apple M3 Pro with a load average of 31-58 (other agents' builds; no idle machine was available). The benchmark now also times one `paint` that merges the 10,000 rectangles into one, the "command affecting 10,000 rectangles" of the target, and its undo; the optimized build asserts the 50 ms limit on that paint, its undo, the erase, and the undo and redo of the erase.

| Measure (10,000 rectangles) | Release, 5 runs | Debug, same machine | Target |
|---|---|---|---|
| one `paint` over all of them, recorded | 0.54-0.58 ms | 5.8 ms | 50 ms: met |
| its undo | 3.09-3.14 ms | 12.1 ms | 50 ms: met |
| one `erase` of all of them, recorded (unrecorded) | 0.44-0.47 ms (0.20-0.21) | 5.5 ms (2.3) | 50 ms: met |
| undo of the erase | 3.07-3.27 ms | 11.1 ms | 50 ms: met |
| redo of the erase | 2.94-3.09 ms | 11.2 ms | 50 ms: met |
| undo of erase + paint (two steps, 20,000 events) | 11.4-16.5 ms | 27.6 ms | 50 ms: met |
| 10,000 separate `db::paint` calls in one step (unrecorded) | 62.0-64.0 ms (60.2-61.7) | 628 ms (624) | not an interactive command |

Result: the database part of the edit-latency target (edit, recording, replay, bounding boxes) is met with a margin of more than 15x for a single command affecting 10,000 rectangles, even under load. Recording costs about 0.25 ms per 10,000 tile changes. The 10,000-call loop is the only row above 50 ms. It is not one user command: no Magic command paints 10,000 separate boxes. Its cost is the same without recording, about 6 µs per `db::paint` call (not profiled further). Plan: if M3's DRC or M4's scripting paints many boxes in one command, add a batched paint primitive (one plane walk per command) and add that workload to this benchmark. The view update after the edit is SHELL's, measured by the canvas benchmark. The time for background DRC to start (100 ms) is M3's.

## Remaining gaps (open items)

- Raven (issue texts in `docs/decisions/raven-go-no-go.md`, "M2 round 3"): custom cursor images (Magic's glyphs); an anchored popover widget; accessible states and press actions for custom widgets; the Cocoa function-key text and key-repeat bugs (worked around in `TextKeyFilter`). To be proposed upstream (design.md R1), not patched.
- VIEW: a pixel-width outline for the box and selection in `render::highlight_batches` (Magic's 2-pixel sides; 1 framebuffer pixel is thin on Retina).
- UNDO: a per-step label (the command that made the step) for the undo history and the "Undid …" message.
- Not built in M2, moved to M4 by the user's decision of 2026-11-10 (`docs/roadmap.md`, M4 "Carried from M2"): floating panels and drag-to-dock, the panel layout saved per user, the modal key-binding sheet with key capture, the label canvas popover, the wiring HUD and Wire inspector, a per-area minimap update, the crash-dialog capture.

## Round 3 review fixes (INTEGRATION, 2026-11-10)

Checked by scripted captures of `build/m2` (macos-debug, desktop on) on the sky130A `dfxtp_1` cell, one instance at a time:

| Finding | Fix | Evidence |
|---|---|---|
| Color window cut off at 1024 x 640 | `ColorPanel` column in a `ScrollBox` | capture `--width 1024 --height 640 --run 'specialopen color 47; panel keys show'`: the dock scrolls to the Value bar, value strip and file row |
| "undo off" chip overflowed the top bar at 1600 px | Undo/Redo widths counted in `top_full_width_` | capture `--width 1600 --height 1000 --run 'undo disable'`: the deferred group gives way, nothing clipped |
| Edge separators on the wrong side after re-docking | `panel_edge_index` (edge on the canvas side) | capture `--run 'panel layers dock right; panel inspector dock left'`; test "a docked panel's edge separator is on the canvas side" |
| "Undid …" only for the exact lines `undo`/`redo` | `undo_move_message` in a post-command client update handler | capture `--run '...; paint metal2; undo 2'` shows "Undid paint …"; tests "any command that moves the undo log names the step it passed", "the undo message holds for an undo inside a multi-command line" |

## M2 exit criteria evidence (task 11.6, 2026-11-10)

| Criterion | Evidence | State |
|---|---|---|
| Unit tests green on every leg | `build/m2` macos-debug `-Werror` desktop on: `ctest -LE network` 100 % of 1562 (deviations counted as skipped); macos-release, macos-asan, macos-bench and macos-xcode presets configured, built and tested (task 11.1); CI legs of the round-3 pull request | see tasks.md 11.1, 11.6 |
| Golden tests green | `tools/golden/replay.py`: 226 pass, 0 fail, 0 skip, 38 deviation, 0 error (every deviation names an approved decision or a `deferred` milestone); golden-capture verify runs 42803858356, 42805161830 | done |
| Fuzz targets for the new readers | `fuzz_macro_file`, `fuzz_wiring_section`, `fuzz_wire_segment_file` with seed corpora; seed replays pass in ctest; CI libFuzzer job (60 s per target) | done |
| Requirement coverage | `check_requirement_coverage.py --milestone M2`: 163 of 163 exact/behavioral requirements covered | done |
| Next milestone readiness | `check_milestone_readiness.py --milestone M3`: 24 unresolved questions (DRC, MASK, MISC) to decide before M3 starts | reported |
| `openspec validate --all --strict` | 172 passed, 0 failed | done |
| Usability review (2.3), boards in the reference artifact (2.1) | need the user | open |
| Raven go/no-go (10.1, 11.4) | final GO for M2 (user decision 2026-11-10) on the Apple M3 Pro measurements and the Linux lavapipe CI job; the reference M1 frame-rate run, Linux X11/Wayland live input and Orca are M4 confirmation checks (`docs/decisions/raven-go-no-go.md`) | done |
| Audible VoiceOver run by a person (10.1) | the user's guided review | open |
| Board decisions for Paint/Label/Measure, dialogs, panels, undo history (10.3) | confirmed by the user 2026-11-10; specified in `hefesto-input-and-touch` and `hefesto-app-ux` | done |

## M4 closing notes (CARRY, 2026-11-11)

The M2 items the user moved to M4 (`docs/roadmap.md` "Carried from M2"; `openspec/changes/m4-scripting/design.md` D17, tasks 11.1-11.8). Owner: CARRY. Built on branch `m4-round2`, checked on macOS 27.0.1 arm64 (Apple M3 Pro, MoltenVK 1.4.2), build `build/m4r2-carry` (macos-debug, `-Werror`, desktop on). The approved M2 boards apply; no new board.

CARRY owns the models, the core and renderer parts and their tests; the main window, the Raven wrapper and the canvas are SHELL's files (D2), so each on-screen hookup is an open item for SHELL with its exact edit (listed in the round's report to INTEGRATION). A row is **done** only after a capture of the product app, as in M2; until the hookup lands and is captured, a built and unit-tested model is **partial**.

Round 3 (CARRYUI, 2026-11-11, branch `m4-round3`, build `build/m4r3-carryui`, macos-debug `-Werror`, desktop on): the panel layout, key-binding sheet, label popover, measure Escape, Paint state, wiring chip, HUD and Wire inspector are wired into the main window (`panels_host.cpp`, `key_bindings_view.cpp`, `label_tool_view.cpp`, `wiring_hud_view.cpp`). Captures are the product app with `--capture` on `sky130_fd_sc_hd__dfxtp_1.mag` (sky130A), `CAD_ROOT` pointing at Magic's display styles and `HOME` at a scratch folder; they are kept in the session scratchpad (`m4r3-shots/`). States reached only with the pointer or real key presses (header drag, label sweep, HUD, key capture) are not captured.

| Carried item | Implemented as | Status | Gap / why |
|---|---|---|---|
| Floating panels and drag-to-dock: panel headers, drop-zone highlight, collapsed edge strips | Model `panels_layout.hpp` (round 2). On screen (round 3, `panels_host.cpp`): every panel (strip leaders Layers \| Cells \| Libraries, DRC, Inspector, Technology, Console \| Keys) in a frame with a 32 px header (title, Collapse/Expand and Float buttons, Dock on a floating one; the header's context menu docks left/right/bottom, floats, resets: the keyboard and click alternative to dragging); docks stacked left and right of the canvas, the bottom dock under it, floating panels as outlined cards over the canvas kept inside it; a side dock with no expanded panel, or one the window is too narrow for (`fit_to_window`: at 1024 × 640 the right dock first), shows its 32 px edge strip, whose entries run `panel NAME show`. The `panel` command is `run_panel_command()` (registered by the main window instead of the M2 `ViewActions` one); docking or floating a tab moves its strip (`panel_group_leader`) | partial | Captures (CARRYUI, `build/m4r3-carryui`, 1600 × 1000 unless noted): `panels-default` (headers on the Desktop-Layout arrangement), `panels-float-strip` (`panel inspector float 900 200 300 420; panel layers hide; panel console show`: floating Inspector card, the Layers edge strip, the Console under the canvas), `panels-narrow` (1024 × 640: the right dock as its "Inspector" strip). Match M2-Panels tiles 1, 3, 4. Not on screen: dragging a header with the ghost and dashed drop zone (tile 2): Raven has the events (`Rdrag_begin`, drop callbacks, mouse-down in the event hook), the wrapper `raven_ui.hpp` does not expose them yet (SHELL open item); `PanelDrag` is ready. Floating cards have no 8 px radius or shadow (a 1 px outline). |
| Panel layout saved per user and restored at launch | `save_panel_layout()` / `restore_panel_layout()`: `panel-layout.json` (version 1) in `platform::default_storage_dir()`. The main window restores it when it builds its panels and saves it after every `panel` command that changed it and on quit (not in a scripted capture or smoke run, which leave the user's settings alone); `--console` still shows the Console | done | Test "the panel layout is saved per user and restored at launch". Capture `panels-restored`: a saved file with the Inspector docked left, the Layers tabs right and the Console shown, then a plain launch (`HOME` pointed at that folder): the window comes up in that arrangement. |
| Modal key-binding sheet: capture, conflict warning with Replace, Reset table, Load file | Model `key_bindings_sheet.hpp` (round 2). On screen (`key_bindings_view.cpp`): `panel keys show` opens the sheet over the canvas (the Keys tab behind it keeps the full list); TABLE chooser (the current tool's table first), the bindings list (display names, command, "imacro"), Add binding / Edit (also Return on a row) / Delete, Reset table, Load file…, Close; while it captures it takes every key (Esc cancels the capture, then closes the sheet); a key bound in the table shows the ▲ warning with Replace; ✓ Save issues the `macro` lines through the command line | partial | Capture `key-sheet` (`panel keys show`) matches the M2-Key-Bindings sheet layout. The capture and conflict states need real key presses (no scripted path); a manual check is left. Raven has no window-modal sheet for custom content: the sheet is a card that holds the keyboard (Raven issue 4). |
| Label text entry as a canvas popover | `LabelPopover` (round 2). On screen (`label_tool_view.cpp`): the Label sweep's prompt opens a card in the canvas overlay right of the box (`LayoutCanvas::popover_point`): text, position (auto or Magic's directions), layer (enabled with a position); Return or Add label issues `label …`, Esc or Cancel closes it | partial | Built; not captured: the popover opens from a pointer sweep only, and a scripted run has no pointer (manual check left). The card is placed by `LayoutCanvas::label_point` (SHELL closing pass): the box's window rectangle through `label_popover_point()`, right of the box and flipped to its left at the canvas edge, kept on the canvas. Raven issue 4. |
| Escape after a measure issues `unmeasure` | `MeasureEscape`, armed after the Measure sweep's `measure` line (between frames, by the session's command counter); Escape on the window with no sweep in progress and the command bar not focused issues `unmeasure` when nothing ran since | partial | Test "Escape right after a measure issues unmeasure". Wired in the window key hook (`tool_shortcut`); needs a pointer sweep, so no capture (manual check left). |
| Paint tool disabled until a paint layer is chosen | `paint_tool_state()`: the rail's Paint button is disabled (not clickable), with the tooltip "Choose a layer to paint" and the accessible name "Paint, unavailable", until the palette has a paint layer; `mode_for_paint_layer()` leaves the Paint mode when the layer is cleared | done | Test "the Paint button is disabled until a paint layer is chosen"; captures `panels-default` (disabled Paint in the rail, no layer). |
| Undo steps named after their user command | `undo::UndoLog::close_step(label)`, `step_label()`, `undo::command_label()` ("paint metal1"; `*bypass` dropped; 60 bytes at most) and `undo::history()` (the redo tail farthest first, then the undoable steps newest first, each with `redo N` / `undo N`); `undo print` is unchanged | partial | Tests in `tests/unit/undo/undo_log_test.cpp` (`[cap:layout-undo]`, and `[cap:hefesto-app-ux][req:Undo history]` for the spec's "Undo two steps" scenario). Hookups done: `editor::post_command` labels each step (INTEGRATION, round 2); `undo_history()`, `next_undo_step()` and `undo_move_message()` in `screen_models.cpp` name steps with `step_label()` (SHELL closing pass; test "the undo history names each step after the command that made it": "↶ erase metal1", "Undid erase metal1"). Not captured: the history is a popup menu opened by a pointer or the Menu key. |
| Undo history on press-and-hold of Undo | SHELL closing pass: `ui::Painted::on_hold` (raven_ui) over the widget's `R_EVENT_MOUSE_DOWN`/`UP` and a one-shot `Rtimer` of `kHoldDelay` (500 ms); the toolkit-free `PressHold` decides: released before the delay = a click (`undo`), still held when it passes = the undo history under the button, whose release then does nothing; a button released off the widget opens nothing. Undo and Redo both have it; right click and the Menu key still open the history | partial | Test "pressing and holding Undo opens the history; a short press undoes" (`screen_models_test.cpp`). Not checked with a real pointer (no pointer reaches a scripted run; posting system events into the user's session is not done here): manual check left. With a hold handler the Undo button clicks on release instead of on press. |
| Per-area minimap update | `minimap.hpp`: `MinimapThumbnail` keeps the thumbnail's cells; `dirty_area(area)` gives the world rectangle a partial scene must cover, `update(partial, area)` resets and recomposites only the cells under the area. A rebuild equals `minimap_thumbnail()` fill for fill, and an update equals a rebuild of the edited scene | partial | Tests `minimap_test.cpp` (random scenes, point and line edits on cell boundaries, erase). Benchmark (hidden test, Debug, 2 M random rectangles, loaded machine): rebuild 1475 ms, update 10.6 ms (8 cells). SHELL closing pass: `LayoutCanvas` keeps a `MinimapThumbnail`; a partial redisplay inside the same bounds updates only the cells under each spliced area from a scene built over `dirty_area(area)` (`update_minimap`), a full scene build or grown bounds rebuild it (still deferred 750 ms for a grown scene over 1 M rectangles). Seen in `docs/images/app/m4-script-dimmed.png` (the metal2 painted by a running script is in the minimap). |
| Pixel-width box and selection outline in `render::highlight_batches` | `render::HighlightOptions{pixel_ratio}`: highlight lines are one logical pixel wide; the thick box (`display::box_drawing`: an outline item in the box or medium-highlights style) draws each side two logical pixels wide inside the box, one batch layer per side. `render::LayerStyle` gained `line_width` and `line_shift_x/y` (pushed in `View.base.w`; `rect.vert` widens and moves zero-width instances; SPIR-V regenerated) | partial | Tests in `layout_scene_test.cpp` and a GPU readback on MoltenVK (`headless_gpu_test.cpp`, "the thick box's sides are drawn inside the box": at ratio 1 and 2 exactly the 2 / 4 framebuffer columns and rows inside each edge are white). SHELL closing pass: `LayoutCanvas::refresh_highlights` passes `HighlightOptions{pixel_ratio_()}` and rebuilds the batches when the ratio changes (a window moved to a display of another scale). Retina (2×) captures `docs/images/app/m4-script-dimmed.png` and `m4-consent-origin.png` show the one-logical-pixel box outline; not compared pixel by pixel with the board. |
| Wiring tracking HUD, "Wiring · tracking" chip, Wire inspector | Models `wiring_hud.hpp` (round 2). On screen (`wiring_hud_view.cpp`): the chip in the top bar while the tool tracks (a click issues `trackwire cancel`); the Wire inspector in the Inspector's slot while the wiring tool is current (type, width with Width − / +, Switch layer, the technology's wiring stack, a row issues `wire type NAME`); the HUD, a painted label over the canvas at the cursor (+14, +20, flipped near the edges) | partial | Capture `wiring` (`tool wiring; box 20 20 20 20; wire type metal2; trackwire current`): chip and Wire inspector as M2-Wiring tiles 3-4. The HUD follows the pointer, so no scripted capture (manual check left); it is placed from `popover_point`, which keeps it 280 px from the canvas edge (SHELL open item: a world-to-window mapping on `LayoutCanvas`). |
| Capture of the crash-recovery dialog | `CellSession::confirm_recovery` → `DialogPromptHost` (M2); captured: `hefesto -T sky130A.tech dfxtp_1.mag --run "box 0 0 50 40; paint m1; crash save FILE"`, then `hefesto -T sky130A.tech --run "crash recover FILE" --capture crash.ppm` (1600 × 1000, dark) | done | Matches the M2-Dialogs board: the question "Recover from backup file FILE?", Cancel and the primary, focused Recover. Noted: the dialog grows to the width of a long backup path instead of wrapping it. The capture is kept in the session scratchpad. |
| VoiceOver press on custom-drawn controls | not possible at the Raven pin | Raven gap | Raven 0.2.2 (4a05d6f) makes a node pressable only for its built-in buttons, checkboxes, menu items and list/tree items (`src/core/accessibility.c`, `RVN_A11Y_INVOKABLE` by widget class); `accessibilityPerformPress` does nothing on a custom widget, and there is no API to give one an action or a checked state. Recorded as a gap at M4 exit, Raven issue 3. |
