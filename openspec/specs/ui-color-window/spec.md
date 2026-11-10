# UI Color Window

## Purpose
Defines the `color` window client. It edits one entry of the session color map interactively, using RGB and HSV bars, and loads or saves color-map files. The file format is in `display-style-files`. Generic window behavior is in `ui-windows-and-views`.

## Requirements

### Requirement: Color window creation and layout
`specialopen color [OCTAL]` SHALL open a color window that edits color index OCTAL. OCTAL is read as octal, ANDed with 0o377, and defaults to 0.

Its frame SHALL be the 250 × 200 pixels at the bottom right of the screen ((x1−250, 0)–(x1, 200)), replacing any frame `specialopen` was given (CMWcreate). The window SHALL have no scrolling, no scroll bars and no caption bar. Its surface SHALL always show the fixed area (0,1500)-(24000,17000). That area SHALL contain six labeled bars, each with a decrement pump `-` on its left and an increment pump `+` on its right:

| Bar | Bar rectangle | Name rectangle | Pumps (−, +) |
|---|---|---|---|
| Red | (2000,8000)-(10000,9000) | (2000,9500)-(10000,10500) | (500,8000)-(1500,9000), (10500,8000)-(11500,9000) |
| Green | (2000,5000)-(10000,6000) | (2000,6500)-(10000,7500) | (500,5000)-(1500,6000), (10500,5000)-(11500,6000) |
| Blue | (2000,2000)-(10000,3000) | (2000,3500)-(10000,4500) | (500,2000)-(1500,3000), (10500,2000)-(11500,3000) |
| Hue | (14000,8000)-(22000,9000) | (14000,9500)-(22000,10500) | (12500,8000)-(13500,9000), (22500,8000)-(23500,9000) |
| Saturation | (14000,5000)-(22000,6000) | (14000,6500)-(22000,7500) | (12500,5000)-(13500,6000), (22500,5000)-(23500,6000) |
| Value | (14000,2000)-(22000,3000) | (14000,3500)-(22000,4500) | (12500,2000)-(13500,3000), (22500,2000)-(23500,3000) |

A swatch of the edited color SHALL fill (6000,12000)-(18000,15000), with the text `Color Being Edited` centered in (6000,15500)-(18000,16500). Each bar SHALL be filled from its left edge to the fraction equal to its component value. The fills SHALL use the red, green, blue, yellow, gray and brown-1 styles respectively. All outlines and texts SHALL use the bounding-box style.

The RGB values shown SHALL be (c + 0.5)/255. HSV SHALL be derived from these values (see Requirement: RGB/HSV conversion). The window caption SHALL be `COLOR = 0<octal index>`.

Compatibility: behavioral
(Magic ref: cmwind/CMWmain.c:92-132, cmwind/CMWmain.c:152-191, cmwind/CMWmain.c:248-375, cmwind/CMWmain.c:376-410)

#### Scenario: Caption shows octal
- **WHEN** `specialopen color 47` is executed
- **THEN** the window SHALL edit color 39 and its caption SHALL be `COLOR = 047`

#### Scenario: Frame at the bottom right of the screen
- **GIVEN** a 512 × 512 screen (x1 = 511)
- **WHEN** `specialopen 10 10 300 300 color` is executed
- **THEN** the color window's frame SHALL be (261,0)-(511,200), not the frame given on the command line

### Requirement: Color window buttons
In a color window, a button press SHALL act on the layout point under the cursor:
- On a bar, the component SHALL be set to (x − bar.xbot)/(bar.xtop − bar.xbot), with x clamped to the bar.
- On a pump, the left button SHALL add the pump amount and any other button SHALL add 5× the amount. The amount is ±0.0078 for R, G and B, and ±0.01 for H, S and V.
- Informative: in Magic's non-Tcl build, a press on the swatch starts a pick. The press prints `Release button over new color to edit.` (left) or `Release button over color to copy.` (other buttons). The release then reads the pixel value under the cursor. Left loads that color index for editing. Other buttons copy its RGB into the edited color. Magic's Tcl build, which is normative, does not start a pick from the swatch.
- Hefesto SHALL NOT provide a screen-pixel color picker (eyedropper) in the color window, in either `magic-compat` mode. The normative Tcl build compiles picking out, and under the translucent OpenGL default of X-5 a screen pixel blends several styles rather than naming one colormap entry; picking a layer's display style from the layout belongs to the design-system and UX scope, not to Magic compatibility (see Q-UI-37, resolution: other).

Compatibility: behavioral
(Magic ref: cmwind/CMWcmmnds.c:141-298, cmwind/CMWcmmnds.c:cmwButtonDown; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-37)

#### Scenario: Click red bar midpoint
- **GIVEN** a color window editing a color with RGB (0,0,0)
- **WHEN** the left button is pressed at layout point (6000,8500)
- **THEN** red SHALL become round(0.5×255) = 128 and green and blue SHALL stay 0

