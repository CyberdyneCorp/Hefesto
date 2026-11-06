# Miscellaneous Layout Commands

## Purpose

Defines the remaining layout-window commands that are not owned by another capability: `xor`, `random`, `shell`, the command surface of `straighten`, and the build-specific commands `history`, `makesw` and `sgraph`. It also summarises commands owned elsewhere: `see`, `crosshair` and `findbox` (owned by capability `ui-windows-and-views`), `feedback` (owned by capability `ui-feedback-and-highlights`) and the layout wizard commands (owned by capability `ui-wizard-and-debug`). Where a summary and its owner differ, the owner governs. The plowing algorithm is in capability `plow`. Coordinates are in `cmd-coordinates-and-units` and the box is in `cmd-box`. The command surface is `behavioral`. The `xor` geometry is `exact`.

## Requirements

### Requirement: Xor command

`xor [-nolabels] [-nosubcircuits] [-novendor] DEST` SHALL compare the flattened contents of the edit cell (or of the window's top cell when there is no edit cell) against the existing cell DEST, modifying DEST in place:

1. DEST SHALL already exist. Otherwise the command reports `DEST does not exist`.
2. Every paint tile of the source, flattened through all levels (except instances excluded by `-nosubcircuits` or `-novendor`, which are copied as instances instead), SHALL be painted into DEST with this result rule: painting type T over existing type H gives **space** if T = H, and gives **T** otherwise. Where the source has no paint, DEST is unchanged.
3. Unless `-nolabels` is given, source labels SHALL be copied into DEST with hierarchical names, as `flatten` does.
4. The operation is not undoable.

So identical same-type areas vanish, source-only areas appear, and DEST-only areas remain: the symmetric difference for one type. Where the two cells have different types, the source type wins (see Q-CMDEDIT-94). Option words must start with `-no` and be longer than 3 characters. Any other word before DEST is a usage error. Magic tests the 4th character of the **first** option for every option given (see Q-CMDEDIT-95).

Compatibility: exact

(Magic ref: commands/CmdTZ.c:2421-2545)

#### Scenario: Xor of identical cells is empty
- **GIVEN** cells `a` and `b` with identical paint, and `a` is the edit cell
- **WHEN** `xor b` is run
- **THEN** `b` SHALL contain no paint

#### Scenario: Difference
- **GIVEN** the edit cell has metal1 (0,0)–(10,10), and `b` has metal1 (5,0)–(15,10)
- **WHEN** `xor b` is run
- **THEN** `b` SHALL contain metal1 at (0,0)–(5,10) and (10,0)–(15,10)

### Requirement: Random command

`random` SHALL return (scripting) or print a pseudo-random non-negative integer. `random seed [V]` SHALL seed the generator with integer V, or with the current time in seconds if V is omitted. Any other form prints `usage: random [seed [<value>]]`. Hefesto MAY use a different generator, but the same seed SHALL give the same sequence within Hefesto.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:89-135)

#### Scenario: Reproducible sequence
- **WHEN** `random seed 42` and `random` are run twice in sequence
- **THEN** both `random` results SHALL be equal

### Requirement: Shell command

`shell [COMMAND…]` SHALL run COMMAND (the remaining words joined by spaces) in an operating-system shell and show its output. This is host-specific (process spawning). On platforms without process spawning (iPadOS), Hefesto SHALL report that the command is unavailable (see Q-CMDEDIT-96).

Compatibility: informative

(Magic ref: commands/CmdRS.c:2736-2780)

#### Scenario: Unavailable platform
- **GIVEN** Hefesto on iPadOS
- **WHEN** `shell ls` is run
- **THEN** an "unavailable on this platform" error SHALL be reported and nothing else SHALL happen

### Requirement: See command

`see [no] [LAYERS|allSame]` SHALL change which layers the current layout window displays, as specified by capability `ui-windows-and-views`, requirement "Layer visibility (see)", which governs. In summary: `see LAYERS` shows and `see no LAYERS` hides the listed layers, `see` / `see no` alone affect all layers, and `allSame` controls drawing non-edit material in normal style. Visibility is per window and does not change layout data; it also affects `$` layer specs, `select visible` and paint-by-example.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:421-540)

#### Scenario: Hide a layer
- **WHEN** `see no metal1` is run
- **THEN** metal1 SHALL not be drawn in that window
- **AND** `select visible` SHALL not select metal1

