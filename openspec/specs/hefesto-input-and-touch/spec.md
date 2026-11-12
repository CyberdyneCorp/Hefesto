# Hefesto Input and Touch

## Purpose
Defines how mouse, keyboard, trackpad, touch and Apple Pencil input map onto Magic's tools, macros and commands. Every interaction ends up as a command of the single command API (see capability `hefesto-architecture`). The interaction model is the same on every platform.

## Requirements

### Requirement: Desktop input parity with Magic
On Linux and macOS, input SHALL arrive through Raven's input events (mouse, keyboard, trackpad scroll and pinch, pen). With a mouse and keyboard, the following SHALL behave as specified by capability `ui-tools-and-input`:
- the default key bindings
- the button bindings of each tool (box, wiring, netlist, rsim)
- user macros

Compatibility: behavioral

(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: Box tool buttons
- **WHEN** the left mouse button is clicked in a layout window with the box tool active
- **THEN** the box SHALL move by its lower-left corner to the cursor, as in Magic

### Requirement: Desktop canvas gestures
On Linux and macOS, the layout canvas SHALL take trackpad and touch-screen gestures through Raven's gesture events (`RGestureEvent`, kind `R_GESTURE_PINCH`; on macOS Raven turns `magnifyWithEvent` into it) and the wheel events Raven marks precise (`REvent::wheel_precise`, from `hasPreciseScrollingDeltas` on macOS):
- a pinch (two fingers on a trackpad or a touch screen) SHALL zoom the view continuously by each event's scale change, about the center of the fingers: the layout point under the fingers stays under them, and when the fingers also move, it moves with them
- two fingers moving on a trackpad (precise wheel deltas) SHALL pan the view, the content following the fingers
- a mouse wheel (deltas that are not precise) SHALL zoom the view about the pointer
- these gestures SHALL stop at the zoom range the zoom control's `zoom` commands reach: from the core window's smallest scale (one 1/65536 pixel per internal unit, Magic's "At minimum scale!") to the renderer's largest
- after each gesture event the zoom percentage of the zoom control, the rulers, the minimap's view rectangle and the core window's view SHALL show the new view
- when the canvas is resized (the window, a panel shown or hidden, the console revealed by an error), the view SHALL be refitted to the cell only until the first gesture or press in the canvas; after it, the scale and the layout point under the canvas's top-left pixel SHALL be kept, so the layout under every pixel still shown, and a tool's gesture in progress (a wiring leg), stays where it was

The canvas SHALL take the whole pinch when it begins, so that Raven neither scrolls nor zooms another widget with it. A swipe of three or more fingers SHALL be left to Raven (back / forward). Continuous gestures move the view directly instead of issuing one command per event (design.md D6 of change m1-core-viewer); keys and the zoom control issue commands.

Compatibility: native
(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: Trackpad pinch zooms at the pointer
- **GIVEN** a cell shown at 93 % in the desktop app on macOS
- **WHEN** the user spreads two fingers on the trackpad until the distance between them is 2.5 times larger, with the pointer over a contact
- **THEN** the view SHALL zoom smoothly to about 232 %, the contact SHALL stay under the pointer, and the zoom control SHALL read the new percentage

#### Scenario: Two-finger scroll pans, the wheel zooms
- **WHEN** the user moves two fingers up on a trackpad
- **THEN** the view SHALL pan with the fingers and its zoom SHALL not change
- **WHEN** the user then turns a mouse wheel one notch up
- **THEN** the view SHALL zoom in about the pointer

#### Scenario: A resize during a wiring leg keeps the view
- **GIVEN** the wiring tool tracking from a press on a cell's material, the cell fitted at load
- **WHEN** the console opens below the canvas before the pointer moves east of the press
- **THEN** the view SHALL not be refitted, and the leg SHALL go east

#### Scenario: Pinch stops at the zoom limit
- **GIVEN** a view already at the largest zoom the zoom-in button reaches
- **WHEN** the user keeps spreading two fingers
- **THEN** the zoom SHALL stay at that limit and the point under the fingers SHALL not drift

### Requirement: Desktop Paint, Label and Measure tools
On Linux and macOS, the tool rail's Paint, Label and Measure buttons SHALL be modes of the box tool: choosing one issues `tool box` when another tool is current, and the rail shows the mode's button as the active tool. In a mode, the left button SHALL sweep an area instead of running the box tool's left-button macro; the other buttons and every key SHALL keep the box tool's bindings of capability `ui-tools-and-input`. While the button is held, the canvas SHALL draw the swept area, snapped to the tool grid, as a dashed preview. On release the desktop SHALL issue `box values X0i Y0i X1i Y1i` for the swept area (internal units), followed in the same command line by:
- **Paint:** `paint L`, where L is the palette's paint layer (`palette paint L`); with Option (Alt) held at release, `erase L` instead. Without a paint layer, only the box is set.
- **Label:** nothing; the command bar opens prefilled with `label `, so that the user types the text (and optionally the position and layer) and Enter issues `label TEXT …` on the new box. A click without a drag gives a zero-size box at the point.
- **Measure:** `measure` (capability `ui-box-stack-and-measure`).

Escape while the button is held SHALL cancel the sweep and issue nothing. Every command issued SHALL be logged like a typed command.

Deferred to M4 (user decision 2026-11-10): the label text entry as a popover on the canvas (the board's design; Raven 0.2.2 has no anchored popover), Escape after a measure issuing `unmeasure` (until then `unmeasure` is typed or bound), and the Paint button disabled with the tooltip "Choose a layer to paint" while no paint layer is set.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23, board `docs/design/mockups/M2-Tool-States.dc.html`, confirmed by the user 2026-11-10)

#### Scenario: Paint tool drag
- **GIVEN** the Paint tool is active and the palette's paint layer is `m1`
- **WHEN** the user presses the left button at (30,40) internal units, drags to (5,8) and releases
- **THEN** the desktop SHALL issue `box values 5i 8i 30i 40i ; paint m1`, and the box SHALL be (5,8)–(30,40)

#### Scenario: Option-release erases
- **GIVEN** the Paint tool is active and the paint layer is `m1`
- **WHEN** the user drags from (0,0) to (2,2) and releases with Option held
- **THEN** the desktop SHALL issue `box values 0i 0i 2i 2i ; erase m1`

#### Scenario: Escape cancels a sweep
- **GIVEN** the Paint tool is active and the left button is held after a press at (0,0)
- **WHEN** the user presses Escape and then releases the button at (9,9)
- **THEN** no command SHALL be issued

#### Scenario: Label tool
- **GIVEN** the Label tool is active
- **WHEN** the user drags from (1,1) to (3,3) and releases
- **THEN** the box SHALL be (1,1)–(3,3) and the command bar SHALL open with the text `label `
- **WHEN** the user types `A` and presses Enter
- **THEN** `label A` SHALL be issued, placing label A on that box

#### Scenario: Measure tool
- **GIVEN** the Measure tool is active
- **WHEN** the user drags from (0,0) to (4,0) and releases
- **THEN** the desktop SHALL issue `box values 0i 0i 4i 0i ; measure`

#### Scenario: Other buttons keep the box tool bindings
- **GIVEN** the Measure tool is active
- **WHEN** the user presses the right button at (20,20)
- **THEN** the box tool's right-button macro (`*bypass box corner ur cursor`) SHALL run, as with the plain box tool

### Requirement: Touch and Pencil mapping
On iPadOS (SwiftUI/UIKit gesture recognizers), every tool action and every default key or button binding SHALL be reachable without a mouse or a hardware keyboard. The mapping SHALL be:
- one-finger drag: the tool's primary action
- two-finger drag: pan
- pinch: zoom
- long press: context menu with the actions of the tool's secondary and tertiary buttons
- Apple Pencil: precise primary actions
- an on-screen command bar: typed commands

A floating modifier key SHALL switch the next touch to middle-button or right-button semantics. Hardware keyboards and pointers on iPad SHALL get desktop bindings.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: Paint by touch
- **GIVEN** an iPad with no keyboard
- **WHEN** the user sets the box by dragging, then picks a layer and taps "paint" in the context menu
- **THEN** the same `paint` command SHALL be issued that Magic's middle-button action issues

### Requirement: Box tool touch gestures
On touch devices, with the box tool active, the mode-based mapping above SHALL give the box tool these gestures:
- a one-finger drag that starts inside the box SHALL move the whole box by the drag offset;
- a one-finger drag that starts on a corner handle of the box SHALL move that corner, following the corner dragging rules of capability `cmd-box`;
- a one-finger drag that starts anywhere else SHALL sweep a new box from the touch-down point to the lift point;
- paint or erase by example (Magic's middle button over the box) SHALL be an item of the long-press context menu.

Points SHALL be snapped as for pointer input (capability `cmd-coordinates-and-units`). Each gesture SHALL issue the same `box` or `paint` command that the corresponding Magic button issues, so the rules of capability `cmd-box` (and `cmd-paint-erase` for paint by example) apply to it and the command log records the same command (see Q-CMDEDIT-13, resolution: platform).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-5, docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-13; Magic ref: dbwind/DBWbuttons.c:DBWBoxHandler)

#### Scenario: Drag inside the box moves it
- **GIVEN** an iPad with no keyboard, the box tool active, snap `internal` and a box (0,0)–(10,10)
- **WHEN** the user drags one finger from root (5,5) to root (25,15)
- **THEN** the box SHALL be (20,10)–(30,20)

#### Scenario: Drag on a corner handle moves that corner
- **GIVEN** the same box
- **WHEN** the user drags one finger from the upper-right corner handle to root (30,40)
- **THEN** the box SHALL be (0,0)–(30,40)

#### Scenario: Drag elsewhere sweeps a new box
- **GIVEN** the same box
- **WHEN** the user drags one finger from root (50,50) to root (60,70)
- **THEN** the box SHALL be (50,50)–(60,70)

### Requirement: Command entry on every platform
Every platform SHALL provide a command entry for typing any command of the command API. It SHALL support history and completion of command names, cell names and layer names.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Typed command on iPad
- **WHEN** a user types `drc why` in the command bar on iPad
- **THEN** the DRC explanation SHALL be shown exactly as on desktop
