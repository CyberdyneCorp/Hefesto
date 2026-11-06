# Extract Commands

## Purpose

Defines the `extract` command family: which cells are extracted and when (incremental, all, single cell, parents), extraction options (`do`/`no`), their defaults (including the start-up value of `fringehalo`; the tech-format rule that resets it on technology load is owned by `extract-tech-section`), warnings, styles, side halo and step size overrides, output path and `.ext` file location, permanent and temporary unique naming, path-length (`length`) extraction, and cell properties that change extraction. The result of extraction is specified by the other `extract-*` capabilities. On hosts without a command line (iPadOS), the same operations SHALL be offered through the application's interaction surface; the command syntax below is the reference for scripting.

## Requirements

### Requirement: Command syntax and option matching

The command SHALL be `extract [list|listall] [OPTION [ARGS...]]` with OPTION one of `all, cell, do, halo, help, length, no, parents, path, showparents, stepsize, style, unique, warn`, matched case-insensitively by unique prefix (an exact match wins). An unknown or ambiguous option SHALL report `"<opt>" isn't a valid extract option.  Type "extract help" for a list of valid options.` and do nothing. A first argument beginning with `list` (or `listall`) SHALL select list-style results for `path` and `style` and be removed before matching. All options except `style` and `help` SHALL require a current layout window or box-tool cell to determine the root cell (`Point to a window first` otherwise). Wrong argument counts SHALL report `Wrong number of arguments in "extract" command.  Try "extract help" for help.`
Compatibility: behavioral (operations); informative (message text)

(Magic ref: commands/CmdE.c:1064-1220, utils/lookup.c:58-160)

#### Scenario: Ambiguous prefix
- **GIVEN** the command `extract s`
- **WHEN** it is executed
- **THEN** it reports that `s` isn't a valid extract option (it matches `showparents`, `stepsize`, `style`)

### Requirement: Incremental extraction

`extract` with no option SHALL extract the root cell's tree incrementally: the root cell MUST be named (`Please name the cell before extracting.` otherwise); all cell timestamps SHALL first be brought up to date; then each cell in bottom-up order (see `extract-hierarchy`) SHALL be re-extracted only if its `.ext` file cannot be opened for reading, its first line is not `timestamp <N>`, or N differs from the cell's timestamp. Cells not re-extracted SHALL still have their isolated substrate regions prepared (see `extract-substrate`) so that parents are extracted correctly.
Compatibility: behavioral

(Magic ref: extract/ExtMain.c:879-920, extract/ExtMain.c:931-960, extract/ExtCell.c:89-95)

#### Scenario: Unchanged child skipped
- **GIVEN** a parent and a child, both previously extracted, and an edit only in the parent
- **WHEN** `extract` runs
- **THEN** only the parent's `.ext` file is rewritten

### Requirement: Extract all, cell, parents and showparents

`extract all` SHALL extract every cell of the root's tree regardless of timestamps (root must be named). `extract cell NAME` SHALL extract only the currently selected cell into the file `NAME.ext` (children are not extracted but their substrates are prepared; path-length extraction is not done), followed by full R-C extraction of that cell when option `extresist` is on; with no selection it SHALL report `No cell selected`. `extract parents` SHALL extract the selected cell and every cell that uses it directly or indirectly (selected cell first, then parents depth-first). `extract showparents` SHALL print, without extracting, the names of the cells `parents` would extract, comma-separated.
Compatibility: behavioral

(Magic ref: commands/CmdE.c:1220-1310, extract/ExtMain.c:250-460, extract/ExtMain.c:620-700)

#### Scenario: Extract a single cell into a named file
- **GIVEN** a selected instance of cell `inv`
- **WHEN** `extract cell myinv` runs
- **THEN** the file `myinv.ext` is written with the content of `inv`'s extraction and no child `.ext` files are written

### Requirement: Cell properties affecting extraction

A cell with property `noextract` set to true SHALL NOT be extracted (only its substrate is prepared). A cell with property `LEFview` true SHALL be written with an `abstract` record, without a designated substrate node, without duplicate-label warnings and without unique renaming. A cell with property `parameter` SHALL get a `parameters :<cell> <value>` header record. A cell with property `device` SHALL be written as a single device (see `extract-devices`).
Compatibility: exact

(Magic ref: extract/ExtCell.c:84-88, extract/ExtBasic.c:295-310, extract/ExtCell.c:594-606, extract/ExtUnique.c:115-116)

