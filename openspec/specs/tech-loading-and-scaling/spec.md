# Technology Loading and Scaling

## Purpose

Defines how a technology file is found, loaded, partly reloaded and selected at startup, and what a load does to layout already in memory. It also defines the internal-grid scale model (internal units per lambda) and how technology tables are rescaled to match it at load time and through the `scalegrid` command. The file format is in capability `tech-file-format`. The `tech` command surface is in capability `tech-command`. How each section's numeric values are scaled is defined by the section owners: the `mask-*`, `drc-*`, `extract-*` and `lef-*` capabilities, and the wiring and router capabilities.

## Requirements

### Requirement: File search rules

To open a named file with an optional suffix, the system SHALL use these rules:

Compatibility: behavioral

1. If a suffix is given, it is always appended to the name, even if the name already ends with it.
2. A name starting with `~` or `$` is expanded (home directory or environment variable) and opened directly. No search is done.
3. A name starting with `/` is opened directly. If that fails, the open fails.
4. A name equal to `.` or `..`, or starting with `./` or `../`, is tried directly. If that fails, the search continues with step 5.
5. Otherwise, try each directory of the search path `.` in turn, then each directory of the system library path.
   - The search stops at the first file that opens.
   - It also stops, with failure, at the first error other than "file not found".

The default system library path is `. $CAD_ROOT/magic/sys $CAD_ROOT/magic/sys/current`. This path, `$CAD_ROOT`, `~` and `$VAR` expansion are host mechanisms and are `informative`. Hefesto SHALL provide an equivalent ordered list of technology search locations, which on iPadOS are in the app sandbox (see Q-TECH-28).

(Magic ref: utils/path.c:647-800, utils/paths.h:40, utils/main.c:583)

#### Scenario: Relative name found in the working directory
- **GIVEN** a file `./tiny.tech` in the working directory
- **WHEN** the technology `tiny` is opened with suffix `.tech`
- **THEN** `./tiny.tech` is opened before any system-path copy

### Requirement: Technology file name resolution

Given a technology argument X, the loader SHALL locate the file as follows:

Compatibility: exact

1. **Split.** Split X into a directory part and a base name. Let D be the last `.` in the base name.
2. **`.tech` suffix.** If the base name ends in `.tech` (D followed by exactly `tech`), remove that suffix from X.
3. **Other suffix.** Otherwise, if the base name contains a `.` (D exists), first try to open X verbatim with no suffix. Example: `SCN4M_SUBM.20`.
4. **`.tech`.** If nothing is open yet, open X + `.tech`.
5. **`.tech27`.** If that fails, open X + `.tech27`. The suffix `.tech27` then becomes the include suffix for this load (see capability `tech-file-format`).
6. **Failure.** If all fail, report `Could not find file '<X>.tech' in any of these directories:`, then the system path on the next line, indented by 9 spaces. The load fails.

The resolved full path becomes the current technology file name, which `tech filename` reports. A reload of the current file, as used by a partial reload, reopens that exact path without adding a suffix.

(Magic ref: utils/tech.c:461-540)

#### Scenario: Name with dot
- **GIVEN** a file `SCN4M_SUBM.20.tech` and no file `SCN4M_SUBM.20`
- **WHEN** `tech load SCN4M_SUBM.20` is executed
- **THEN** `SCN4M_SUBM.20.tech` is loaded

#### Scenario: Explicit suffix
- **GIVEN** a file `tiny.tech`
- **WHEN** `tech load tiny.tech` is executed
- **THEN** `tiny.tech` is loaded, and not `tiny.tech.tech`

#### Scenario: Legacy suffix
- **GIVEN** only a file `old.tech27`
- **WHEN** `tech load old` is executed
- **THEN** `old.tech27` is loaded

### Requirement: Full technology load

A full load SHALL replace the current technology with the one in the named file, in this order:

Compatibility: behavioral

