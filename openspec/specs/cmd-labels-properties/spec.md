# Label, Port, Property and Annotation Commands

## Purpose

Defines the commands that create and edit labels and ports, query and set cell properties, locate labels and nodes, and draw non-mask annotation elements: `label`, `setlabel`, `port`, `property`, `findlabel`, `goto` and `element`. How labels, ports and properties are stored in `.mag` files is defined by capability `cell-file-format`. Coordinates and units are in `cmd-coordinates-and-units`. The command surface is `behavioral`. Stored label and port values (rectangle, justification, size and offset units, port indices and flags) are `exact`.

## Requirements

### Requirement: Label units and attributes

A label SHALL have:

- text, which cannot contain space or `/`, and cannot be empty;
- a rectangle in edit-cell internal units (it may have zero size);
- an attached type, where space means "unattached";
- a justification (one of the 9 positions);
- a font, where −1 means the default non-scalable font;
- a size in **eighths of an internal unit**;
- a rotation in integer degrees;
- an offset (x, y) in eighths of an internal unit;
- flags (sticky and port attributes);
- a port index.

Size and offset values typed in commands SHALL be parsed as distances (capability `cmd-coordinates-and-units`), multiplied by 8, and rounded to an integer. When a font is assigned to a label of size 0, its size SHALL become `L1` eighths. When reported, size and offset SHALL be divided by 8 with integer truncation and then formatted per `units`.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:236-300, commands/CmdRS.c:1820-1840, commands/CmdRS.c:2030-2045)

#### Scenario: Size in eighths
- **GIVEN** units `internal`
- **WHEN** `label A 0 2` is run with font 0 loaded
- **THEN** the stored size SHALL be 16

### Requirement: Label command

`label TEXT [POS [LAYER]]` or `label TEXT FONT [SIZE [ROTATION [OFFX OFFY [POS [LAYER]]]]]` SHALL create a label in the edit cell, with its rectangle equal to the box (edit coordinates):

- The second word is taken as POS if it is a direction name. Otherwise it is FONT: an integer font index (an out-of-range index is reported), or a font name (unknown names fail).
- POS is transformed from root to edit orientation.
- OFFX and OFFY may also be given as one word "x y". Non-numeric offsets fail.
- A LAYER prefixed with `-` makes the label **sticky**. An unknown LAYER fails with "Unknown layer".
- Defaults come from the default label set by `setlabel -default`, otherwise: POS automatic, font −1, size 0 (or `L1` when a font is given), rotation 0, offset 0, not sticky, layer automatic.
- **Automatic justification**: let the cell bounding box be split into thirds on each axis (`t = size/3`, integer division). With the label centre `(cx, cy)` (integer division), if `cx ≤ xbot+t` the column is "left", if `cx ≥ xtop−t` it is "right", otherwise "middle". Rows are chosen the same way. The justification SHALL be: left column → NE / E / SE (bottom / middle / top row); right column → NW / W / SW; middle column → N / N / S.
- **Automatic layer**: the label is attached to the most appropriate type under its rectangle, using the label-layer rules of the layout database.
- Any existing label in the edit cell with the same rectangle and text SHALL be replaced.
- Labels are re-attached in the box area, and the bounding box is recomputed. The change is undoable.

Fewer than 1 or more than 8 arguments SHALL print usage. Illegal text SHALL fail ("Can't have null label name." or an illegal-character error).

Compatibility: exact

(Magic ref: commands/CmdLQ.c:86-382, database/DBlabel.c:135-200)

#### Scenario: Automatic justification
- **GIVEN** an edit cell with bounding box (0,0)–(90,90) and the box (5,5)–(5,5)
- **WHEN** `label IN` is run
- **THEN** the label SHALL be justified NE (`t` = 30, and (5,5) is in the left column and bottom row)

#### Scenario: Sticky layer
- **WHEN** `label VDD n -metal2` is run
- **THEN** the label SHALL be attached to metal2, justified north, and sticky

### Requirement: Setlabel command

`setlabel [-default] OPTION [VALUE…]` SHALL query or change attributes of the selected labels that are in the edit cell, or, with `-default`, of the default label used by later `label` commands. Options (keyword-matched):

