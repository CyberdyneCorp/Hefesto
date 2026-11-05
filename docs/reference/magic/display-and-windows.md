# Display and Windows

## Purpose
Magic draws layout through a device-independent graphics layer with interchangeable backends:
- X11, or Tk in Tcl builds
- OpenGL, or TOGL in Tcl builds
- Cairo, Tcl builds only
- NULL

Windows belong to clients (layout, color map, netlist, 3D). The tech file's `styles` section names a display-style family, and the `.dstyle`, `.cmap` and glyph files for that family set how layers are drawn.

## Requirements

### Requirement: Window management
The system SHALL allow at most 31 layout windows. Exceeding the limit SHALL produce "Can't have more than %d windows.".

Windows SHALL have a scrollbar, caption and border by default. Layout window captions SHALL read `<root> EDITING <edit>` or `<root> [NOT BEING EDITED]`.

Window commands SHALL include `openwindow`, `closewindow`, `specialopen <type>`, `over`, `underneath`, `zoom`, `view`, `xview`, `scroll`, `center`, `windowcaption`, `windowborder`, `windowscrollbars` and `redraw`. (src: windows/windMove.c:49-316, commands/CmdSubrs.c:1016-1098, windows/windCmdSZ.c)

#### Scenario: Zoom bounds
- **WHEN** `zoom 25` is run
- **THEN** the system SHALL report "zoom factor must be between 0 and 20."

#### Scenario: Single netlist window
- **WHEN** a second netlist window is opened
- **THEN** the system SHALL report "Sorry, can't have more than one netlist menu at a time."

### Requirement: Display backends
The display type SHALL be chosen with `-d`, or else from the `displays` file, or else XWIND when `DISPLAY` is set and NULL otherwise. Which backends are compiled in SHALL depend on the build:
- Tcl builds: Tk (replacing X11), TOGL (replacing OpenGL) and Cairo
- non-Tcl builds: X11 and OpenGL

(src: graphics/grMain.c:102-426, scripts/configure.in:1471-1521)

#### Scenario: Headless run
- **WHEN** `-d NULL` is given
- **THEN** no windows SHALL be drawn, and all non-graphical commands SHALL work

### Requirement: Display style files
When a technology is loaded, the system SHALL read these files for its `styletype`, searching the system path:
1. `<styletype>.<disp>.<mon>.cmap`
2. `<styletype>.<disp>.dstyle`
3. the cursor glyphs

`<disp>` SHALL be `7bit`, `24bit` or `bw` for X11/Tk, chosen by the screen's color depth. It SHALL be `OpenGL` for OpenGL and Cairo.

A dstyle file SHALL contain the sections `display_styles`, `layout_styles`, `pale_styles` and `stipples`, and MAY contain a `version` section. Version 7 and later SHALL use hexadecimal masks. (src: dbwind/DBWdisplay.c:1726-1772, graphics/grDStyle.c:405-645, graphics/grCMap.c:122-219)

#### Scenario: Missing colormap
- **WHEN** the cmap file cannot be found
- **THEN** the system SHALL report "Couldn't open color map file \"%s.cmap\""

### Requirement: Crosshair
`crosshair x y` SHALL draw a crosshair at the given root coordinates, in every window that shows the same root cell. `crosshair off` SHALL hide it. (src: dbwind/DBWtools.c:426-602)

#### Scenario: Show crosshair
- **WHEN** `crosshair 100 100` is run
- **THEN** a crosshair SHALL be drawn at (100,100) in all windows that share the root cell

### Requirement: Tools
Button behavior SHALL be determined by the current tool. The tools SHALL be `box` (the default), `wiring`, `netlist` and `rsim`, and at most 10 tools can exist.

`tool [name]` SHALL select a tool by name or cycle to the next one. In the Tcl build, `tool` SHALL be provided by `tools.tcl`. (src: dbwind/DBWbuttons.c:52-257, commands/CmdTZ.c:667-695)

#### Scenario: Unknown tool
- **WHEN** `tool foo` is run
- **THEN** the system SHALL report "\"foo\" isn't a tool name." and list the valid tools

### Requirement: Color map editing
The color window (`specialopen color`) SHALL support:
- `color [#|next|last|get|rgb]`
- `load` and `save` of the color map, which by default use the current style type, display type and monitor

(src: cmwind/CMWmain.c:413-436, cmwind/CMWcmmnds.c)

#### Scenario: Save colormap
- **WHEN** `save` is run in the color window
- **THEN** the current color map SHALL be written to `<styletype>.<disp>.<mon>.cmap`