### Requirement: Crosshair command

`crosshair X Y` SHALL show a horizontal and a vertical line through the point (X, Y) in root coordinates, and `crosshair off` SHALL remove it, as specified by capability `ui-windows-and-views`, requirement "Crosshair", which governs. The crosshair is display-only.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:3801-3850, dbwind/DBWtools.c:500-600)

#### Scenario: Crosshair
- **WHEN** `crosshair 10 20` is run
- **THEN** lines SHALL be displayed at x = 10 and y = 20

### Requirement: Findbox command

`findbox` SHALL scroll the current window, without changing its scale, so that the box is centered, and `findbox zoom` SHALL frame the box plus a margin of max(2, ⌊width/20⌋) in x and max(2, ⌊height/20⌋) in y, as specified by capability `ui-windows-and-views`, requirement "center and findbox", which governs (including the error messages).

Compatibility: behavioral

(Magic ref: commands/CmdFI.c:668-770)

#### Scenario: Zoom to box
- **GIVEN** the box (0,0)–(200,100)
- **WHEN** `findbox zoom` is run
- **THEN** the view SHALL be set to (−10,−5)–(210,105)

#### Scenario: Minimum findbox margin
- **GIVEN** the box (0,0)–(20,20)
- **WHEN** `findbox zoom` is run
- **THEN** the view SHALL be set to (−2,−2)–(22,22)

### Requirement: Feedback command

`feedback OPTION` SHALL manage feedback areas: highlighted root-coordinate areas with an explanatory text and a display style, used by DRC, extraction and other tools. The command, its options, messages and save-file format are owned by capability `ui-feedback-and-highlights` requirement "feedback command"; the summary below does not override it. Options (keyword-matched):

- `add TEXT [STYLE] [x y …]`: add an area. Without points it covers the box. With 1 point (2 values) it is a point. With 2 points (4 values) it is a line. With more points it is a polygon split into rectangles and triangles. STYLE is `dotted`, `medium`, `outline`, `pale` (default), `solid`, or a long display-style name. An odd number of coordinates is a usage error.
- `clear [SUBSTRING]`: remove all areas, or only those whose text contains SUBSTRING.
- `count`: return or print the number of areas.
- `find [N]`: set the box to the next (or N-th) area and print its text.
- `save FILE`: write the areas as a script of `feedback add` commands (each with its text in quotes and style name) that recreates them.
- `why`: print each distinct text of areas overlapping the box (in the box's root cell) once.
- `help`: print the options.

Feedback areas are not part of any cell and are not saved with layout.

Compatibility: behavioral

(Magic ref: commands/CmdFI.c:146-480, dbwind/DBWfdback.c:279-700)

#### Scenario: Add and count
- **GIVEN** no feedback
- **WHEN** `feedback add "check me"` and then `feedback count` are run in scripting mode
- **THEN** the result SHALL be `1`

### Requirement: Straighten command surface

`straighten DIR` SHALL straighten jogs in the box area by pulling material in the Manhattan direction DIR, using the plowing engine. The algorithm and its results are defined in capability `plow`. The edit cell and a box in a window on it are required. A non-Manhattan DIR is rejected.

Compatibility: behavioral

(Magic ref: plow/PlowCmd.c:337-400)

#### Scenario: Requires direction
- **WHEN** `straighten` is run without an argument
- **THEN** a usage error SHALL be reported

### Requirement: Wizard and debug commands

The readline `history` command and the LLNL-only `makesw` and `sgraph` are build-specific tools, and the layout wizard commands (`*coord`, `*extract`, `*plow`, `*psearch`, `*showmem`, `*showtech`, `*tilestats`, `*tsearch`, `*watch`; owned by capability `ui-wizard-and-debug`, requirement "Layout wizard commands") are internal debugging tools. Hefesto MAY omit them or provide its own diagnostics. They SHALL NOT be required for data compatibility.

Compatibility: informative

(Magic ref: dbwind/DBWcommands.c:200-230, commands/CmdWizard.c:86-1300, commands/CmdFI.c:1472-1550, dbwind/DBWcommands.c:603-611)

#### Scenario: Omitted debug command
- **WHEN** `*tilestats` is run in Hefesto
- **THEN** Hefesto MAY report an unknown command without any effect on the layout
