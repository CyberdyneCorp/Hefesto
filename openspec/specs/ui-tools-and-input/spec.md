# UI Tools and Input

## Purpose
Defines pointer tools and the default input bindings:
- the tool registry and the `tool` command
- button semantics of the `box`, `wiring`, `netlist` and `rsim` tools, and the Tcl `pick` tool
- cursor glyph selection and the `cursor` command
- the default key and button binding table installed by the system startup file
- how Hefesto maps these to touch input on iPadOS (decision X-5; the gesture vocabulary is owned by `hefesto-input-and-touch`)

Macro mechanics and key-name syntax are in `ui-command-dispatch`. The box commands are owned by `cmd-box`. Wire operations are owned by `wiring-tool`. Netlist operations are owned by `netlist-window`. Rsim is owned by `irsim-interface`.

## Requirements

### Requirement: Tool registry
The system SHALL keep at most 10 tools. Each tool SHALL have:
- a name
- a usage text
- a button handler, which may be absent for tools created by `macro copy`
- a cursor glyph index

Tools SHALL be registered in this order: `box` (cursor 0), `wiring` (cursor 19), `netlist` (cursor 18, only with the netlist module) and `rsim` (cursor 21, only with the rsim module). `box` SHALL be current at startup. Registering an 11th tool SHALL report `Can't add tool "T":  no space in button handler` and SHALL be ignored.

In layout windows, button presses and releases SHALL go to the current tool's handler. Key macros SHALL be looked up in the current tool's macro table (see ui-command-dispatch).

Compatibility: behavioral
(Magic ref: dbwind/DBWbuttons.c:52-170, dbwind/DBWprocs.c:1089-1117, wiring/wireOps.c:1120-1128, netmenu/NMmain.c:465-478, sim/SimRsim.c:922-934, utils/main.c:742-797)

#### Scenario: Default tool
- **WHEN** the application starts
- **THEN** the current tool SHALL be `box` and the normal cursor (glyph 0) SHALL be shown

### Requirement: tool command
The `tool` command SHALL behave as follows:
- `tool` with no argument SHALL switch to the next registered tool in registration order, wrapping around, and print `Switching to "T" tool.`. The first switch of a session SHALL add `  If you didn't really want to switch,` / `    type ":tool box" to switch back to the box tool.`.
- `tool NAME` SHALL select the tool whose name has NAME as a prefix (case-sensitive).
  - No match SHALL report `"NAME" isn't a tool name.` and list the legal names.
  - Several matches SHALL report `"NAME" is an ambiguous tool name.` and list the legal names.
- `tool info` SHALL print the current tool's usage text, or `(no usage information)` if it has none.
- `tool type` SHALL report `Current tool is "T".`.
- More than one argument SHALL print `Usage: tool [name|info]`.

Switching tools SHALL set the tool's cursor glyph.

In the Tcl build, `tool` is replaced by a script command whose tools are `box`, `wiring`, `nettool` and `pick`, each just a separate macro table. That command is specified in capability `ui-gui-managers`, requirement "Tool modes", which owns it.

Compatibility: behavioral (messages informative)
(Magic ref: commands/CmdTZ.c:667-733, dbwind/DBWbuttons.c:171-297, tcltk/tools.tcl:700-760)

#### Scenario: Cycle tools
- **GIVEN** the tools box, wiring, netlist and rsim are registered and box is current
- **WHEN** `tool` is executed twice
- **THEN** the current tool SHALL be `netlist`

#### Scenario: Unknown tool
- **WHEN** `tool foo` is executed
- **THEN** `"foo" isn't a tool name.` SHALL be reported, followed by `  The legal names are:` and the tool names