1. Resolve the file (see "Technology file name resolution").
2. Mark every section unread.
3. Discard all stored style variants of the cifoutput, cifinput, extract, drc and mzrouter sections. These sections keep lists of named styles, and their owners re-create them while reading.
4. Remember the current plane count K₀.
5. Read the whole file as specified in capability `tech-file-format`. Each section's init procedure resets that section's tables when its header is read.
6. Decide success or failure (see capability `tech-file-format`, "Error reporting and recovery").
7. On success, apply the post-load steps (see "Post-load adjustments").
8. On failure:
   - Restore the plane count to K₀.
   - Clear the current technology file name.
   - Keep every other table in its partly loaded state. Magic does not roll back (see Q-TECH-10).

Hefesto SHALL make a failed full load leave the previously loaded technology fully in effect: an atomic load. The failure SHALL still be reported with the same diagnostics. This is a deliberate deviation recorded in Q-TECH-10.

A full load never edits the tile types stored in existing cells (see "Effect on existing layout").

(Magic ref: utils/tech.c:425-870)

#### Scenario: Load failure is reported
- **GIVEN** a technology file missing its `connect` section
- **WHEN** it is loaded
- **THEN** `Section "connect" was missing from <file>.` is reported, and the `tech load` command's result is `Error in loading technology file\n`

### Requirement: Readability pre-check