| Option | Set value | Query result (per label) |
|---|---|---|
| `text` | new text. The label is re-created with the same attributes. Not allowed with `-default`. | text |
| `font` | font name or index (`default` = −1). A name not loaded is loaded from a file, with an optional scale. | font name or `default` |
| `fontlist` | — | the names of the loaded fonts |
| `justify` | a direction name | the position name |
| `layer` | type name | the long type name |
| `offset` | `x y` (one or two words, ×8) | `x y` (÷8 each; see Q-CMDEDIT-90) |
| `rotate` | integer degrees | integer |
| `size` | distance (×8) | value ÷8 |
| `sticky` | `yes true on 1` / `no false off 0` | boolean |
| `box` | `llx lly urx ury` (one or four words). Not allowed with `-default`. | the four values |
| `help` | — | option list |

In scripting mode a query SHALL append one element per selected label to the result list; with `-default` the query result is the default label's value, size and offset divided by 8 like the per-label queries (Q-MISC-23), the justification `*ILLEGAL*` and the layer `default` while they are unset. Values are formatted per `units`. Setting a value SHALL change only labels whose value differs, record undo, re-render, and mark the cell modified. The change is also applied to the selection's copy of each label. More than two value words (five for `box`) SHALL print usage. The four `box` coordinates: with `magic-compat` ON (the default) they SHALL be parsed as relative distances, as in Magic, so `g` operands (and un-suffixed operands under `units user`) do not add the grid origin; with it OFF they SHALL be parsed as absolute coordinates, adding the grid origin (see Q-CMDEDIT-91, resolution: replicate in magic-compat, fix in native); a single `box` word holds four values or two (a point), anything else prints `Label box requires four coordinates.` Errors print Magic's messages and change nothing: an unknown or ambiguous option `Unknown setlabel option "<word>"`, a bad position the position-name list, an unknown layer `Unrecognized layer (type) name "<name>"`, a bad sticky word `Unknown sticky option "<word>"`, one offset word without a space `Usage:  setlabel offset <x> <y>`, `text` or `box` with `-default` `Cannot set a default label text.` / `Cannot set a default label box.`. A size that is not a positive number is ignored; a rotation that is not an integer sets 0.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:1660-2160, commands/CmdRS.c:2163-2672, commands/CmdRS.c:2458-2517; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-91)

#### Scenario: Query sizes
- **GIVEN** two selected edit-cell labels of size 80 and 160 eighths, and units `internal`
- **WHEN** `setlabel size` is run in scripting mode
- **THEN** the result SHALL be `10 20`

#### Scenario: Change justification
- **WHEN** `setlabel justify sw` is run with one selected label
- **THEN** its justification SHALL be south-west and the cell SHALL be modified

#### Scenario: Label box in grid units with magic-compat ON
- **GIVEN** `magic-compat` ON, the user grid `(5,5)–(15,15)` and one selected label
- **WHEN** `setlabel box 1g 1g 2g 2g` is run
- **THEN** the label rectangle SHALL be (10,10)–(20,20), with no grid origin added

#### Scenario: Label box in grid units with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the same grid and label
- **WHEN** `setlabel box 1g 1g 2g 2g` is run
- **THEN** the label rectangle SHALL be (15,15)–(25,25)

### Requirement: Port command target

`port [NAME|INDEX] OPTION …` SHALL operate on one label of the edit cell:

- If the first argument is an integer, it is the port with that index. If it is a word that is not an option, it is the label with that text. These are found among edit-cell labels. Not found: "No label found with index N." or "No port found with name NAME.".
- Otherwise, the target is the single label inside the box (a label entirely inside the box area). If there is none, or more than one, the command reports "Exactly one label may be present under the cursor box." The label must be in the edit cell ("Cannot modify a port in an non-edit cell.").

Options (keyword-matched): `class`, `use`, `shape`, `index`, `equivalent`, `exists`, `connections`, `first`, `next`, `last`, `make`, `makeall`, `name`, `remove`, `renumber`, `help`. Options other than `make`, `makeall`, `exists`, `first`, `last` and `renumber` require the label to be a port ("The selected label is not a port.").

Compatibility: behavioral

(Magic ref: commands/CmdLQ.c:1523-1780)

#### Scenario: Ambiguous target
- **GIVEN** two labels inside the box
- **WHEN** `port make` is run
- **THEN** the command SHALL fail with "Exactly one label may be present under the cursor box."

### Requirement: Port attributes

Port attribute keywords SHALL be:

- **class**: `default input output tristate bidirectional inout feedthrough feedthru`. `inout` = `bidirectional` and `feedthru` = `feedthrough`.
- **use**: `default analog signal digital power ground clock`. `digital` = `signal`.
- **shape**: `default abutment ring feedthrough feedthru`.

