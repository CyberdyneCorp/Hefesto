# Hefesto App UX

## Purpose
Defines the information architecture of Hefesto's modern GUI: the main window, panels, browsers and multi-window behavior on desktop and iPad. Magic's Tk wrapper GUI is specified separately for reference (capability `ui-gui-managers`). Hefesto provides equivalent functions in a modern layout. Every interaction maps to commands of the command API.

## Requirements

### Requirement: Main window layout
Each layout document window SHALL contain:
- the layout canvas
- a layer palette showing the tech's layers with their display styles, plus visibility and lock toggles
- a cell hierarchy browser
- a property inspector for the current selection
- a command bar
- a status bar showing cursor coordinates in the current units, the box size, the edit cell and the DRC state

Panels SHALL be collapsible. On the desktop, the window SHALL be built from Raven widgets: dock area, splitters, tabs and tree/table views. Panels SHALL be dockable, and the UI theme SHALL use Raven's theme system, including light and dark themes. On iPad, panels SHALL be SwiftUI sidebars or popovers that adapt to the size class. Visual styling follows capability `hefesto-design-system`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-11, docs/decisions/product.md#P-23)

#### Scenario: Layer palette paint
- **WHEN** the user selects a layer in the palette and invokes "paint"
- **THEN** the `paint <layer>` command SHALL be issued for the box area

### Requirement: Prompt dialogs
On the desktop, every question a command asks through the prompt host (`cmd::PromptHost`: crash recovery, `flush` of a modified cell, an unsaved color map, `quit` with modified cells, overwriting a file) SHALL be shown as a modal dialog:
- the title is the question's first sentence, and the rest of the question is the body
- there is one button per answer of the prompt, and each button returns that answer by its index in the prompt's answer list
- for a yes/no question whose consequence is known, the buttons are verbs: "yes" reads "Discard changes" (a question about throwing away or losing changes), "Overwrite", "Recover" or "Quit without saving", and "no" reads "Cancel"; other answers keep their words, capitalized
- the default answer's button is last, primary and has the keyboard focus, so Enter gives the default answer
- Escape, or closing the dialog, gives the default answer, as when a script runs without a prompt host

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23, board `docs/design/mockups/M2-Dialogs.dc.html`, confirmed by the user 2026-11-10)

#### Scenario: Flush of a modified cell
- **GIVEN** cell `top` has unsaved changes
- **WHEN** `flush` asks "Really throw away all changes made to cell top?" with the answers `no`, `yes` and the default `no`
- **THEN** the dialog SHALL show the buttons "Discard changes" (answer 1, `yes`) and "Cancel" (answer 0, `no`), "Cancel" last, primary and focused
- **WHEN** the user presses Escape
- **THEN** the answer SHALL be `no` and the cell SHALL keep its changes

#### Scenario: Answers without a known verb
- **WHEN** a prompt asks "top has been modified." with the answers `write`, `flush`, `skip`, `abort`, `autowrite` and the default `write`
- **THEN** the dialog SHALL show five buttons, "Flush" first and the default "Write" last

### Requirement: Panel layout and focus cycling
On the desktop:
- the panels (Layers, Cells and Libraries, the tabs of the left strip; DRC; Inspector; techmanager; Console and Keys, the tabs of the bottom strip) SHALL be collapsible with `panel NAME show|hide|toggle`; `panel NAME` alone reports "shown" or "hidden"; showing a tab shows its strip; a collapsed panel keeps its 32 px header row
- every panel SHALL dock left or right of the canvas or under it with `panel NAME dock left|right|bottom`, the panel's edge separator on the canvas side, or float over the canvas with `panel NAME float ?X Y W H?` (window coordinates, kept inside the window); docking or floating a tab moves its whole strip; each panel's header SHALL have Collapse/Expand and Float buttons (Dock on a floating panel) with accessible names that carry the state ("Layers, collapsed"), and a context menu that docks, floats or resets
- a side dock with no expanded panel, or one the window is too narrow for (the canvas below 600 px: the right dock first), SHALL show a 32 px edge strip whose entries show their panel
- `panel reset` (Reset layout) SHALL restore the default arrangement: Layers on the left, Inspector on the right, both shown
- F6 SHALL move the keyboard focus to the next region and Shift-F6 to the previous one, in the order tool rail, Layers (filter field), canvas, Inspector (first action), Console, command bar, skipping hidden panels and an Inspector without an action