Before an interactive or startup full load (`tech load`, `-T`, or the startup default), the system SHALL check the file. The first logical line, after comments and blank lines are skipped and with no directive processing, must be exactly the single token `tech`. If the file cannot be found or opened, or its first logical line is anything else, the load is not attempted. No table is touched. `tech load` sets its result to `Technology file does not exist or is not readable\n` (Magic's Tcl text: no period, trailing LF; see capability `tech-command`, "tech load"), and startup reports `Failed to load technology "<name>"`.

Compatibility: behavioral

So a technology file loaded this way must begin with its `tech` section. Automatic loads triggered by reading a cell skip this check (see "Automatic load from a cell's technology").

(Magic ref: utils/tech.c:548-559, commands/CmdTZ.c:576-591, utils/main.c:872-884, utils/main.c:1145-1151)

#### Scenario: File starting with the version section
- **GIVEN** a technology file whose first section is `version`
- **WHEN** `tech load` names it
- **THEN** the result is `Technology file does not exist or is not readable\n` and the current technology is unchanged

### Requirement: Post-load adjustments

After a successful full load, the system SHALL apply these steps in order:

Compatibility: exact

1. **Minimum grid.** If the current CIF output style has the option `set-minimum-grid` (see the `mask-*` capabilities), set the internal scale to 1 lambda = ⌊scaleFactor / gridLimit⌋ internal units. That is, L = 1 and I = ⌊scaleFactor / gridLimit⌋ (see "Internal grid scale"). scaleFactor and gridLimit are the output style's values in the units defined by the mask capabilities. A grid limit of 0 makes Magic divide by zero (see Q-TECH-27).
2. **Rescale tables.** If the internal scale is not 1:1 (L ≠ 1 or I ≠ 1), rescale every scale-dependent technology table from lambda to internal units by the factor I/L. These tables are cifinput, cifoutput, drc, extract, wiring, lef and router. Each owner capability defines the rounding.
   - LEF defaults are recomputed after the LEF table is scaled.
   - Then print `Scaled tech values by <I> / <L> to match internal grid scaling`.
   - Then check the grid limit with factor 1/1 (see "Grid limit check"). If it fails, print `WARNING:  Current grid scale is smaller than the minimum for the process!`.
3. **Router and plow hooks.** Recompute the parameters derived from the technology for the maze router, the interactive router, the gate-array router and plowing. These are owned by those capabilities.
4. **Layout check.** Check existing layout and adjust planes (see "Effect on existing layout").

Partial reloads (see "Partial section reload") do not run these steps. Each style-change command scales its own section.

(Magic ref: utils/tech.c:804-860, cif/CIFtech.c:1052, cif/CIFtech.c:466-489)

#### Scenario: Load at a doubled internal grid
- **GIVEN** an internal scale of 1 lambda = 2 internal units, set earlier by `scalegrid 1 2`
- **WHEN** a technology whose drc width rule for metal1 is 3 (lambda) is loaded
- **THEN** `Scaled tech values by 2 / 1 to match internal grid scaling` is printed
- **AND** the metal1 width rule is held as 6 internal units

### Requirement: Effect on existing layout

A technology load SHALL NOT translate, re-type or delete tiles in existing cells. Cells keep their stored type numbers, which are interpreted under the new technology. After a successful full load, the system SHALL check whether any non-internal cell has paint, and adjust planes:

Compatibility: behavioral

1. **Paint check.** A cell "has paint" if any tile of type 6 or above lies on any of planes 3..K₀−1 of that cell, where K₀ is the plane count before the load.
2. **Warnings.** If any cell has paint, print `Warning:  Number of planes has changed.  ` if the plane count K changed, and in every case `Existing layout may be invalid.`.
3. **Plane count change.** If K ≠ K₀, every cell in memory gets its planes adjusted:
   - If K > K₀, empty planes K₀..K−1 are added.
   - If K < K₀, planes K..K₀−1 are deleted with all their tiles.

Hefesto SHALL do the same with `magic-compat` ON. With `magic-compat` OFF, Hefesto SHALL instead remap existing layout by long type name. See Q-TECH-31.

(Magic ref: utils/tech.c:281-310, utils/tech.c:852-859, commands/CmdTZ.c:85-103)

#### Scenario: Fewer planes after reload
- **GIVEN** a cell with metal2 paint on plane 9, and a new technology with only 9 planes (0..8)
- **WHEN** the new technology loads successfully
- **THEN** `Warning:  Number of planes has changed.  Existing layout may be invalid.` is reported
- **AND** plane 9 and its paint are removed from the cell

### Requirement: Partial section reload

The system SHALL support re-reading selected sections of the current technology file, leaving every other section in effect. Commands that switch a section's style use this: CIF output or input style, extract style, drc style, plot and router parameters. The reload works as follows:

Compatibility: behavioral

1. **Reopen.** The current technology file is reopened by its stored full path. If there is none, report `Invalid technology file load.` and fail. If it cannot be opened, report `Could not find file '<path>' in any of these directories:` followed by the path, and fail.
2. **Exclude other sections.** Every section except the selected ones is "excluded". Its header and body are skipped silently. Its prerequisites count as met, and it is not checked for presence.
3. **Init and final.** Section init and final procedures run only for the selected sections.
4. **No reset, no post-load steps.** The global style lists are not reset, and the post-load adjustments are not applied. The selecting command does its own scaling and finalization.
5. **Missing section.** A selected section that is missing from the file is reported as missing, and the reload fails.

The file is read from disk again, so edits made to it since the full load take effect for the selected sections.

When a partial reload fails, Magic restores the plane count from an uninitialized value and clears the stored file name (see Q-TECH-10). Hefesto SHALL leave both unchanged.

(Magic ref: utils/tech.c:159-182, utils/tech.c:466-484, utils/tech.c:691-695, utils/tech.c:773-784, cif/CIFtech.c:2288-2289, extract/ExtTech.c:1096-1105, drc/DRCtech.c:529-530)

#### Scenario: Extract style change
- **GIVEN** a loaded technology with two extract styles
- **WHEN** the extraction style is changed
- **THEN** only the `extract` section is re-read from the technology file
- **AND** the planes, types and paint tables are unchanged

### Requirement: Startup technology selection

At startup, the system SHALL select the technology in this order:

Compatibility: behavioral

1. **Bootstrap.** Load the built-in technology `minimum`. If this fails, report `Cannot load technology "minimum" for initialization` and stop with an error. Then clear the stored technology file name, so that the bootstrap does not count as a user choice.
2. **Command line.** If a technology was named on the command line (option `-T <name>`), load it after any system pre-startup script, using the readability pre-check.
   - While startup scripts run, `tech load` commands are ignored silently, so the command-line choice wins.
   - If this load fails, the override is dropped and later startup scripts may load a technology.
3. **Startup scripts.** Run the startup scripts. They may load a technology with `tech load`.
4. **Cell argument.** If no technology has been loaded yet, no command-line technology was given, and a layout file was named on the command line, use the technology named in that file's `tech` header (see capability `cell-file-format`).
5. **Default.** If a default name was chosen by step 2 or step 4 and no technology is loaded, load it with the pre-check.
6. **Fallback.** If still no technology is loaded, load `minimum` again and keep it.

Script names and the pre-startup mechanism are `informative`. On iPadOS, Hefesto SHALL use an app-level setting or project file instead of command-line options (see Q-TECH-28).

(Magic ref: utils/main.c:303-307, utils/main.c:719-736, utils/main.c:868-884, utils/main.c:1130-1166, commands/CmdTZ.c:570)

#### Scenario: Command line wins over the startup script
- **GIVEN** magic started with `-T sky130A`, and a `.magicrc` that runs `tech load gf180mcuD`
- **WHEN** startup completes
- **THEN** the current technology is sky130A

#### Scenario: Technology taken from the layout argument
- **GIVEN** no `-T` option, no startup script loading a technology, and the argument `inv.mag`, whose header line is `tech tiny`
- **WHEN** startup completes
- **THEN** the technology `tiny` is loaded

### Requirement: Automatic load from a cell's technology

When reading a layout cell triggers an automatic technology load, the system SHALL perform it as a full load (see "Full technology load") without the readability pre-check (see "Readability pre-check"). The trigger conditions (a `tech` header naming a different technology, the cell not forced, no paint in memory), the PDK search roots and directory walk, the `T.magicrc` script, the search-path additions and the open_pdks `libs.ref` handling are owned by capability `cell-library-resolution`, requirement "Automatic technology discovery".

Compatibility: behavioral

The environment variables, directory walking and `.magicrc` mechanism are `informative`. Hefesto SHALL provide an equivalent "find PDK technology by name" facility with a configurable list of roots (see Q-TECH-28 and capability `hefesto-files-and-pdks`).

(Magic ref: database/DBio.c:224-320, database/DBio.c:577-735)

#### Scenario: open_pdks layout
- **GIVEN** `PDK_ROOT=/opt/pdk`, the file `/opt/pdk/sky130A/libs.tech/magic/sky130A.tech`, and no paint in memory
- **WHEN** a cell with header `tech sky130A` is read while the current technology is `minimum`
- **THEN** `/opt/pdk/sky130A/libs.tech/magic` is appended to the system path and sky130A is loaded, even if its first logical line is not `tech`

### Requirement: Internal grid scale

The system SHALL keep a global internal-grid scale as a reduced fraction "I internal units = L lambda", with positive integers I and L. It starts at 1:1, and a technology load keeps it (a load does not reset it). The number of internal units per lambda is I/L. Every technology value given in lambda is held internally multiplied by I/L, as defined by each owner.

Compatibility: exact

The scale changes only through these routes:
- `scalegrid` (see "scalegrid command");
- the minimum-grid post-load step;
- `tech lambda`, which sets the fraction without rescaling anything (see capability `tech-command` and Q-TECH-26);
- cell reading with an explicit scale or a `magscale` header (see capability `cell-file-format`, requirement "Coordinate scaling on read");
- CIF/GDS input that needs a finer grid (see the `mask-*` capabilities).

Every change keeps the fraction reduced: the greatest common factor of I and L is divided out.

(Magic ref: database/DBio.c:105, commands/CmdRS.c:356-361, utils/fraction.c:83-98, database/DBio.c:790-812)

#### Scenario: Reduced fraction
- **GIVEN** a scale of 1:1
- **WHEN** `scalegrid 2 4` is executed
- **THEN** the scale becomes I = 2, L = 1 (2 internal units = 1 lambda)

### Requirement: scalegrid command

The command `scalegrid a b`, or `scalegrid a/b`, or `scalegrid a:b`, SHALL change the internal grid so that the internal unit becomes a/b of its former size, rescaling every scale-dependent table and all layout. This requirement owns the rescaling procedure; capability `cmd-coordinates-and-units`, requirement "Scalegrid command", summarizes the command surface and defers here.

Compatibility: exact

1. **Parse.** a and b must be integers greater than 0. Otherwise report `Usage:  scalegrid a b, where a and b are strictly positive integers` and change nothing.
   - In the one-argument form, the first `:` is the separator if present, and otherwise the first `/`.
   - A one-argument form without a separator is a usage error.
2. **Equal values.** If a = b, skip to step 9 (report only).
3. **Reduce.** Reduce a/b by the greatest common factor.
4. **Grid limit.** If the grid-limit check for factor (a, b) fails (see "Grid limit check"), report `Grid scaling is finer than limit set by the process!` and change nothing.
5. **Rescale tables.** Rescale every scale-dependent table by b/a: cifinput, cifoutput, drc, extract, wiring, lef and router. Then recompute the maze-router and interactive-router parameters.
6. **Rescale layout.** Multiply every coordinate of every cell in memory by b/a: tiles, labels, cell uses and bounding boxes. Each value v becomes ⌊v·b/a⌋, rounded toward negative infinity, computed in 64-bit arithmetic. Values v with v ≥ 2³⁰−6 or v ≤ −(2³⁰−6) are the "infinity" markers (infinity = 2³⁰−4) and are not scaled. A cell whose geometry does not scale exactly is marked modified.
7. **Update the scale.** Set L := L·a and I := I·b, then reduce.
8. **Update the view.** Rescale the cursor box by b/a with the same rounding. Rescale every window view to keep the same visible area, and redraw. Discard the whole undo history; this step cannot be undone.
9. **Report.** Print `<I> Magic internal unit<s> = <L> Lambda`, where `<s>` is empty when I = 1 and `s` otherwise.

(Magic ref: commands/CmdRS.c:265-389, database/DBcellsrch.c:1457-1560)

#### Scenario: Halving the internal unit
- **GIVEN** scale 1:1 and a box from (0,0) to (3,5)
- **WHEN** `scalegrid 1 2` is executed
- **THEN** the box becomes (0,0) to (6,10)
- **AND** `2 Magic internal units = 1 Lambda` is printed

#### Scenario: Rounding of negative coordinates
- **GIVEN** scale 2:1 (I = 2, L = 1) and a tile edge at x = −3
- **WHEN** `scalegrid 2 1` is executed (factor b/a = 1/2)
- **THEN** the edge moves to x = −2 (⌊−1.5⌋), and the scale becomes 1:1

#### Scenario: Invalid arguments
- **GIVEN** any state
- **WHEN** `scalegrid 0 2` is executed
- **THEN** the usage message is printed and nothing changes

### Requirement: Grid limit check

The grid-limit check for a scale change by factor (n, d) SHALL use the current CIF output style. That style's scaleFactor, expander and gridLimit are defined by the `mask-*` capabilities.

Compatibility: exact

- If there is no current output style, the check passes.
- Otherwise let g = max(gridLimit, 1)·expander·d and s = scaleFactor·n·10.
- The check fails if ⌊s/g⌋ = 0 or if s mod g ≠ 0. Otherwise it passes.

(Magic ref: cif/CIFtech.c:466-489)

#### Scenario: Grid finer than the process allows
- **GIVEN** an output style with scaleFactor 5, expander 1 and gridLimit 5
- **WHEN** `scalegrid 1 2` is checked
- **THEN** g = 10 and s = 50, so ⌊s/g⌋ = 5 and s mod g = 0, and the check passes
- **AND** for `scalegrid 1 20`, g = 100 and s = 50, so ⌊s/g⌋ = 0 and the check fails

### Requirement: Built-in technologies

The system SHALL provide, at minimum, the technology `minimum`. It is used for bootstrap and as the fallback, and has the content below. Its `styles` section names the family `mos`.

Compatibility: behavioral

```
tech
    format 28
    minimum
end
version
    version 0.0
    description "Minimum technology file structure"
end
planes
end
types
end
contact
end
styles
    styletype	mos
end
compose
end
connect
end
cifoutput
end
cifinput
end
drc
end
extract
end
```

Magic also installs the technologies `gdsquery`, for viewing GDS files without a PDK, `scmos`, `scmos-tm`, `scmos-sub`, `scmosWR` and `nmos`. Bundling them is `informative` for Hefesto.

(Magic ref: scmos/minimum.tech.in, scmos/Makefile.in:25-64)

#### Scenario: Minimum technology properties
- **GIVEN** a fresh session with no technology chosen
- **WHEN** startup completes
- **THEN** `tech name` reports `minimum`, there are 6 planes, and there are no user types