#### Scenario: Abstract cell
- **GIVEN** a cell with `LEFview` true containing two unconnected pins labeled `A`
- **WHEN** it is extracted
- **THEN** the file contains `abstract`, both nodes are written, and no duplicate-label warning is issued

### Requirement: Extraction options

`extract do OPT` / `extract no OPT` SHALL enable / disable an option, OPT matched by unique prefix among: `adjust`, `all`, `capacitance`, `corners [k]`, `coupling`, `extresist`, `length`, `local`, `lumped`, `labelcheck`, `aliases`, `unique [notopports]`, `resistance`, `fringehalo`. With no OPT the current settings SHALL be listed. `all` SHALL denote exactly {adjust, capacitance, coupling, lumped, length, unique}; it SHALL NOT include fringehalo, corners, labelcheck, aliases, extresist or the notopports modifier. `resistance` SHALL be a synonym of `extresist` (full R-C extraction, capability `extresist-command`); `lumped` selects lumped resistance. `corners K` SHALL also set the corner factor to K (parsed as a double; non-numeric gives `Usage:  extract do corners [value]` and no change). `unique notop...` (first 5 characters `notop`) SHALL set both `unique` and `notopports`; `extract no unique notopports` SHALL clear both. `local` SHALL be a synonym for `extract path .` (do) / `extract path none` (no).
Compatibility: behavioral

See Q-EXTRACT-14.

(Magic ref: commands/CmdE.c:1395-1490, extract/extract.h:70-96)

#### Scenario: Fringehalo independent of all
- **GIVEN** a format-35 technology is loaded
- **WHEN** the user runs `extract no all` then `extract do capacitance`
- **THEN** `fringehalo` is still on and `coupling` is off

#### Scenario: Corner factor
- **GIVEN** the command `extract do corners 0.8`
- **WHEN** it is executed
- **THEN** corner extraction is on with factor 0.8

### Requirement: Option defaults

At program start the options SHALL be: adjust, capacitance, coupling, lumped, length, unique, fringehalo, labelcheck and aliases on; corners (factor 1.0), extresist and notopports off; warnings `dup` and `fets` on and `labels` off; no extraction path. Each time a technology is loaded, `fringehalo` SHALL be set from the tech format (on iff format ≥ 34, see `extract-tech-section`); changing the extract style SHALL NOT change any option.
Compatibility: exact

(Magic ref: extract/ExtMain.c:56-70, extract/ExtTech.c:3574-3587)

#### Scenario: Old tech file disables fringe halo
- **GIVEN** a tech file with `format 33`
- **WHEN** it is loaded and `extract do` is run without arguments
- **THEN** the listing shows `no fringehalo`

### Requirement: Warnings

`extract warn [no] OPT` with OPT among `all, dup, fets, labels` SHALL enable/disable the corresponding warning class (`all` = all three); with no argument the enabled classes SHALL be listed. `dup` controls duplicate-label feedback, `fets` malformed-device feedback, `labels` feedback when a node name had to be found the hard way.
Compatibility: behavioral

(Magic ref: commands/CmdE.c:1360-1394)

#### Scenario: Disable device warnings
- **GIVEN** `extract warn no fets`
- **WHEN** a cell with a transistor missing a terminal is extracted
- **THEN** no feedback is created for it, but its device record is still written

### Requirement: Style, halo and step size

`extract style` SHALL print the current style (with `listall`, all styles); `extract style NAME` SHALL select the unique style whose name has NAME as prefix (`Extraction style "<n>" is ambiguous.` / `"<n>" is not one of the extraction styles Magic knows.` otherwise) and reload it (see `extract-tech-section`). `extract halo [D]` SHALL print the side halo in internal units or set it to D (any distance syntax of `cmd-coordinates-and-units`, must be ≥ 0); `extract stepsize [D]` likewise for the step size (must be > 0). These overrides SHALL apply to the loaded style until it is reloaded.
Compatibility: behavioral

See Q-EXTRACT-25.

(Magic ref: commands/CmdE.c:1240-1300, extract/ExtTech.c:770-806)

#### Scenario: Halo override
- **GIVEN** a style with `sidehalo 4` on a 1:1 grid
- **WHEN** `extract halo 8` is run and a cell extracted
- **THEN** sidewall coupling is searched up to 8 units and the hierarchical halo is 9