### Requirement: Box tool buttons
The `box` tool SHALL handle buttons in a layout window as follows. The box semantics (move, resize, nearest-corner choice, snapping) are owned by capability `cmd-box`, requirement "Box tool pointer gestures", which governs; this requirement adds the cursor glyphs.
- Pressing the middle button SHALL paint (or erase) under the box according to the material under the cursor, as defined by `paint cursor` (capability `cmd-paint-erase`, requirement "Paint or erase by example (cursor and pick)").
- Pressing the left button alone SHALL choose the lower-left corner and show the "box with lower-left corner" cursor (glyph 6). Releasing it SHALL move the box, without resizing, so that its lower-left corner is at the cursor point.
- Pressing the right button alone SHALL choose the upper-right corner and show the upper-right corner cursor (glyph 5). Releasing it SHALL move only that corner to the cursor point, resizing the box.
- If the other of left/right is pressed while one is held, the corner nearest the cursor SHALL become the chosen corner. The first button pressed SHALL still decide between move (left) and resize (right).
- Releasing one of two held buttons SHALL only update the cursor glyph.
- When the last button is released, the cursor SHALL return to glyph 0 and the move or resize SHALL run with snapping applied.

The corner cursor glyphs SHALL be:

| Mode | LL | LR | UL | UR |
|---|---|---|---|---|
| move | 6 | 7 | 8 | 9 |
| resize | 2 | 3 | 4 | 5 |

Compatibility: behavioral
(Magic ref: dbwind/DBWbuttons.c:355-478, dbwind/DBWprocs.c:1093-1101)

#### Scenario: Left click moves box
- **GIVEN** a box at (0,0)-(10,10)
- **WHEN** the left button is pressed and released at layout point (20,30)
- **THEN** the box SHALL be (20,30)-(30,40)

#### Scenario: Right click resizes
- **GIVEN** a box at (0,0)-(10,10)
- **WHEN** the right button is pressed and released at (25,5)
- **THEN** the box SHALL be (0,0)-(25,5)

#### Scenario: Move by upper-right corner
- **GIVEN** a box at (0,0)-(10,10)
- **WHEN** left is pressed, then right is pressed with the cursor nearest the upper-right corner, then right is released, then left is released at (50,50)
- **THEN** the box SHALL be (40,40)-(50,50)

### Requirement: Other tools' buttons
The other tools SHALL behave as follows:
- `wiring`: left = pick the wire layer and width under the cursor (`wire type`), right = add a wire leg to the cursor (`wire leg`), middle = place a contact and switch layers (`wire switch`). The cursor SHALL be glyph 19.
- `netlist`: left = select the net containing the terminal nearest the cursor, right = toggle that terminal in or out of the current net, middle = join the current net with that terminal's net. The cursor SHALL be glyph 18.
- `rsim`: left and right act as in the box tool, middle = display the Rsim node values of the selected paint. The cursor SHALL be glyph 21.

Tcl build defaults (informative), as button macros:

| Tool | Button1 | Button2 | Button3 | Shift_Button1 | Shift_Button2 | Shift_Button3 | Button4 / Button5 |
|---|---|---|---|---|---|---|---|
| box | `box move bl cursor` | `paint cursor` | `box corner ur cursor` | `box corner bl cursor` | `erase cursor` | `box move ur cursor` | scroll up/down 0.05 w |
| wiring | trackwire pick | trackwire done | trackwire cancel | `wire incr type ; wire show` | `wire switch` | `wire decr type ; wire show` | `wire incr width ; wire show` / `wire decr width ; wire show` |
| nettool | `netlist select` | `netlist join` | `netlist terminal` | (none) | (none) | (none) | scroll up/down 0.05 w |
| pick | keep selection | start pick-drag | cancel selection | `box corner bl cursor` | start copy-drag | `box move ur cursor` | scroll up/down 0.05 w |

For the box tool, Control_Button1, Control_Button2 and Control_Button3 are `box move bl cursor`, `paint cursor` and `box corner ur cursor`. Control_Button1 is `trackwire current` in wiring.

Compatibility: behavioral (non-Tcl handlers); informative (Tcl table)
(Magic ref: wiring/wireOps.c:1120-1128, netmenu/NMmain.c:465-478, sim/SimRsim.c:922-934, tcltk/tools.tcl:519-584, magic/proto.magicrc.in:274-298)

#### Scenario: Wiring tool left click
- **GIVEN** the wiring tool is current and the cursor is over 3-wide metal1
- **WHEN** the left button is clicked
- **THEN** the wire type SHALL become metal1 with width 3 (see wiring-tool)

### Requirement: cursor command
`cursor` SHALL report the cursor position in the pointed window as two values, `X Y`. The default units are the current print units, or internal units when the units are `default`. An optional argument selects:
- `internal`, `lambda`, `user`, `grid`, `microns`: the layout position in those units, snapped when snapping is not internal
- `window`: window pixel coordinates
- `screen`: screen pixel coordinates

