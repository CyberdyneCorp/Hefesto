# Layout Editing

## Purpose
Interactive editing is centered on three things:
- **The box:** a rectangular cursor region.
- **The edit cell:** the only cell that edits modify, viewed within a root cell.
- **The selection:** kept in the internal cell `__SELECT__`.

Commands paint and erase under the box, select material, transform the selection, and manage the cell hierarchy.

## Requirements

### Requirement: Box tool
`box` SHALL support these forms:
- `width`, `height`, `size`, `position`, `values`, `remove`, `select`, `exists`, `help`
- `move|grow|shrink|corner <dir> <dist>|cursor`
- `llx lly urx ury`
- a trailing `-edit` to report in edit-cell coordinates

With no arguments it SHALL print the box in microns, lambda and internal units. The default box tool button bindings SHALL be:
- left: move the box by its lower-left corner
- right: move the upper-right corner
- middle: paint the layers under the cursor into the box, or erase if there are none

(src: commands/CmdAB.c:680-1316, dbwind/DBWbuttons.c:449-520, commands/CmdLQ.c:989-1066)

#### Scenario: Set box by coordinates
- **WHEN** `box 0 0 10 20` is run
- **THEN** the box SHALL be 10 × 20 units with its lower-left corner at the origin

### Requirement: Paint and erase
`paint <layers>` SHALL paint the box area in the edit cell. `erase [layers]` SHALL erase it. Both SHALL schedule DRC.

With no arguments, `erase` SHALL erase all paint and labels but not subcells. Subcells SHALL be erased only when `subcell` is listed.

Layer lists SHALL accept these special names, plus `+` and `-` modifiers:
- `$`: the layers under the cursor
- `*`: all layers except labels and cells
- `errors`, `labels`, `subcell`, `connect`

(src: commands/CmdLQ.c:896-984, commands/CmdE.c:620-800, commands/CmdSubrs.c:486+)

#### Scenario: Paint a label layer
- **WHEN** `paint labels` is run
- **THEN** the system SHALL report "Label layer cannot be painted.  Use the \"label\" command"

### Requirement: Selection
`select` SHALL support these forms:
- the qualifiers `more`, `less`, `nocycle`, `same`
- `area`, `visible`, `cell`, `labels`, `intersection`, `clear`, `flat`, `keep`, `move`, `pick`
- `save <file>`, `feedback`, `bbox`, `box`
- `chunk`, `region`, `net`, `short a b`
- a trailing `at x y`

A plain `select` SHALL cycle through the tile types under the cursor. Repeated clicks at the same spot SHALL grow the selection from chunk to region to net. Over empty space, `select` SHALL pick a subcell. Selecting from a different root cell SHALL clear the previous selection. (src: commands/CmdRS.c:756-1752, select/selCreate.c:110-258)

#### Scenario: Net selection
- **WHEN** `select net` is run with the cursor on a wire
- **THEN** all electrically connected material SHALL be selected

#### Scenario: Shortest short path
- **WHEN** `select short A B` is run and no conducting path connects the labels
- **THEN** the system SHALL report "No shorting path found between source and destination!"

### Requirement: Selection operations
The selection SHALL support these operations:
- `delete`
- `copy [dir [amt]]|to x y` and `move [dir [amt]]|to x y`; a bare direction moves 1 lambda
- `move origin`
- `stretch`
- `clockwise [deg]` (alias `rotate`), `sideways`, `upsidedown`, `orient`
- `array`

These operations SHALL modify only edit-cell material. For material outside the edit cell they SHALL print a warning. (src: select/selOps.c:99-165, commands/CmdCD.c:2653-2802, commands/CmdLQ.c:673-890)

#### Scenario: Copy to cursor
- **WHEN** `copy` is run with no arguments
- **THEN** the selection SHALL be copied by the offset from the box's lower-left corner to the cursor, and the command SHALL be logged as `copy to X Y`

### Requirement: Cell hierarchy management
The system SHALL provide these commands (src: commands/CmdLQ.c:391-560, dbwind/DBWprocs.c:286-640, commands/CmdE.c:82-195, commands/CmdFI.c:1067-1130):
- `load name [scaled n [d]] [-nowindow|-dereference|-force|-quiet|-fail]`
- `edit`, `down`
- `getcell`/`dump cell [child ref] [parent ref] [transform]`
- `expand [toggle]`, `unexpand`
- `flatten`
- `save [file]`, `writeall [force]`
- `flush`
- `cellname`/`instance`
- `path`, `addpath`
- `locking`

`load` of a cell that does not exist SHALL print "Creating new cell". `edit` SHALL refuse a read-only cell.

#### Scenario: Exit with unsaved changes
- **WHEN** `quit` is run while cells are modified
- **THEN** the system SHALL ask "N Magic cells have been modified. Do you want to exit magic and lose them?" (src: commands/CmdSubrs.c:1188-1207)

#### Scenario: Self-placement
- **WHEN** `getcell` would place a cell exactly on a copy of itself
- **THEN** the system SHALL report "Can't place a cell on an exact copy of itself."

### Requirement: Labels, ports and properties commands
The system SHALL provide `label text [pos [layer]]`, `setlabel`, `port`, `property`, `findlabel`, `goto` and `element`. These commands SHALL create and edit labels, port attributes, cell properties and decorative elements. (src: dbwind/DBWcommands.c:379,447,416,424,339,358,309)

#### Scenario: Make a port
- **WHEN** a label is selected and `port make` is run
- **THEN** the label SHALL get the next free port index

### Requirement: Grid and scaling
`grid [on|off|state|box|multiple n|xs [ys [xo yo]]]` SHALL control the display grid. Each new window SHALL start with a 1×1 grid at the origin.

`scalegrid a b` SHALL change the internal grid as follows:
- multiply all geometry by b/a
- rescale all technology tables
- flush undo, so the change cannot be undone
- reject grids finer than the process limit

(src: commands/CmdFI.c:1304-1468, commands/CmdRS.c:265-394)

#### Scenario: Grid too fine
- **WHEN** `scalegrid` would go below the process grid limit
- **THEN** the system SHALL report "Grid scaling is finer than limit set by the process!"