### Requirement: Color component update
Updating component k to value x SHALL work as follows:
1. Compute the current values r/255, g/255, b/255 and their HSV.
2. Set (replace) or add (increment) x to component k.
3. Clamp the result to [0,1].
4. If k is H, S or V, recompute RGB from HSV.
5. Store each channel as ⌊v×255 + 0.5⌋.

The color entry's name SHALL be cleared. The change SHALL be recorded as one undoable event holding the old and new RGB. Every color window editing that index SHALL be redrawn. The session SHALL be marked "color map modified".

Compatibility: behavioral
(Magic ref: cmwind/CMWcmmnds.c:541-646, graphics/grCMap.c:384-427, cmwind/CMWundo.c:107-180)

#### Scenario: Undo a color change
- **GIVEN** color 5 is (184,73,83) and its red is raised by the `+` pump
- **WHEN** `undo` is executed in the color window
- **THEN** color 5 SHALL return to (184,73,83)

### Requirement: RGB/HSV conversion
RGB→HSV SHALL use:
```
v = max(r,g,b); d = v - min(r,g,b)
if v == 0 or d == 0: s = 0, h = 0
else s = d / v
     mr=(v-r)/d, mg=(v-g)/d, mb=(v-b)/d
     h = (r==v) ? mb-mg : (g==v) ? 2+mr-mb : 4+mg-mr
     h = h/6; if h < 0: h += 1
```

HSV→RGB SHALL use:
```
i = trunc(h*6), f = h*6-i, m = v(1-s), n = v(1-s·f), k = v(1-s(1-f))
then by i mod 6:
  0: (v,k,m)   1: (n,v,m)   2: (m,v,k)   3: (m,n,v)   4: (k,m,v)   5: (v,m,n)
```

Compatibility: exact
(Magic ref: cmwind/CMWrgbhsv.c:48-135)

#### Scenario: Pure red hue
- **WHEN** RGB (1,0,0) is converted
- **THEN** h SHALL be 0, s SHALL be 1 and v SHALL be 1

### Requirement: Color window commands
The color client SHALL register `color`, `load`, `pushbutton` and `save`.

`color`:
- With no argument, it SHALL print `Current color is %o octal (%d decimal) (red = %d, green = %d, blue = %d)`.
- `color N` (C-integer syntax: decimal, 0x hex, leading-0 octal) SHALL select index N.
- `next` and `last` SHALL step by one with wrap-around. Only the first 4 letters are checked.
- `get` SHALL return the index as an integer (informative: Magic's non-Tcl build prints `Color index %d`).
- `rgb` SHALL return the list of the three components (informative: Magic's non-Tcl build prints `Color RGB=%d %d %d`).
- An index outside [0, size) SHALL report `The colormap only has values from 0 to MAX (decimal).`.
- Other arguments SHALL report `Usage: color [#|next|last|get|rgb]`.

`load [T D M]`:
- It SHALL first ask to discard unsaved color changes.
- It SHALL then read the color map for the given style, display and monitor types, or for the current ones. The search path is `.` and then the system library path.

`save [T D M]` SHALL write the color map (see display-style-files) and clear the modified mark on success.

`load` and `save` SHALL accept exactly 0 or 3 arguments. Otherwise they print `Usage: <cmd> [techStyle displayStyle monitorType]`.

`pushbutton left|middle|right` SHALL simulate a press of that button at the current point. No release SHALL be generated, as in Magic: every color-window action fires on press, and the release handler is reached only after a press over the current-color swatch (see Q-UI-37, resolution: other, replicate press-only).

Compatibility: behavioral
(Magic ref: cmwind/CMWmain.c:411-437, cmwind/CMWcmmnds.c:300-540, cmwind/CMWcmmnds.c:cmwPushbutton; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-37)

#### Scenario: Next wraps
- **GIVEN** a 256-entry map and a color window on index 255
- **WHEN** `color next` is executed
- **THEN** the window SHALL edit index 0

#### Scenario: pushbutton is press-only
- **GIVEN** a color window editing a color with RGB (0,0,0) and the current point at layout point (6000,8500) on the red bar
- **WHEN** `pushbutton left` is executed
- **THEN** red SHALL become 128, as for a pointer press
- **AND** no release event SHALL be delivered to the color window

#### Scenario: Save default name
- **GIVEN** styletype `mos`, the OpenGL display class and monitor `std`
- **WHEN** `save` is executed in the color window
- **THEN** the file `mos.OpenGL.std.cmap` SHALL be written

### Requirement: Unsaved color map protection
If the color map was modified since the last save or load, quitting and `load` SHALL ask `The color map has been modified.` / `  Do you want to lose the changes? ` with the answers `no` (default) and `yes`. Answering `no` SHALL cancel the quit or load.

Compatibility: behavioral
(Magic ref: cmwind/CMWcmmnds.c:648-659, windows/windCmdNR.c:262-334)

#### Scenario: Quit vetoed
- **GIVEN** a color was edited and not saved
- **WHEN** `quit` is executed and the user answers `no`
- **THEN** the application SHALL keep running