`cursor N` with an integer N SHALL set the cursor glyph to N. An unknown option SHALL print `Usage: cursor glyphnum` and ` (or): cursor [internal | lambda | microns | user | window | units]`.

Compatibility: behavioral
(Magic ref: windows/windCmdAM.c:460-590)

#### Scenario: Set glyph
- **WHEN** `cursor 22` is executed
- **THEN** the pointer SHALL show glyph 22 (the pick cursor)

### Requirement: Default key bindings
The system startup file SHALL install the following bindings into the `box` tool table. This is the "new macro" set, which is the default build configuration. The table is the required default for keyboard input on all platforms.

| Key | Command | Key | Command |
|---|---|---|---|
| a | `select visible` | A | `select more visible` |
| ^A | `select less visible` | b | `box` |
| B | `findbox` | c | `copy` |
| d | `delete` | ^D | `erase $` |
| e | `edit` | f | `sideways` |
| F | `upsidedown` | g | `grid` |
| G | `grid 2` | i | `select cell` |
| I | `select more cell` | Control_XK_i | `select less cell` |
| l | imacro `label ` (readline or Tcl builds) | L | `shell ls` |
| ^L | `redraw` | m | `move` |
| M | `stretch` | ^N | (empty: deleted) |
| o | `openwindow` | O | `closewindow` |
| p | imacro `paint ` (readline or Tcl builds) | Control_Shift_XK_q | `quit` |
| r | `clockwise` | R | `clockwise 270` |
| ^R | `clockwise 180` | s | `select` |
| S | `select more` | ^S | `select less` |
| Control_Shift_XK_s | `undo ; select` | u | `undo` |
| U | `redo` | v | `view` |
| V | `xview` | w | `writeall` |
| W | `writeall force` | x | `expand` |
| X | `unexpand` | ^X | `expand toggle` |
| z | `zoom .5` | Z | `zoom 2` |
| ^Z | `findbox zoom` | Control_Shift_XK_z | `center` |
| ? | `drc why` | ! | `drc find; findbox zoom; zoom 16` (without readline); imacro `shell ` (with readline) |
| / | `select area; what ; select clear` | , | `select clear` |
| " " (space) | `tool` | XK_colon, XK_semicolon | imacro `:` (opens the command prompt) |
| Shift_XK_space | `tool box` | Control_XK_space | `tool wiring` |

Arrow keys:

| Key | Command |
|---|---|
| XK_Left / Right / Up / Down | `scroll l/r/u/d .1 w` |
| Shift_ + arrow | `scroll l/r/u/d 1 w` |
| Control_XK_Left / Right / Up / Down | `box grow w/e/n/s 1i` |
| Control_Shift_XK_Left / Right / Up / Down | `box shrink e/w/s/n 1i` |

Pointer wheel:

| Key | Command |
|---|---|
| XK_Pointer_Button4 / 5 | `scroll u .05 w` / `scroll d .05 w` |
| Shift_ + Button4 / 5 | `scroll l .05 w` / `scroll r .05 w` |
| Control_ + Button4 / 5 | `zoom 1.6` / `zoom 0.625` |

Keypad:

| Key | Command |
|---|---|
| KP_Delete | `box size 0 0` |
| KP_Insert | `box size 4l 4l` |
| KP_0, Shift_KP_0, XK_0 | `box size 7l 2l` |
| Control_KP_0, Control_KP_Insert | `box size 2l 7l` |
| KP_End, KP_Next, KP_Home, KP_Prior | `move sw/se/nw/ne 1i` |
| KP_Down / Left / Right / Up | `move d/l/r/u 1i` |
| KP_2 / 4 / 6 / 8 and XK_2 / 4 / 6 / 8 | `stretch d/l/r/u 1i` |
| Shift_KP_2 / 4 / 6 / 8 | `stretch d/l/r/u 1` |
| Shift_KP_Down / Left / Right / Up | `move d/l/r/u 1` |
| Control_KP_Down / Left / Right / Up | `stretch d/l/r/u 1i` |
| KP_Begin | `findbox zoom` |
| KP_5, Shift_KP_5, XK_5 | `findbox` |