Querying (no value) SHALL return the keyword (the first name of each synonym group), or print `Class = …`, `Use = …`, `Shape = …`. Setting SHALL apply the value to the target **and to every other port in the edit cell with the same text and the same index**. `index N` sets the index on all ports with the same text and the old index (query prints `Index = N`). `name` returns the text. `exists` returns true/false (also when the label is not a port). An invalid keyword SHALL print the usage with the legal list.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:1540-1625, commands/CmdLQ.c:1830-1980)

#### Scenario: Set class on equivalent ports
- **GIVEN** two port labels `A` with index 3
- **WHEN** `port A class input` is run
- **THEN** both labels SHALL have class input

### Requirement: Port make, numbering and directions

- `port make [INDEX] [DIR…]` SHALL turn the target label into a port. If it already is one, the command reports "The selected label is already a port." Index: INDEX if given. An INDEX already used by a port with different text is an error. Otherwise the labels are scanned in label-list order: the first other port with the same text SHALL give its index and end the scan, so a same-text port always wins; if there is none, the index SHALL be one more than the highest existing port index. The only order dependence is between several same-text ports with different indices, where the first in the label list wins, as in Magic; Hefesto SHALL keep the label order read from the `.mag` file so that this choice matches Magic (see Q-CMDEDIT-92, resolution: replicate). The first port gets index 1.
- **Directions**: each DIR is a Manhattan direction (transformed to edit orientation), ORed into the connection set N/E/S/W. With no DIR, the set is derived from the label justification: N→{N}, S→{S}, E→{E}, W→{W}, NE→{N,E}, NW→{N,W}, SE→{S,E}, SW→{S,W}, center→{N,E,S,W}.
- `port makeall [INDEX] [DIR]` SHALL make every non-port label in the edit cell a port, as `make` does.
- `port connections [DIR…]` SHALL query (`Directions = ` followed by a subset of `nesw`, in that order) or set the direction set.
- `port remove` SHALL turn the port back into a plain label (clearing the index and port attributes).
- `port first` / `port last` SHALL report the lowest and highest port index in the edit cell. `port next [N]` SHALL report the smallest port index greater than the target's (or N's).
- `port equivalent [N]` SHALL give the target the index of port N (or of another port with the same text), making them one electrical port.
- `port renumber` SHALL reassign indices 1, 2, 3, … to the ports in sorted text order, with ports of identical text sharing one index (verify against Magic).

All changes are undoable and mark the cell modified.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:1980-2320, commands/CmdLQ.c:2195-2215; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-92)

#### Scenario: Auto index
- **GIVEN** ports with indices 1 and 4 and a new label `C` under the box
- **WHEN** `port make` is run
- **THEN** `C` SHALL have index 5
- **AND** its directions SHALL follow its justification

#### Scenario: Same-text port wins
- **GIVEN** ports `A` with index 2 and `B` with index 7, and a new label `A` under the box
- **WHEN** `port make` is run
- **THEN** the new port SHALL have index 2

#### Scenario: First same-text port in label order wins
- **GIVEN** a cell read from a `.mag` file whose label list holds port `A` with index 3 before port `A` with index 6, and a new label `A` under the box
- **WHEN** `port make` is run
- **THEN** the new port SHALL have index 3

### Requirement: Property command

`property [list] [TYPE] [NAME [VALUE…]]` SHALL query or set properties of the **window's top (root) cell**, or of the edit cell if there is no window, as Magic does in both `magic-compat` modes; Magic's PCell toolkit calls `property` right after `pushstack`, where the two coincide, and scripts use `cellname property` for an explicit target (see Q-CMDEDIT-93, resolution: replicate). `cellname property CELL …` applies the same command to a named cell. TYPE is one of `string` (default), `integer`, `dimension`, `double`, `plane`, `compat`:

- `property` alone SHALL list all properties (scripting: a flat list of name/value pairs).
- `property NAME` SHALL return the value, or report `Property name "NAME" is not defined` (silent with `list`).
- `property NAME ""` SHALL delete the property.
- `property NAME VALUE…` SHALL set it. String properties keep the text. `integer` parses 32-bit integers. `double` parses 64-bit integers. `dimension` parses each value as a coordinate and stores internal units, reported per `units`. `plane` takes groups of four coordinates as rectangles, stored as a geometry plane.
- Reserved names force their type: `GDS_START`, `GDS_END` (double); `GDS_FILE` (string); `FIXED_BBOX`, `OBS_BBOX` (dimension); `MASKHINTS_*` (plane). Their meaning is defined by the capabilities that use them (MASK, LEF/DEF, extraction).
- `property compat [yes|no]` SHALL query or set the global property compat mode (capability `layout-properties`, requirement "Property compat mode"), in which all properties are stored and written as strings.
- A value that cannot be parsed as an integer reports "Unable to parse integer value" and stores 0.