### Requirement: Output path and file location

`extract path DIR` SHALL set the output directory; `none`, `(none)` or `null` (case-insensitive) SHALL clear it; with no argument the path SHALL be printed (`(none)` when unset). The `.ext` file of a cell SHALL be located as follows: with path `.`, `<cellname>.ext` in the current directory; with another path, `<path>/<cellname>.ext`, creating the directory (mode 0755) when missing and falling back to the current directory when creation fails (`Path "<p>" does not exist;  saving locally.`); with no path, next to the cell's `.mag` file (its file name with the extension replaced by `.ext`), or `<cellname>.ext` for a cell never saved. If that file cannot be opened for writing, the writer SHALL retry `<basename>.ext` in the current directory unless a `<basename>.mag` exists there, and otherwise report `Cannot open output file` and skip the cell.
Compatibility: behavioral; see Q-EXTRACT-11 [platform] for sandboxed hosts

(Magic ref: extract/ExtCell.c:150-245, extract/ExtTech.c:705-730)

#### Scenario: Path directory created
- **GIVEN** `extract path ext/` and no `ext/` directory
- **WHEN** a cell `top` is extracted
- **THEN** the directory `ext/` is created and `ext/top.ext` is written

### Requirement: Unique command and temporary unique option

`extract unique [OPT]` with OPT among `all` (default), `#`, `noports`, `notopports` SHALL permanently rename duplicate labels of every cell in the tree, bottom-up (see `extract-nodes-and-labels` for the rules; `notopports` applies `all` to every cell except the root); if option `unique` was on it SHALL be turned off with `Warning:  Extract option "do unique" disabled because "extract unique" was run.` When option `unique` is on, every extracted cell SHALL be renamed temporarily at the start of its extraction (with `notopports`, the root cell keeps duplicated port names) and the renaming SHALL be reverted after the extraction run.
Compatibility: behavioral (renaming rules are exact, see `extract-nodes-and-labels`)

(Magic ref: extract/ExtMain.c:370-430, extract/ExtCell.c:500-517, extract/ExtMain.c:744-760)

#### Scenario: Permanent unique
- **GIVEN** two unconnected `net1` labels and no ports
- **WHEN** `extract unique` runs
- **THEN** one label is permanently renamed `net1_uq0` and `do unique` is off afterwards

### Requirement: Path length extraction

`extract length driver NAME...` and `extract length receiver NAME...` SHALL add hierarchical terminal names (relative to the root) to the driver and receiver tables; `extract length clear` SHALL empty both. When option `length` is on and the root cell is extracted, for every driver whose label is found, the net through the driver SHALL be flattened, every receiver label on that net found, and for each driver label and receiver label a record `distance <driver> <receiver> <min> <max>` SHALL be written, where min and max are the shortest and longest path lengths (internal units) from driver to receiver accumulated by flooding the net's tiles, each tile crossing costing the Manhattan distance between the midpoints of its entry and exit boundaries (plus the tile's width or height when entry and exit lie on the same side).
Compatibility: behavioral (see Q-EXTRACT-12)

(Magic ref: extract/ExtLength.c:147-310, extract/ExtLength.c:968-1040, extract/ExtCell.c:542-544)

#### Scenario: Straight wire
- **GIVEN** a single metal1 wire (0,0)-(100,4) with driver label `d` at the left end and receiver label `r` at the right end, `extract length driver d`, `extract length receiver r`
- **WHEN** the root cell is extracted with option `length` on
- **THEN** a record `distance d r <min> <max>` with min ≈ max ≈ 100 is written (verify against Magic)

### Requirement: Progress and summary messages

For each extracted cell the system SHALL report `Extracting <cell> into <file>:` and, when there were errors or warnings, `<cell>: N error(s) M warning(s)`; at the end of a run `Total of N error(s) (check feedback entries).` and `Total of M warning(s).` Long extractions MAY report `Completed N%` progress. Errors and warnings SHALL also be recorded as feedback areas on the layout.
Compatibility: informative

(Magic ref: extract/ExtCell.c:96-125, extract/ExtMain.c:760-775, extract/ExtSubtree.c:262-282)

#### Scenario: Warning summary
- **GIVEN** a cell with one duplicate label pair
- **WHEN** `extract all` runs
- **THEN** the summary reports `Total of 2 warnings.`
