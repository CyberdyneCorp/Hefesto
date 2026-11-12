# Hierarchy and Cell Management Commands

## Purpose

Defines the commands that load, create, place, navigate, expand, flatten, save and manage cells and cell instances: `load`, `xload`, `edit`, `down`, `getcell`/`get`, `dump`, `expand`, `unexpand`, `flatten`, `save`, `writeall`, `flush`, `cellname`/`instance`, `path`, `addpath`, `locking`, `archive` and `crash`. The `.mag` file grammar is in capability `cell-file-format`. Coordinates are in `cmd-coordinates-and-units`, the box in `cmd-box` and the selection in `cmd-selection`. The command surface is `behavioral`. Instance placement transforms, generated instance identifiers and flattened geometry are `exact`.

## Requirements

### Requirement: Cell and instance naming rules

Cell names given to `load`, `xload` and `save` SHALL be rejected if they contain `[`, `]` or `,`, or any character that is not printable. Use identifiers (instance names) SHALL be rejected if they contain `,` or `/`. A cell-name argument MAY include a directory path and a `.mag` suffix, optionally followed by `.gz`. The cell name is the final path component without these suffixes, and the path is remembered as the cell's file location.

When a new instance is created without an identifier, it SHALL be named `<base>_<n>` as specified in capability `layout-hierarchy` requirement "Instance identifier assignment": `<base>` is the cell name after its last `/`, and `<n>` is the smallest integer ≥ 0 such that no existing instance in the parent has that identifier.

Compatibility: exact

(Magic ref: commands/CmdSubrs.c:1343-1375, database/DBcellname.c:2405-2450, dbwind/DBWprocs.c:330-360)

#### Scenario: Generated instance names
- **GIVEN** the edit cell already contains instances `inv_0` and `inv_2` of cell `inv`
- **WHEN** `getcell inv` is run
- **THEN** the new instance SHALL be named `inv_1`

#### Scenario: Illegal cell name
- **WHEN** `load a[1]` is run
- **THEN** the command SHALL fail with an illegal-character error

### Requirement: Load command

`load [NAME] [scaled N [D]] [-nowindow] [-dereference] [-force] [-quiet] [-silent] [-fail]` SHALL make cell NAME the top cell of the current layout window. Trailing `-` options are matched with the keyword-matching rule. An unknown option SHALL report `No such option` and be ignored. Behavior:

1. If NAME is omitted, the window SHALL show the cell `(UNNAMED)`, which is created empty if needed.
2. If a cell named NAME is already in memory, it is used. If NAME carried a path that names a different file than the one the cell was read from, the load SHALL fail with "The cell was already read and points to conflicting location". If the stored path is invalid and the given path exists, the given path is adopted with a warning.
3. Otherwise the cell SHALL be read from the file (see `cell-file-format`, search paths per `path`). `-force` reads even if the technology in the file header differs. `-dereference` ignores file paths stored for subcells and uses the search paths.
4. If the file does not exist, a new empty cell SHALL be created ("Creating new cell"), unless `-fail` is given, in which case the command SHALL fail with `No file "NAME" found or readable.` (printed only when neither `-quiet` nor `-silent` is given: Magic's DBW_LOAD_QUIET). On any other read error the cell is discarded.
5. `-quiet` prints only errors. `-silent` prints nothing.
6. `-nowindow` reads the cell into memory without changing any window.
7. If the window was the edit window, or no window holds the edit cell, the new top cell SHALL become the edit cell. If the cell is read-only, there SHALL be no edit cell. The edit change SHALL be recorded for undo.
8. The view SHALL be set to the cell bounding box grown on each side by `max(0, (60 − size)/2) + (size + 1)/10` (per axis, integer division). For a cell with no bounding box, a (0,0)–(1,1) area is used.
9. The previously displayed `(UNNAMED)` cell SHALL be deleted from memory if it is unmodified and no longer shown, while a display runs. With NULL graphics (a batch or headless session) or a suspended display (`updatedisplay suspend`), it SHALL stay in memory, as in Magic, whose NULL driver leaves the display suspended.

A leading `./` SHALL be dropped from NAME, then a `.gz` and a `.mag` extension of its last component. Magic cuts them off NAME itself, so the path recorded for a missing file (and printed as `File <F> couldn't be read`) has neither: `load /w/inv.mag.gz` records `/w/inv`.

`scaled N [D]` SHALL read the cell (and all of its hierarchy, by expanding everything once) with every coordinate multiplied by N/D, by temporarily changing the internal-unit ratio. Any other extra argument SHALL print the usage. There must be a layout window unless `-nowindow` is given ("Point to a window first.").

Compatibility: behavioral

(Magic ref: commands/CmdLQ.c:391-583, dbwind/DBWprocs.c:286-640)

#### Scenario: Load missing file creates cell
- **GIVEN** no file `newcell.mag` on the search path
- **WHEN** `load newcell` is run
- **THEN** an empty cell `newcell` SHALL be shown and become the edit cell
- **AND** "Creating new cell" SHALL be printed

#### Scenario: Untouched (UNNAMED) deleted while the display runs
- **GIVEN** a window showing an unmodified `(UNNAMED)`
- **WHEN** `load top` is run
- **THEN** with a running display `cellname list allcells` SHALL list `top` only
- **AND** with NULL graphics it SHALL list `(UNNAMED) top`

#### Scenario: Fail option
- **WHEN** `load nosuch -fail` is run with no such file
- **THEN** no cell SHALL be created and the window SHALL be unchanged

### Requirement: Xload command

`xload [NAME]` SHALL behave like `load NAME`, but after loading, every instance in the whole hierarchy below the top cell SHALL be set to unexpanded in that window. Only the top cell is displayed with its contents. More than one argument SHALL print usage.

Compatibility: behavioral

(Magic ref: commands/CmdTZ.c:2373-2400, dbwind/DBWprocs.c:525-535)

#### Scenario: Xload unexpanded
- **GIVEN** cell `top` containing an instance of `sub`
- **WHEN** `xload top` is run
- **THEN** the `sub` instance SHALL be shown unexpanded (bounding box and name only)

### Requirement: Edit command

`edit [INSTPATH]` SHALL change the edit cell to a cell instance without changing the window's top cell:

- With INSTPATH (a hierarchical instance path from the window's top cell, for example `x1/x2`), that instance SHALL become the edit instance. If it is not found, the command SHALL report `Usage: edit` and `Maybe you want the "load" command` and change nothing.
- Without arguments: among the selected cell instances, the first whose area overlaps the pointer area SHALL be chosen. If none overlaps, the last selected instance is used. If no instance is selected, the command SHALL report "No cell selected to edit." and change nothing.
- If the chosen cell is not yet read, it SHALL be read. If it is read-only, the command SHALL report "File FILE is not writeable. Edit not set." and keep the old edit cell.
- The edit-to-root transform SHALL become the instance's transform to the root. The edit change is recorded for undo. Without arguments, the command is logged as `edit USEID`.

With INSTPATH, Magic changes only the edit cell use: the edit-to-root transform and the edit root cell keep their previous values (identity right after `load`), so box coordinates keep being mapped through the previous transform. With `magic-compat` ON (the default), Hefesto SHALL do the same, and whenever a command maps the box (or other root coordinates) into edit coordinates through a transform that no longer matches the edit instance's transform to the root, it SHALL report the warning `Warning: edit transform does not match instance <INSTPATH>; the box is mapped through the previous edit transform`. With it OFF, `edit INSTPATH` SHALL set the instance's full transform to the root and its edit root cell, as for the forms without arguments (see Q-CMDEDIT-30, resolution: replicate in magic-compat, fix in native, with the warning added by user decision).

Compatibility: behavioral

(Magic ref: commands/CmdE.c:82-250; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-30)

#### Scenario: Edit selected instance
- **GIVEN** instance `x1` of `inv` at translation (100,0) is selected
- **WHEN** `edit` is run
- **THEN** `inv` SHALL be the edit cell, with edit-to-root transform = translate(100,0)
- **AND** `paint metal1` with the root box (100,0)–(110,10) SHALL paint (0,0)–(10,10) in `inv`

#### Scenario: Edit by instance path with magic-compat ON
- **GIVEN** `magic-compat` ON, `load top` was just run, and `top` contains instance `x1` of `inv` at translation (100,0)
- **WHEN** `edit x1` is run and then `paint metal1` with the root box (100,0)–(110,10)
- **THEN** `inv` SHALL be the edit cell and metal1 SHALL be painted at (100,0)–(110,10) in `inv`, as in Magic
- **AND** `paint` SHALL report `Warning: edit transform does not match instance x1; the box is mapped through the previous edit transform`

#### Scenario: Edit by instance path with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the same cells
- **WHEN** `edit x1` is run and then `paint metal1` with the root box (100,0)–(110,10)
- **THEN** metal1 SHALL be painted at (0,0)–(10,10) in `inv`
- **AND** no warning SHALL be reported

### Requirement: Down command

`down [INSTPATH]` SHALL choose an instance exactly as `edit` does, then load that instance's cell definition as the top cell of the window (as `load` with technology checking off). It then becomes the edit cell with an identity transform. Without arguments it is logged as `down USEID`. If no new cell is chosen, it SHALL report "You haven't selected a new cell to edit."

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:3898-4000)

#### Scenario: Down into instance
- **GIVEN** a selected instance of `inv`
- **WHEN** `down` is run
- **THEN** the window's top cell SHALL be `inv`

### Requirement: Placement reference syntax for getcell and dump

`getcell` and `dump` SHALL take `CELL [child REF] [parent REF] [ORIENT [REF]]`, where:

- CELL is a cell name, optionally with a path. If CELL is in memory with a different file path, the new cell SHALL be renamed `CELL_a`, `CELL_b`, … (the first unused) with a warning.
- **child REF** is a point in the child's coordinates: `ll`, `lr`, `ul`, `ur` (corners of the child bounding box, or of its `FIXED_BBOX` property if present), `label NAME` (the location of label NAME in the child; error if not found), or `X Y` (relative distances). Default: `ll`.
- **parent REF** is the point where the child reference point lands: `ll`, `lr`, `ul` or `ur` of the box, a label, or `X Y`. Default: the box lower-left. The box SHALL be in a window on the edit root; otherwise the command reports that the box's lower-left must point into the edit cell.
- **ORIENT** is one of `0 90 180 270 v 0v 90v 180v 270v h 0h 90h 180h 270h`, with these transforms: `90` x'=y,y'=−x; `180` x'=−x,y'=−y; `270` x'=−y,y'=x; `v`/`0v`/`180h` y'=−y; `h`/`0h`/`180v` x'=−x; `90v`/`270h` x'=y,y'=x; `270v`/`90h` x'=−y,y'=−x. The orientation is followed by a translation that keeps the child bounding box's lower-left fixed, or that keeps the given REF point fixed.

The final transform child→root SHALL map the (oriented) child reference point onto the parent reference point. The edit cell SHALL exist ("No cell def being edited; cannot place cell use!"). CELL SHALL not be an ancestor of the edit cell ("you're trying to create a circular structure"). CELL is read from disk if needed. If it does not exist, it is created empty.

Compatibility: exact

(Magic ref: commands/CmdCD.c:4893-5420)

#### Scenario: Place with child label at box corner
- **GIVEN** cell `pad` with label `P` at (5,5), and the box lower-left at (100,200) in the edit root (identity edit transform)
- **WHEN** `getcell pad child label P` is run
- **THEN** the new instance SHALL have transform translate(95,195)

#### Scenario: Orientation keeps lower-left
- **GIVEN** cell `c` with bounding box (0,0)–(10,20), and the box lower-left at (0,0)
- **WHEN** `getcell c 90` is run
- **THEN** the instance bounding box SHALL be (0,0)–(20,10)

### Requirement: Getcell command

`getcell …` and its alias `get …` SHALL place a new instance of CELL in the edit cell, with the transform from "Placement reference syntax" composed with root→edit. The instance identifier SHALL be generated. If an identical instance (same cell, same transform, same array) already exists, the command SHALL fail with "Can't place a cell on an exact copy of itself." On success:

- the box SHALL be set to the new instance's bounding box (root coordinates),
- the selection SHALL be replaced by the new instance,
- the edit cell's bounding box SHALL be updated and DRC queued over the instance area,
- in scripting mode the result SHALL be the new instance identifier.

The placement is undoable.

Compatibility: exact

(Magic ref: commands/CmdFI.c:1067-1150)

#### Scenario: Getcell result
- **WHEN** `getcell inv` is run in scripting mode in an edit cell with no instances of `inv`
- **THEN** the result SHALL be `inv_0`
- **AND** the box SHALL equal the instance bounding box

### Requirement: Dump command

`dump …` SHALL copy the contents of CELL into the edit cell (paint, labels and the child's own subcell instances) at the transform from "Placement reference syntax", instead of creating an instance. The copied items SHALL become the selection. Copied instance identifiers SHALL be made unique in the edit cell. The operation is undoable.

Compatibility: exact

(Magic ref: commands/CmdCD.c:4855-4870, select/selOps.c:2183-2208)

#### Scenario: Dump paint
- **GIVEN** cell `blk` containing metal1 (0,0)–(10,10), and the box lower-left at (50,50)
- **WHEN** `dump blk` is run
- **THEN** the edit cell SHALL contain metal1 (50,50)–(60,60) and no instance of `blk`

### Requirement: Expand and unexpand commands

Expansion state is kept per instance per window. `expand [selection|surround|overlap|all] [toggle]` SHALL set instances in the current window to expanded:

- `overlap` (default for `expand`): instances whose area overlaps the box, recursively (newly expanded instances' children overlapping the box are also expanded).
- `surround`: instances whose bounding box is entirely inside the box, recursively.
- `all`: every instance in the hierarchy.
- `selection`: the selected instances.

A last argument that is a prefix of `toggle` SHALL toggle instead of set. `expand toggle` alone toggles under `overlap`. `unexpand [selection|surround|overlap|all]` SHALL unexpand; its default is `surround`. Unknown options print help. Except for `selection`, the box SHALL be in the current window ("The box isn't in the same window as the cursor.").

If reading newly expanded cells changes the internal-unit ratio (a cell saved at a finer scale), the box and windows SHALL be rescaled, `expand: rescaled by D` is printed, and expansion repeats until stable. Expansion changes display only. It is not recorded as a cell modification.

Compatibility: behavioral

(Magic ref: commands/CmdE.c:819-1060, commands/CmdTZ.c:734-870)

#### Scenario: Default expand overlap
- **GIVEN** two unexpanded instances, one overlapping the box and one outside it
- **WHEN** `expand` is run
- **THEN** only the overlapping instance SHALL be expanded

### Requirement: Flatten command

`flatten [OPTIONS] DEST` SHALL create a new cell DEST holding the fully flattened contents of the edit cell (or of the window's top cell if there is no edit cell). Options start with `-no` or `-do`, and the character after that prefix selects the option. Each option word must be longer than 3 characters. Any other option SHALL report `Bad flatten option`.

| Option | Effect |
|---|---|
| `-nolabels` | do not copy labels |
| `-dotoplabels` | copy only the top cell's labels (no hierarchical labels) |
| `-noports` | labels lose port status |
| `-nosubcircuits` | do not flatten instances of cells that are subcircuits (have ports); keep them as instances |
| `-novendor` | do not flatten vendor (GDS-preserved) cells; keep them as instances |
| `-doproperty` | flatten only instances whose cells carry the flatten property; keep the others as instances |
| `-dobox` | flatten only the box area. The box must be in the source cell. DEST may already exist, and is added to. |
| `-doinplace` | flatten in place (below) |

Without `-dobox`, if DEST exists the command SHALL fail with `DEST already exists`. Paint SHALL be copied from every level, transformed to the top coordinates, and merged with the paint rules. Mask hints SHALL be flattened. Labels from subcells SHALL be named with the hierarchical instance path prefixed to the label text (verify the separator against Magic) and keep text, layer, justification and font. Only top-level labels keep port attributes. DEST is created in memory, unsaved and not displayed. Flattening into a new cell is not undoable.

`flatten -doinplace [INSTANCE]` SHALL replace the named instance of the edit cell (or, without a name, every selected instance) with its flattened contents in the edit cell, then delete the instance(s). With the `magic-compat` setting ON (the default), the undo history is not recorded for this, as in Magic. With it OFF, the whole flatten in place is recorded as one undo step (capability `layout-undo`, requirement "Undoable property changes and flatten in place with magic-compat off"; Q-CMDEDIT-31 and Q-MODEL-28, decided by D-10). Flattening into a new cell is not undoable with either setting.

Compatibility: exact

(Magic ref: commands/CmdFI.c:1945-2224)

#### Scenario: Flatten two levels
- **GIVEN** edit cell `top` with instance `x1` of `mid` at translate(100,0), `mid` with instance of `leaf` at translate(10,0), and `leaf` with metal1 (0,0)–(5,5)
- **WHEN** `flatten topflat` is run
- **THEN** `topflat` SHALL contain metal1 (110,0)–(115,5) and no instances

#### Scenario: Destination exists
- **GIVEN** a cell `topflat` exists
- **WHEN** `flatten topflat` is run
- **THEN** the command SHALL fail with `topflat already exists`

#### Scenario: Flatten in place and undo
- **GIVEN** edit cell `top` with instance `x1` of `leaf` at translate(100,0), and `leaf` with metal1 (0,0)–(5,5)
- **WHEN** `flatten -doinplace x1` is run and then `undo` is run
- **THEN** with the `magic-compat` setting ON, `top` SHALL still contain metal1 (100,0)–(105,5) and no instance `x1`
- **AND** with the `magic-compat` setting OFF, `top` SHALL again contain instance `x1` and no metal1 (100,0)–(105,5)

### Requirement: Save command

`save [NAME]` SHALL write the edit cell (or the window's top cell if there is no edit cell, which also clears its read-only flag) in `.mag` format:

- Timestamps are updated before writing.
- With NAME, the cell is written to NAME (adding `.mag`) and then renamed to NAME, unless NAME ends in `.tcl` or a cell named NAME already exists ("there is already a cell named"). If the cell is the edit cell, the window captions are updated.
- Without NAME, the cell is written to its known file. A cell `(UNNAMED)` or a cell with no file requires a name ("Must specify name for cell (UNNAMED)."). In interactive mode the user is asked for one.
- Writing over an existing different file asks `File F already exists.\n  Overwrite it with C? ` (no/yes, default no); only the answer `no` refuses the name. When the question cannot be asked (no prompt host, a headless session, or a script session without `tk_dialog`), the file SHALL be overwritten with `magic-compat` ON, as Magic's TxDialog result -1 does, and the name refused with it OFF (Q-SCRIPT-48).
- When the name is refused (an existing file not to be overwritten, or a cell already named NAME: `Can't rename cell 'C' to 'NAME' because that cell already exists.`), the user SHALL be asked for another file name with `File for cell C: [hit return to abort save] ` and one line of input (capability `scripting-api`, "Prompts and dialogs through the script layer"); the answer is checked like NAME, and an empty answer or the end of input abandons the save with `Cell not saved.` (Magic's cmdCheckNewName). With `magic-compat` ON only the first 7 characters of the answer are the name, as in Magic, which reads it into `sizeof` a pointer; with it OFF the whole line is the name (Q-CELL-47).
- On success the cell's modified flag SHALL be cleared. A write failure SHALL report "Could not write file. Cell not written."

The file writing (including locks and the backup/temporary-file protocol) is defined in capability `cell-file-format`.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:195-235, commands/CmdSubrs.c:706-800)

#### Scenario: Save as new name
- **GIVEN** the edit cell `(UNNAMED)` with paint
- **WHEN** `save adder` is run
- **THEN** `adder.mag` SHALL be written and the cell SHALL be renamed `adder`

#### Scenario: Overwrite question that cannot be asked
- **GIVEN** a `-dnull` script session (no `tk_dialog`), the edit cell `top` with paint and an existing file `other.mag`
- **WHEN** `save other` is run
- **THEN** with `magic-compat` ON `other.mag` SHALL be overwritten and the cell renamed `other`
- **AND** with `magic-compat` OFF `other.mag` SHALL be kept and the file name asked for

#### Scenario: Long file name answer
- **GIVEN** a refused `save other` whose file-name prompt is answered `abcdefghij`
- **THEN** with `magic-compat` ON the cell SHALL be written to `abcdefg.mag`, and with it OFF to `abcdefghij.mag`

### Requirement: Writeall command

`writeall [force|modified [CELL …]]` SHALL write cells:

- With no argument: for every non-internal cell with modified paint, changed bounding boxes, or changed timestamps, the user SHALL be asked `write, autowrite, flush, skip, or abort`. The question names the kind of change (`(bboxes)`, `(timestamps)` or `(bboxes/timestamps)`) when only those changed. `autowrite` writes this and all remaining cells without asking.
- `writeall force`: write every cell without asking, except read-only cells (which get a warning if modified) and an unmodified `(UNNAMED)`.
- `writeall modified`: as `force`, but only cells whose paint/content is modified.
- With CELL names: write exactly those cells. Unknown names report `No such cell "CELL"`. If none is found, nothing happens.

Timestamps of all cells (or of the named cells) SHALL be updated first. Unknown options print usage.

Compatibility: behavioral

(Magic ref: commands/CmdTZ.c:2200-2372)

#### Scenario: Force writes all
- **GIVEN** modified cells `a` and `b`
- **WHEN** `writeall force` is run
- **THEN** both SHALL be written without prompting

### Requirement: Flush command

`flush [CELL] [-noprompt] [-dereference]` SHALL discard the in-memory changes of CELL (default: the edit cell, or the window's top cell) and re-read it from its file:

- If the cell has changes and `-noprompt` is not given, the user SHALL be asked `Really throw away all changes made to cell CELL?` and answering `no` cancels. A question that cannot be asked goes ahead with `magic-compat` ON (Magic's TxDialog -1) and cancels with it OFF (Q-SCRIPT-48).
- A cell whose instance is the parent of the current edit instance SHALL not be flushed ("Cannot flush cell whose subcell is being edited.").
- `-dereference` drops the stored file path and re-reads via the search paths.
- The undo history SHALL be discarded. The cell is cleared, re-read, its bounding box recomputed and its modified flag cleared. DRC is queued in all parents over the instance areas. The selection is cleared.
- The command prints `[Flushed]` or `[Flushed Modifications were Discarded]`.

An unknown CELL does nothing.

Compatibility: behavioral

(Magic ref: commands/CmdFI.c:977-1065, commands/CmdSubrs.c:381-435)

#### Scenario: Flush discards paint
- **GIVEN** cell `a` read from disk, then painted
- **WHEN** `flush a -noprompt` is run
- **THEN** `a` SHALL match its file contents and be unmodified

### Requirement: Cellname and instance commands

`cellname` and `instance` (the latter is any command name containing `in`) SHALL accept `[list] OPTION [NAME] [args] [-list|-def|-force]`. `list` (as the first word) or a trailing `-list` returns a scripting list instead of printing. NAME defaults to the selected cell or instance. Options (keyword-matched):

| Option | `cellname` (cell definitions) | `instance` (instances) |
|---|---|---|
| `children` | child cell names of NAME | child instances of NAME's cell |
| `parents` | cells that instantiate NAME | parent of the instance |
| `exists`, `self` | with `list`: NAME when it is loaded, else `0`; without `list`: prints "Cell NAME is currently loaded." or reports "Cell NAME is not currently loaded." with no result; without NAME, the cells with an instance in the selection (their names, or one "Cell C is currently loaded." line each), else the edit root's | the instance name |
| `instances` | instance names of cell NAME; an arrayed instance is named with its first indices, backslash-escaped (`x2\[0\]`, `x3\[0,1\]`) | the instance itself |
| `childinst` | child instance names (an arrayed instance with its first indices, as for `instances`); with `magic-compat` ON in Magic's instance-identifier hash order (a string hash table of 16 buckets whose entries stay when an instance is deleted or renamed, so the order depends on the placement, rename and delete history), with it OFF in placement order (Q-CELL-46) | child instances, in the same order |
| `celldef` | the cell name | cell name of the instance |
| `allcells` | all cell names (internal cells excluded) | — |
| `topcells` | cells with no parents | error "Instances do not have a top level" |
| `window` | top cell of the current window | unimplemented |
| `create NAME` | create an empty cell if absent | error (use `getcell`) |
| `delete NAME [-noprompt]` | delete the cell from memory (asks if modified, unless `-noprompt`) | error (use `delete`) |
| `dereference NAME` | drop stored file path and reload via search paths | unimplemented |
| `filepath NAME [PATH]` | report or set the file directory; with `list` and `magic-compat` ON the whole stored file path, as in Magic (Q-CELL-45) | unimplemented |
| `flags NAME` | report cell flags (available, modified, read-only, …) | unimplemented |
| `timestamp NAME [VALUE]` | report or set the integer timestamp. A non-integer is "Invalid timestamp". | unimplemented |
| `lock`, `unlock` | error "Cell definitions cannot be locked" | lock/unlock the instance (locked instances cannot be moved, copied or deleted) |
| `property NAME …` | the `property` command applied to NAME (capability `cmd-labels-properties`) | unimplemented |
| `abutment` | error (use `property get FIXED_BBOX`) | abutment box of the instance |
| `orientation [-def]` | error | orientation as `0 90 180 270 0h 90h 180h 270h`, or with `-def` as `N E S W FN FE FS FW`. Setting by name is refused ("Use selection"). |
| `position` | error | instance origin in its parent |
| `rename NAME NEW [-force]` | rename cell NAME (or the edit root with one argument) | error (use `identify`) |
| `writeable NAME [yes/no]` | report or set read-write state (yes values `yes true on 1`, no values `no false off 0`). Setting writeable fails if an advisory lock is held by another process. | unimplemented |
| `modified` | names of modified cells | unimplemented |

More than 4 arguments, or an unknown option, SHALL print usage. The output order of list results (verify against Magic) follows the in-memory cell table order.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:1125-1700, database/DBcellname.c:754-1200, database/DBcellname.c:1470-1535)

#### Scenario: File path of a cell
- **GIVEN** cell `inv` whose file path was set with `cellname filepath inv /lib`
- **WHEN** `cellname filepath inv` and `cellname list filepath inv` are run
- **THEN** both SHALL give `/lib`, except that with `magic-compat` ON the list form SHALL give the whole stored path `/lib/inv`, as Magic does (Q-CELL-45, resolution: compat)

#### Scenario: Orientation names
- **GIVEN** a selected instance placed with transform x'=−x, y'=−y
- **WHEN** `instance orientation -def` is run in scripting mode
- **THEN** the result SHALL include `S`
- **AND** without `-def` it SHALL be `180`

#### Scenario: Exists
- **WHEN** `cellname list exists nosuch` is run in scripting mode
- **THEN** the result SHALL be `0`
- **AND** `cellname exists nosuch` SHALL report "Cell nosuch is not currently loaded." and return no result

### Requirement: Search path commands

`path [search|cell|sys|help] [[+]PATH]` SHALL query or set the cell search path, the cell library path and the system path:

- `path` alone prints all three paths, one line each ("Search path for cells is \"…\"", "Cell library search path is \"…\"", "System search path is \"…\""), and returns no result in every mode.
- `path KIND` returns (scripting) or prints that path.
- `path KIND PATH` replaces it. `path KIND +PATH` appends PATH to it (colon or space separated).
- `path PATH` (a word that is not a keyword) sets the search path.
- An ambiguous keyword reports `Ambiguous path option`. More than two arguments print usage.

`addpath PATH` SHALL append PATH to the cell search path. It needs exactly one argument.

Search paths are directory lists searched in order when reading a cell by name. On sandboxed platforms they refer to locations the user has granted access to (see Q-CMDEDIT-32).

Compatibility: behavioral

(Magic ref: commands/CmdLQ.c:1090-1195, commands/CmdAB.c:73-90)

#### Scenario: Append path
- **GIVEN** the search path is `.`
- **WHEN** `path search +lib` is run
- **THEN** the search path SHALL be `. lib` (verify the separator against Magic)

### Requirement: File locking command

`locking [enable|disable]` SHALL report or set whether cell files are advisory-locked while open for editing. Accepted values: `enable yes true on 1` and `disable no false off 0`. With no argument it SHALL return/print `enabled` or `disabled`. When locking is enabled, a cell whose file is locked by another process SHALL be opened read-only. When locking is unavailable on the platform, enabling SHALL report that it is not available.

Compatibility: behavioral

(Magic ref: commands/CmdLQ.c:599-672)

#### Scenario: Query locking
- **WHEN** `locking disable` and then `locking` are run in scripting mode
- **THEN** the result SHALL be `disabled`

### Requirement: Locking mechanism

Hefesto SHALL treat Magic's locking mechanism (POSIX advisory record locks on the open `.mag` file, held while the cell is loaded) as reference material. The mechanism, the session-locking semantics and the platform alternative are owned by capability `cell-io-safety` requirements "Locking mechanism" and "Advisory locking between sessions" (see Q-CELL-26 and Q-CMDEDIT-33).

Compatibility: informative

(Magic ref: commands/CmdLQ.c:620-660, utils/flock.c)

#### Scenario: Locked by another process
- **GIVEN** another Magic process holds the lock on `a.mag`
- **WHEN** `load a` is run with locking enabled
- **THEN** cell `a` SHALL be read-only

### Requirement: Archive command

`archive write|writeall|read|readref FILE` SHALL write or read a single-file archive of cells, with the four modes, the set of cells written, the file-name rules and the archive format specified in capability `cell-io-safety` requirements "Archive files" and "Crash backup file format":

- `write` writes every available cell, modified or not, except internal, not-found and read-only cells.
- `writeall` also writes read-only (for example PDK) cells.
- `read` loads the archive with every cell dereferenced.
- `readref` loads the archive without dereferencing, so recorded file paths are honored.

Reading appends `.mag` to the given name (see Q-CELL-34). The command SHALL require exactly two arguments. On a wrong count it SHALL print the usage and stop, changing nothing, whatever the `magic-compat` setting. Magic prints the usage and continues, but its option is still unset so it does nothing more; stopping is observably identical (see Q-CMDEDIT-34, resolution: fix).

Compatibility: behavioral

(Magic ref: commands/CmdAB.c:84-147, database/DBio.c:6382-6560; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-34)

#### Scenario: Archive round trip
- **GIVEN** cells `top` and `sub` in memory
- **WHEN** `archive writeall arch.mag`, then a fresh session runs `archive read arch` (which opens `arch.mag`)
- **THEN** both cells SHALL be in memory with identical contents

### Requirement: Crash backup command

`crash save [FILE]` SHALL write a crash backup of the cells selected by capability `cell-io-safety`, requirement "Crash backup policy" (modified, available, editable, not internal and not flagged not-found), to FILE or, without FILE, to the session backup file. `crash recover [FILE]` SHALL recover from FILE or, without FILE, from the candidate with the newest status-change time, as specified by capability `cell-io-safety`, requirement "Crash recovery", which governs (confirmation, refusal while the session has a backup file, deletion after reading). The sub-command is keyword-matched. An unknown sub-command, or more than two arguments, prints `Usage: crash save|recover [filename]`; `crash` alone does nothing. The periodic automatic backup is the autosave service of capability `hefesto-files-and-pdks`. The session backup file lives in Hefesto's app-private crash backup directory, and the default candidates of `crash recover` include, on the desktop, Magic's `$TMPDIR/MAG*` backups (capability `cell-io-safety`, requirements "Crash backup location and session lock" and "Import of Magic crash backups"; Q-CELL-30, resolved by decision D-8).

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:3751-3780, database/DBio.c:1113-1215)

#### Scenario: Recover
- **GIVEN** `crash save bk` was run with modified cell `a`
- **WHEN** a new session runs `crash recover bk`
- **THEN** cell `a` SHALL be restored with the modifications