Setting a property SHALL mark the cell modified.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:2324-2830; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-93)

#### Scenario: Fixed bbox
- **GIVEN** units `internal`
- **WHEN** `property FIXED_BBOX "0 0 100 200"` and then `property FIXED_BBOX` are run in scripting mode
- **THEN** the result SHALL be `0 0 100 200`

#### Scenario: Delete
- **WHEN** `property foo bar`, then `property foo ""`, then `property foo` are run
- **THEN** the last command SHALL report that `foo` is not defined

#### Scenario: Target is the window's root cell
- **GIVEN** a window whose root cell is `top`, and `edit` has made a child cell `sub` the edit cell
- **WHEN** `property foo bar` is run
- **THEN** `top` SHALL have property `foo` = `bar` and `sub` SHALL not

### Requirement: Findlabel command

`findlabel [-glob] NAME [N]` SHALL locate labels in the window's top cell hierarchy:

- Without `-glob`, it finds the N-th (default 0th) label whose hierarchical name equals NAME, including instance-path prefixes such as `x1/x2/A`. It SHALL set the box to that label's rectangle in root coordinates. A zero width or height is widened by one unit on that axis. If not found, it reports `Couldn't find label NAME`.
- With `-glob`, it SHALL list the hierarchical names of all labels matching the glob pattern NAME (scripting: a list). With N, it sets the box to the N-th match instead.

A window and a box in that window are required.

Compatibility: behavioral

(Magic ref: commands/CmdFI.c:801-976)

#### Scenario: Box on point label
- **GIVEN** label `OUT` at (40,10)–(40,10) in the top cell
- **WHEN** `findlabel OUT` is run
- **THEN** the box SHALL be (40,10)–(41,11)

### Requirement: Goto command

`goto NODENAME [-nocomplain]` SHALL set the box to the location of a node and report its type:

- NODENAME MAY carry an instance path (`a/b/name`). Each component before the last `/` is resolved as an instance.
- If the leaf has the form `PLANE_X_Y`, where PLANE is a plane short name and X/Y are integers with an optional leading `n` meaning negative (the extractor's generated node name), the location is the unit square at (X, Y) in that cell. A trailing `#` suffix is allowed.
- The legacy form `P_X_Y` with three integers, where X and Y encode sign in the low bit (value = ±⌊v/2⌋, negative when odd), is also accepted.
- Otherwise the leaf is a label name, located as by `findlabel`.
- The box SHALL be set to the location, transformed to root coordinates. The result (scripting) SHALL be the long name of the type at that location, and it prints `node NAME is type TYPE`.
- With `-nocomplain`, not-found errors are suppressed.

Compatibility: behavioral

(Magic ref: commands/CmdFI.c:1629-1870)

#### Scenario: Generated node name
- **GIVEN** a technology whose metal1 plane short name is `m1`, with metal1 at (−5,7)
- **WHEN** `goto m1_n5_7#` is run
- **THEN** the box SHALL be (−5,7)–(−4,8) and the result SHALL be `metal1`

### Requirement: Element command

`element add|delete|configure|names|inbox|help …` SHALL manage named display elements: lines, rectangles and text attached to a cell for annotation. They are not mask data.

- `element add line NAME STYLE x1 y1 x2 y2`, `element add rectangle NAME STYLE llx lly urx ury`, `element add text NAME STYLE cx cy TEXT`. STYLE is a display style number or long style name. Coordinates are in the window's top cell.
- `element delete NAME`.
- `element configure NAME text [get|replace STRING]`, `style [get|add S|remove S]`, `position [get | x y | x1 y1 x2 y2]`, `flags F`. F is one of `halfx halfy exactx exacty arrowleft arrowbottom arrowright arrowtop plainleft plainbottom plainright plaintop small medium large xlarge default persistent temporary`.
- `element names` SHALL list all element names. `element inbox` SHALL return the name of the element nearest the box.
- Elements are **temporary** by default. `persistent` elements are saved with the cell (see `cell-file-format`) and reloaded with it.

Unknown styles or wrong argument counts SHALL print usage. Elements are not undoable.

Compatibility: behavioral

(Magic ref: commands/CmdE.c:308-600, dbwind/DBWelement.c:178-1290)

#### Scenario: Add rectangle element
- **WHEN** `element add rectangle r1 1 0 0 10 10` and then `element names` are run
- **THEN** the names SHALL include `r1`