Function keys F1–F12: `paint ndiff`, `pdiff`, `poly`, `poly2`, `m1`, `m2`, `m3`, `m4`, `ndc`, `pdc`, `pc`, `via`.

Buttons for all tools in the non-Tcl build: Control_Button1 `box move bl cursor`, Control_Button2 `paint cursor`, Control_Button3 `box corner ur cursor`. The box-tool button macros Button1–3 and Shift_Button1–3 are as in the Tcl table above. In the Tcl build these commands are prefixed with `*bypass`.

Color window: Button1 / Button2 / Button3 `pushbutton left/middle/right`, u `undo`, U `redo`, plus `color next`, minus `color last`.

Netlist window: Button1 / Button2 / Button3 `pushbutton left/middle/right`.

The startup file SHALL also load the label fonts `FreeSans.pt3` (0.58), `FreeSerif.pt3` (0.58) and `FreeMono.pt3` (0.6). The "original" macro set, used when new macros are disabled at build time, is informative only.

Compatibility: behavioral (commands bound); key names are exact strings for `macro` round-tripping
(Magic ref: magic/proto.magicrc.in:11-318, scripts/configure.in:1304-1312)

#### Scenario: s selects
- **GIVEN** the default startup file with the box tool current
- **WHEN** the user presses `s` in a layout window
- **THEN** `select` SHALL run

#### Scenario: Ctrl-wheel zoom
- **WHEN** the user turns the wheel up with Control held
- **THEN** `zoom 1.6` SHALL run, which zooms out

### Requirement: Key event normalization
A key event SHALL be turned into a macro key as follows:
- Modifier-only key presses SHALL be ignored.
- For a key with an ASCII value, the system SHALL drop a lone Shift, or a lone Control, when neither Lock nor Meta is present. Ctrl+letter SHALL then become the control code, so Ctrl-A is `^A`.
- For keypad keys, Shift SHALL be dropped and the keysym kept, so that `KP_4` and `4` differ.
- Keys without an ASCII value SHALL keep all modifiers with their keysym.
- On macOS, any of Mod1–Mod5 SHALL count as Meta.
- Ctrl-C SHALL raise an interrupt. A second Ctrl-C while an interrupt is pending SHALL exit the program with status 0.
- Mouse buttons 1–5 SHALL be treated as keys named `Pointer_Button1` to `Pointer_Button5`, carrying the same modifiers.

Compatibility: behavioral
(Magic ref: graphics/grTOGL1.c:505-610)

#### Scenario: Keypad distinguished
- **GIVEN** `XK_KP_4` is bound to `stretch l 1i`
- **WHEN** keypad 4 is pressed with Num Lock on
- **THEN** `stretch l 1i` SHALL run

### Requirement: Touch and pointer gesture mapping
Hefesto SHALL let every tool action, key binding and button binding be reached on platforms without mouse buttons or a physical keyboard (iPadOS touch). It SHALL do this through a gesture and on-screen-control mapping that dispatches the same commands as the bindings above.

The mapping SHALL keep these semantics, which follow decision X-5 and are specified in full by capability `hefesto-input-and-touch` (requirement "Touch and Pencil mapping"), which owns the gesture vocabulary:
- a one-finger tap or drag, or the Apple Pencil, is the primary button (Button1) of the current tool
- a long press opens a context menu with the actions of the tool's secondary and tertiary buttons (Button2 and Button3)
- a floating modifier key switches the next touch to middle-button (Button2) or right-button (Button3) semantics, and provides the Shift/Control variants
- two-finger pan is `scroll` by the dragged distance
- pinch is `zoom` about the pinch center

The mapping SHALL be user-rebindable through the same macro tables (see Q-UI-22).

Compatibility: behavioral
(Magic ref: magic/proto.magicrc.in:247-298, dbwind/DBWbuttons.c:355-478)
(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: Tap moves the box
- **GIVEN** Hefesto on iPadOS with the box tool current
- **WHEN** the user taps at layout point (20,30)
- **THEN** the same effect as `box move bl cursor` at (20,30) SHALL occur

#### Scenario: Pinch zoom
- **WHEN** the user pinches out by a factor of 2 around the view center
- **THEN** the view SHALL change as by `zoom 0.5`