The panel layout (dock, size, collapsed state, floating window) SHALL be saved per user (`panel-layout.json` in the user's storage folder) after each change and on quit, and restored at launch; a scripted capture or smoke run SHALL NOT save it. Dragging a panel header (a press that moves 4 or more units; a still press stays a click) SHALL carry the panel's title with the pointer; over a dock or an edge strip the target dock SHALL be shown as a dashed ember drop zone with "Drop to dock on the left|right|at the bottom" (and "NAME was undocked from here" over the dock a floating panel came from; an empty dock previewed at its width), over the canvas the floating window's outline, and the release SHALL issue the same `panel` command (`panel NAME dock ZONE`, or `panel NAME float X Y W H` over the canvas). Escape SHALL cancel the drag and change nothing.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23, board `docs/design/mockups/M2-Panels.dc.html`, confirmed by the user 2026-11-10)

#### Scenario: Dock and reset
- **WHEN** the user runs `panel layers dock right` and then `panel inspector dock left`
- **THEN** the Layers panel SHALL be right of the canvas and the Inspector left of it, each with its edge separator next to the canvas
- **WHEN** the user runs `panel reset`
- **THEN** the Layers panel SHALL be on the left and the Inspector on the right, both shown, and the command SHALL report "default layout"

#### Scenario: F6 skips hidden panels
- **GIVEN** the Layers panel is hidden (`panel layers hide`) and the focus is on the tool rail
- **WHEN** the user presses F6
- **THEN** the focus SHALL move to the canvas
- **WHEN** the user presses Shift-F6
- **THEN** the focus SHALL return to the tool rail

#### Scenario: Docking a floating panel again
- **GIVEN** the user ran `panel inspector float 700 150 300 300`
- **WHEN** the user runs `panel inspector dock right` (or drops its header on the right dock)
- **THEN** the Inspector SHALL fill the right dock at the dock's width and full height, as before it floated

#### Scenario: Layout restored at launch
- **GIVEN** the user ran `panel layers dock right` and `panel inspector float 900 200 300 420` and quit
- **WHEN** Hefesto starts again
- **THEN** the Layers, Cells and Libraries tabs SHALL be right of the canvas and the Inspector a floating card at that place

### Requirement: Undo history
On the desktop, the top bar SHALL have Undo and Redo buttons that run `undo` and `redo`, are enabled only when there is a step to undo or redo, and name the next step in their tooltips. A right click on Undo, pressing and holding it for 500 ms (a shorter press is a click), or the Menu key while it has the focus SHALL open the undo history, titled with the step count and the history limit ("Undo history · N steps · limit L"):
- first the undone steps that can be redone, farthest first, each choosing `redo N`
- then the undoable steps, newest first, each choosing `undo N`
- at most 8 of each, each named after the user command that made it ("paint metal1"; layout-undo's step label), or by the description of its newest event when the step has no label
- while undo recording is off (`undo disable`), only the redo entries are enabled
- with an empty log, the status bar says "nothing to undo or redo" instead

After any command that moves the undo log, the status bar SHALL read "Undid STEP · N more steps" (or "Redid …").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23, board `docs/design/mockups/M2-Undo-Redo.dc.html`, confirmed by the user 2026-11-10)

#### Scenario: Undo two steps from the history
- **GIVEN** three edits were made, each its own undo step, and none was undone
- **WHEN** the user right-clicks Undo and chooses the second entry
- **THEN** `undo 2` SHALL be issued, and the status bar SHALL read "Undid STEP · 1 more step", STEP naming the older of the two undone steps

#### Scenario: Recording off
- **GIVEN** `undo disable` was run after one undone step
- **WHEN** the user opens the undo history
- **THEN** the `redo 1` entry SHALL be enabled and every `undo N` entry SHALL be disabled

### Requirement: Functional parity with Magic's GUI managers
Hefesto SHALL provide the following browsers and dialogs:
- a cell manager and a library manager
- a DRC results browser, which steps through errors in the order of `drc find` and shows the why-text
- a net browser, which selects, highlights and lists terminals
- a text and label helper
- device generator (PCell) dialogs driven by the PDK generators of capability `pcell-toolkit`

These SHALL cover every function of capability `ui-gui-managers`.

Compatibility: behavioral

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: PCell dialog
- **WHEN** the user opens the device generator for sky130 nfet
- **THEN** a dialog SHALL list the generator's parameters with their defaults and checks, and "create" SHALL place the generated cell

### Requirement: Multiple windows and documents
Hefesto SHALL support multiple documents and multiple windows per document. Each window has its own view, root cell and expansion state, as in Magic's multiple layout windows. On iPad, this SHALL use iPadOS scenes, including Stage Manager and external displays.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: External display on iPad
- **WHEN** an iPad is connected to an external display under Stage Manager
- **THEN** a second layout window SHALL be openable on that display

### Requirement: Desktop platform integration through Raven
On the desktop, the shell SHALL use Raven's platform features for these purposes:
- **Tables:** result and list browsers SHALL use Raven's Table widget with virtual rows, sortable and resizable columns, keyboard and screen-reader support. These include the DRC results, LVS mismatches, cell and library managers, net browser, and simulation measurements.
- **Menu bar:** on macOS, the application menu SHALL appear in the system menu bar.
- **Job completion:** when a long job finishes while the window is not focused, Hefesto SHALL post a system notification (macOS Notification Center, freedesktop notifications on Linux). Long jobs include DRC, extraction, LVS, simulation sweeps or Monte Carlo, tapeout packaging, and agent proposals. A setting SHALL turn this off.
- **Job progress:** a running long job SHALL show its progress on the Dock icon (macOS) or the launcher entry (Linux).
- **Developer inspector:** Raven's widget inspector MAY be enabled in development builds. It SHALL be absent from release builds.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-11)

#### Scenario: Extraction finishes in the background
- **GIVEN** a full-chip extraction is running and the user has switched to another application
- **WHEN** the extraction finishes
- **THEN** a system notification SHALL report the result, and the Dock or launcher progress indicator SHALL be cleared

#### Scenario: Large DRC result list
- **WHEN** a DRC run reports 200,000 errors
- **THEN** the DRC browser SHALL list them in a virtual Table, without creating a widget per row, and SHALL sort by rule or by count

### Requirement: Accessibility and localization
The GUI SHALL support:
- the platform screen readers, for all controls and panels: on the desktop through Raven's accessibility support (VoiceOver on macOS, AT-SPI on Linux), on iPad through SwiftUI accessibility
- full keyboard navigation
- Dynamic Type on iPad; on the desktop, Raven's text scale
- on the desktop, Raven's high-contrast themes, color filters for color blindness, and reduced motion, applied to the UI chrome only (layer colors on the canvas follow `hefesto-design-system`)
- localizable user-interface strings

Input methods for Chinese, Japanese and Korean text depend on Raven, which does not connect them yet on the desktop (decision P-11, risks). Until it does, entering such text in labels is unsupported on the desktop.

Layout-data text and command names SHALL NOT be localized.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2, docs/decisions/product.md#P-11)

#### Scenario: VoiceOver on layer palette
- **WHEN** VoiceOver focuses a layer palette entry
- **THEN** it SHALL announce the layer name and its visibility state
