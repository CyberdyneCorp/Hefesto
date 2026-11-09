# Command Coordinates and Units

## Purpose

Defines how editing commands parse distances and coordinates (numbers, unit suffixes, simple expressions), how keyword arguments and direction names are matched, how the `units`, `snap`, `grid` and `scalegrid` commands change the interpretation of values, how pointer positions snap to a grid, and how numeric values are formatted when reported. Every other `cmd-*` capability relies on these rules. The command surface is `behavioral`. The numeric conversions, rounding and formatted output are `exact` because scripts depend on them.

Terminology used throughout the `cmd-*` capabilities:

- **Internal unit (i)**: the integer database grid unit. All stored coordinates are integers in internal units.
- **Lambda (l)**: the technology scale unit. The technology state holds a reduced fraction `Λ = L1 / L0`, the number of internal units per lambda (initially from the technology file, see capability `tech-loading-and-scaling`).
- **U**: the size of one internal unit in nanometres, `U = 10·s / e`, computed in IEEE-754 single precision, where `s` and `e` are the scale factor and expander of the current CIF/GDS output style (see the MASK capabilities), rescaled to the current internal grid as Magic does on `scalegrid` and on a load at a scale other than 1:1: the style's values as read (per lambda), with `s` multiplied by `L0` and `e` by `L1`, reduced by their greatest common factor. When no output style is loaded, U is undefined and a metric operand is an error (see requirement "Unit suffixes" and Q-CMDEDIT-3).
- **User grid**: the per-window rectangle `G = (gx0, gy0)–(gx1, gy1)` set by `grid`. Its width `gx1-gx0` and height `gy1-gy0` are the grid pitches. Default `(0,0)–(1,1)`.
- **Root coordinates**: the coordinate system of the top cell shown in the window. **Edit coordinates**: the coordinate system of the current edit cell. The box is held in root coordinates.

## Requirements

### Requirement: Keyword matching

Every keyword argument of an editing command (options, sub-commands, unit names, and so on) SHALL be matched against that command's keyword table by case-insensitive unique-prefix matching:

1. An argument that equals a table entry (case-insensitive) SHALL select that entry, even if it is also a prefix of other entries.
2. Otherwise, an argument that is a prefix of exactly one entry SHALL select that entry.
3. An argument that is a prefix of two or more entries (and not an exact match of any entry) SHALL be ambiguous.
4. An argument that matches no entry SHALL be unrecognized.

The empty string is a prefix of every entry, so it is ambiguous whenever the table has more than one entry. Only the leading word of each table entry (up to the first space) takes part in matching. A leading `::magic::` or `magic::` qualifier SHALL be ignored before matching. Commands whose description says an argument is compared "exactly" (for example `pick`, `erase`, `cursor`) do not use this rule. This is the same table lookup that capability `ui-command-dispatch` requirement "Command name lookup" specifies for command names.

Compatibility: exact

(Magic ref: utils/lookup.c:40-126, utils/lookup.c LookupStruct)

#### Scenario: Unique prefix
- **GIVEN** the `units` keyword table `default internal lambda grid user microns list print noprint`
- **WHEN** the argument `mi` is matched
- **THEN** it SHALL select `microns`

#### Scenario: Ambiguous prefix
- **GIVEN** the same table
- **WHEN** the argument `l` is matched
- **THEN** the result SHALL be ambiguous because both `lambda` and `list` start with `l`

#### Scenario: Exact match wins over longer entries
- **GIVEN** the direction table that contains `n`, `ne`, `north`, `northeast` and `northwest`
- **WHEN** `n` is matched
- **THEN** it SHALL select `n` (north)
- **AND** `no` SHALL be ambiguous

### Requirement: Direction and position names

Commands that take a direction or position SHALL accept the following names, matched with the keyword-matching rule:

| Names | Meaning |
|---|---|
| `n`, `north`, `top`, `up`, `u` | north |
| `s`, `south`, `bottom`, `down`, `d` | south |
| `e`, `east`, `right` | east |
| `w`, `west`, `left` | west |
| `ne`, `northeast`, `tr`, `ur` | north-east |
| `nw`, `northwest`, `tl`, `ul` | north-west |
| `se`, `southeast`, `br`, `dr` | south-east |
| `sw`, `southwest`, `bl`, `dl` | south-west |
| `center` | center |

The first four rows and `center` are excluded from the diagonal rows: a command that requires a Manhattan direction SHALL accept only north, south, east and west, and SHALL reject every other name with an error that lists the legal Manhattan names. An ambiguous name (for example `t`, which matches `tl`, `top` and `tr`) SHALL be rejected with an "ambiguous" error. An unknown name SHALL be rejected with a "not a valid direction or position" error followed by the list of legal names.

Compatibility: behavioral

(Magic ref: utils/geometry.c:353-450, utils/geometry.h:92-100)

#### Scenario: Synonyms
- **WHEN** `fill up` and `fill n` are run with the same box and layers
- **THEN** both SHALL produce identical results

#### Scenario: Diagonal rejected where Manhattan required
- **WHEN** `fill ne` is run
- **THEN** the command SHALL fail with a "not a Manhattan direction or position" error and change nothing

### Requirement: Numeric literal parsing

A coordinate or distance argument SHALL be parsed as a sequence of operands. Each operand SHALL begin with a decimal floating-point literal: optional sign, digits with an optional fractional part, and an optional exponent (`e`/`E`). A single leading `{` or `"` and any following whitespace SHALL be skipped before parsing. If the argument does not start with a number, the command SHALL report "Coordinate value cannot be parsed: assuming 0" and SHALL use the value accumulated so far (0 for the first operand).

Each operand SHALL be converted to internal units as a double-precision value (see the unit requirements below) and then rounded to the nearest integer, with halves rounded away from zero. Rounding SHALL happen per operand, before the operators are applied.

Compatibility: exact

(Magic ref: commands/CmdSubrs.c:122-150, commands/CmdSubrs.c:241-242)

#### Scenario: Half rounds away from zero (lambda = 1 internal unit)
- **GIVEN** `units internal` is set
- **WHEN** the arguments `2.5` and `-2.5` are parsed
- **THEN** the results SHALL be `3` and `-3` internal units

#### Scenario: Unparseable value
- **WHEN** `box width abc` is run
- **THEN** the message "Coordinate value cannot be parsed:  assuming 0" SHALL be reported
- **AND** the box width SHALL become 0

#### Scenario: Exponent notation
- **GIVEN** `units internal`
- **WHEN** `1e2` is parsed
- **THEN** the result SHALL be `100`

### Requirement: Unit suffixes

The text immediately following an operand's number SHALL select its unit:

| Suffix | Unit | Conversion of value `v` to internal units |
|---|---|---|
| `l` | lambda | `v · L1 / L0` |
| `i` | internal | `v` |
| `g` | user grid | relative: `v · pitch`; absolute: `v · pitch + origin` (pitch and origin of the axis) |
| `nm` | nanometre | `v / (10·s / (e·1))` |
| `um`, `u` (not followed by a letter or digit), `micron…` | micrometre | `v / (10·s / (e·1000))` |
| `mm` | millimetre | `v / (10·s / (e·1000000))` |
| `cm` | centimetre | `v / (10·s / (e·10000000))` |
| `centimicron…`, or exactly `cu` at the end of the argument | centimicron | `v / (10·s / (e·10))` |

The metric divisor `10·s / (e·k)` SHALL be evaluated in single precision before the double-precision division. `micron` and `centimicron` are matched as prefixes of the remaining text, so `microns` is accepted.

A two-letter suffix `Xm` whose first letter is not `n`, `u`, `m` or `c` SHALL produce the error "Unknown metric prefix "Xm"; assuming internal units" and the value SHALL be taken as internal units. Any other unrecognized suffix SHALL produce "Unknown coordinate type at "…"; assuming internal units" and the value SHALL be taken as internal units.

Grid units refer to the user grid of the window the command applies to. For a relative quantity (a size or distance), the origin SHALL NOT be added. For an absolute coordinate, it SHALL be added.

If no layout window can be determined, a `g` operand (or an un-suffixed operand whose effective input unit is the user grid under `units user`) SHALL make the argument invalid: Hefesto SHALL report the error `Grid units need a layout window` and the command SHALL fail and change nothing, as for division by zero (Q-CMDEDIT-5). Magic instead leaves the parse loop before the operand is accumulated and returns 0 or a partial sum; Hefesto does not reproduce this in either mode (see Q-CMDEDIT-4, resolution fix).

If no CIF/GDS output style is loaded, a metric operand (`nm`, `um`, `u`, `micron…`, `mm`, `cm`, `centimicron…`, `cu`) SHALL make the argument invalid: Hefesto SHALL report the error `Metric units need a CIF/GDS output style` and the command SHALL fail and change nothing. Magic divides by a scale of 1.0, so `1nm`, `1um` and `1mm` all give 1 internal unit; Hefesto does not reproduce this in either mode (see Q-CMDEDIT-3, resolution fix).

Compatibility: exact

(Magic ref: commands/CmdSubrs.c:168-256, cif/CIFmain.c:139-146; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-3, docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-4)

#### Scenario: Microns with a 10 nm internal unit
- **GIVEN** an output style with `U = 10` nm per internal unit
- **WHEN** `1.5um` is parsed
- **THEN** the result SHALL be `150` internal units
- **AND** `0.005um` SHALL give `1` (0.5 rounds away from zero)
- **AND** `25nm` SHALL give `3` (2.5 rounds away from zero)

#### Scenario: Lambda suffix
- **GIVEN** `Λ = 2` internal units per lambda (L1 = 2, L0 = 1)
- **WHEN** `3l` is parsed
- **THEN** the result SHALL be `6` internal units

#### Scenario: Grid suffix, absolute vs relative
- **GIVEN** the user grid is `(5,5)–(15,25)` (x pitch 10, y pitch 20)
- **WHEN** `2g` is parsed as an absolute x coordinate
- **THEN** the result SHALL be `25`
- **AND** `2g` parsed as a relative y distance SHALL be `40`

#### Scenario: Unknown metric prefix
- **WHEN** `5km` is parsed
- **THEN** the error "Unknown metric prefix "km"; assuming internal units" SHALL be reported
- **AND** the result SHALL be `5`

#### Scenario: Metric operand without an output style
- **GIVEN** no CIF/GDS output style is loaded and the box is (0,0)–(10,10)
- **WHEN** `box width 1um` is run
- **THEN** the error `Metric units need a CIF/GDS output style` SHALL be reported, whatever the `magic-compat` setting
- **AND** the box SHALL be unchanged

#### Scenario: Grid operand without a layout window
- **GIVEN** no layout window can be determined for the command
- **WHEN** an argument `5+2g` is parsed
- **THEN** the error `Grid units need a layout window` SHALL be reported, whatever the `magic-compat` setting
- **AND** the command SHALL fail and change nothing

### Requirement: Default unit of un-suffixed values

An operand with no suffix that is followed by the end of the argument or by whitespace SHALL use the **effective input unit**:

- If the `units` setting is `default`, the effective input unit SHALL be the current `snap` setting: `internal`, `lambda` or user grid.
- Otherwise, it SHALL be the `units` setting: `internal`, `lambda`, `user` (grid) or `microns`.

An operand with no suffix that is immediately followed by an operator character (`+ - * /`): with `magic-compat` ON (the default) it SHALL be taken as internal units, whatever the settings, as in Magic, whose suffix tests exempt the operator characters from the unknown-suffix check; with it OFF it SHALL use the effective input unit like any other un-suffixed operand (see Q-CMDEDIT-1, resolution: replicate in magic-compat, fix in native).

At startup, `units` SHALL be `default` and `snap` SHALL be `lambda`, so un-suffixed values are lambda.

Compatibility: exact

(Magic ref: commands/CmdSubrs.c:153-206, commands/CmdSubrs.c:251-252, dbwind/DBWtools.c:72-82; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-1)

#### Scenario: Startup default is lambda
- **GIVEN** a fresh session with `Λ = 2`
- **WHEN** `box width 5` is run
- **THEN** the box width SHALL become `10` internal units

#### Scenario: Units override snap
- **GIVEN** `snap lambda` and then `units microns`, with `U = 10` nm
- **WHEN** `box width 2` is run
- **THEN** the box width SHALL become `200` internal units

#### Scenario: Un-suffixed operand before an operator with magic-compat ON
- **GIVEN** `magic-compat` ON, `Λ = 2`, units `default` and snap `lambda`
- **WHEN** `box width 3+1` is run
- **THEN** the box width SHALL become `5` internal units: `3` is internal because it is followed directly by `+`, and `1` is lambda (2 internal)

#### Scenario: Un-suffixed operand before an operator with magic-compat OFF
- **GIVEN** `magic-compat` OFF, `Λ = 2`, units `default` and snap `lambda`
- **WHEN** `box width 3+1` is run
- **THEN** the box width SHALL become `8` internal units: both operands are lambda

### Requirement: Coordinate expressions

An argument MAY contain several operands joined by the binary operators `+`, `-`, `*`, `/`, with optional spaces or tabs around operators (spaces only arise when the argument is quoted as a single word). The expression SHALL be evaluated strictly left to right with no operator precedence, using 32-bit signed integer arithmetic on the already-rounded operands. Division SHALL truncate toward zero. A closing `}` or `"` SHALL end the expression.

After an operand with a unit suffix, Magic's parser does not skip the suffix, so an operator that follows a suffixed operand ends the expression. With `magic-compat` ON (the default), Hefesto SHALL do the same: the expression SHALL end at the suffixed operand, its value SHALL be the value accumulated up to and including that operand, and Hefesto SHALL report the warning `Ignored text after unit suffix: "<rest>"`, where `<rest>` is the ignored remainder of the argument. With `magic-compat` OFF, the suffix SHALL be skipped and evaluation SHALL continue with the operator that follows it (see Q-CMDEDIT-2, resolution: replicate in magic-compat, fix in native). Any other character after an operand ends the expression silently in both modes. Division by an operand that rounds to 0 is undefined in Magic. Hefesto SHALL reject it with an error (see Q-CMDEDIT-5).

Compatibility: exact

(Magic ref: commands/CmdSubrs.c:243-315; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-1, docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-2, docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-5)

#### Scenario: Left-to-right evaluation
- **GIVEN** `units internal`
- **WHEN** `{2 + 3 * 4}` is parsed
- **THEN** the result SHALL be `20`, not `14`

#### Scenario: Operand directly before an operator is internal with magic-compat ON
- **GIVEN** `magic-compat` ON, `Λ = 2` with units `default` and snap `lambda`
- **WHEN** `100+5` is parsed
- **THEN** the result SHALL be `110`: `100` is internal because it is followed directly by `+`, and `5` is lambda (10 internal)

#### Scenario: Operand directly before an operator uses the input unit with magic-compat OFF
- **GIVEN** `magic-compat` OFF, `Λ = 2` with units `default` and snap `lambda`
- **WHEN** `100+5` is parsed
- **THEN** the result SHALL be `210`: both operands are lambda

#### Scenario: Suffix ends the expression with magic-compat ON
- **GIVEN** `magic-compat` ON and `U = 10` nm
- **WHEN** `1um+2um` is parsed
- **THEN** the result SHALL be `100` (the `+2um` part is ignored)
- **AND** the warning `Ignored text after unit suffix: "+2um"` SHALL be reported

#### Scenario: Suffixed operands continue the expression with magic-compat OFF
- **GIVEN** `magic-compat` OFF and `U = 10` nm
- **WHEN** `1um+2um` is parsed
- **THEN** the result SHALL be `300`
- **AND** no warning SHALL be reported

#### Scenario: Truncating division
- **GIVEN** `units internal`
- **WHEN** `{7 / 2}` and `{-7 / 2}` are parsed
- **THEN** the results SHALL be `3` and `-3`

### Requirement: Units command

`units [type [print|noprint]]` SHALL query or set the units used to read un-suffixed values and to report values. `type` is matched with the keyword-matching rule against `default`, `internal`, `lambda`, `grid`, `user`, `microns`, `list`, `print`, `noprint`. `grid` and `user` are synonyms.

- `units` with no argument, or `units list`, SHALL report the current setting. In scripting mode it SHALL return a list: the type name (`default`, `internal`, `lambda`, `user` or `microns`) followed, for any type except `default`, by `print` or `noprint`. In interactive mode it SHALL print the name, followed by ` print` when the print flag is set.
- `units T` (T = `internal`, `lambda`, `user`/`grid`, `microns`) SHALL set the type. The print flag SHALL keep its previous value, or stay clear if the previous type was `default`.
- `units T print` / `units T noprint` SHALL set the type and set or clear the print flag. Any other third argument SHALL produce "Unrecognized units option" and leave the type set.
- `units default` SHALL restore backward-compatible behavior: un-suffixed input follows `snap`, and reported values are plain internal-unit integers.
- `units print` / `units noprint` given alone SHALL set or clear the print flag when the current type is not `default`. If the current type is `default`: with `magic-compat` ON (the default), the command SHALL also change the type to `microns`, as Magic does (Magic's own "Display units" menu sends these forms alone); with it OFF, the command SHALL report the error `set a unit type first` and change nothing (see Q-CMDEDIT-6, resolution: replicate in magic-compat, fix in native).
- An unrecognized or ambiguous type SHALL print the usage `Usage: units [default | internal | lambda | microns | user] [print]` and change nothing.

When the print flag is set, reported values SHALL carry a unit suffix (see "Reported value formatting").

Compatibility: behavioral

(Magic ref: commands/CmdTZ.c:862-1004, commands/CmdTZ.c:891-949, dbwind/dbwind.h:179-205, tcltk/wrapper.tcl:1333-1335; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-6)

#### Scenario: Query in scripting mode
- **GIVEN** `units microns print` was run
- **WHEN** `units` is run in scripting mode
- **THEN** the result SHALL be the list `microns print`

#### Scenario: Print flag preserved
- **GIVEN** `units lambda print`
- **WHEN** `units internal` is run
- **THEN** the setting SHALL be `internal print`

#### Scenario: Bare print from default with magic-compat ON
- **GIVEN** `magic-compat` ON and a fresh session (units `default`)
- **WHEN** `units print` is run
- **THEN** `units` SHALL subsequently report `microns print`
- **AND** later un-suffixed values SHALL be read as microns

#### Scenario: Bare print from default with magic-compat OFF
- **GIVEN** `magic-compat` OFF and a fresh session (units `default`)
- **WHEN** `units print` is run
- **THEN** the error `set a unit type first` SHALL be reported
- **AND** `units` SHALL subsequently report `default`

### Requirement: Snap command

`snap [internal|off|lambda|grid|user|on|list]` SHALL set or report the **snap grid**. The snap grid is used to snap pointer positions and, while `units` is `default`, to give the unit of un-suffixed values. Keywords are matched with the keyword-matching rule:

- `internal` and `off` SHALL select the internal grid.
- `lambda` SHALL select the lambda grid.
- `grid`, `user` and `on` SHALL select the window's user grid.
- `snap` with no argument and `snap list` SHALL report the current snap grid as `internal`, `lambda` or `user` (returned as the result in scripting mode, printed otherwise).
- An unrecognized or ambiguous keyword SHALL print `Usage: snap [internal | lambda | user]` and change nothing.

Backward compatibility rule: if `units` is `default` and the argument selects `internal`, the command SHALL also set `units internal`, and SHALL report a warning that this usage is deprecated. No other snap keyword changes `units`.

The snap setting is global to the session, not per window. Its startup value is `lambda`.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:2944-3021, dbwind/DBWtools.c:72)

#### Scenario: Deprecated snap internal
- **GIVEN** units `default`
- **WHEN** `snap internal` is run
- **THEN** both the snap grid and the units SHALL become `internal`
- **AND** a deprecation warning SHALL be reported

#### Scenario: Snap after explicit units
- **GIVEN** `units lambda`
- **WHEN** `snap internal` is run
- **THEN** units SHALL remain `lambda`
- **AND** no warning SHALL be reported

#### Scenario: Query
- **GIVEN** `snap on` was run
- **WHEN** `snap` is run in scripting mode
- **THEN** the result SHALL be `user`

### Requirement: Pointer snapping

A point obtained from the pointer (cursor) in root coordinates SHALL be snapped before a command uses it, unless the snap grid is `internal`:

- For the `lambda` snap grid, the reference grid SHALL have origin (0,0) and pitch `p = max(1, floor(L1 / L0))` on both axes.
- For the `user` snap grid, it SHALL be the window's user grid (origin `(gx0, gy0)`, pitches `gx1-gx0` and `gy1-gy0`).

On each axis independently, with origin `o` and pitch `p`, let `t = x - o`. The two bracketing grid lines are:

- if `t >= 0`: `lo = p·trunc(t/p) + o`, `hi = lo + p`
- if `t < 0`: `hi = p·trunc(t/p) + o`, `lo = hi - p`

where `trunc` truncates toward zero. The snapped coordinate SHALL be `lo` if `|x - lo| < |x - hi|`, and `hi` otherwise, so ties go to `hi`. Values typed on the command line SHALL NOT be snapped.

Compatibility: exact

(Magic ref: dbwind/DBWtools.c:1095-1162, dbwind/DBWtools.c:121-138)

#### Scenario: Snap to user grid with tie
- **GIVEN** `snap user` and a user grid `(0,0)–(4,4)`
- **WHEN** the pointer is at root point (5, 6)
- **THEN** the snapped point SHALL be (4, 8)

#### Scenario: Negative coordinate
- **GIVEN** a lambda grid with pitch 2
- **WHEN** the pointer is at x = -3
- **THEN** the snapped x SHALL be -2: `t = -3`, `hi = 2·(-1) = -2`, `lo = -4`, and the tie goes to `hi`

#### Scenario: Typed values are not snapped
- **GIVEN** `snap lambda` with `Λ = 2`
- **WHEN** `box values 1i 1i 3i 3i` is run
- **THEN** the box SHALL be exactly (1,1)–(3,3)

### Requirement: Grid command

`grid` SHALL control the user grid of the current layout window. The forms are:

- `grid` (no arguments) SHALL toggle grid display.
- `grid toggle` SHALL toggle grid display.
- `grid on` / `grid off` SHALL turn display on or off. `grid 0` (the literal argument `0`) SHALL mean `off`.
- `grid state` SHALL return a boolean in scripting mode, or print `Grid is on|off`.
- `grid box` or `grid what` (no values) SHALL return the four integers `gx0 gy0 gx1 gy1` in internal units, as a space-separated string in scripting mode, or print `Grid unit box is (gx0, gy0) to (gx1, gy1)`.
- `grid help` SHALL print usage.
- `grid multiple [m]` SHALL set the minimum display multiple, an integer 1–255, or `off` (= 1). With no value it SHALL return the current multiple in scripting mode. Magic stores the value but never reads it when drawing (see capability `ui-windows-and-views`, requirement "Grid display and grid command", and Q-UI-21).
- `grid xs [ys [xo yo]]` and `grid box xs [ys [xo yo]]` SHALL set the grid. `xs` is parsed as a relative x distance, `ys` as a relative y distance (default `ys = xs`), and `xo yo` as absolute coordinates (default 0 0). The result SHALL be `G = (xo, yo)–(xo+xs, yo+ys)`, and display SHALL be turned on. A spacing that is not greater than 0 SHALL produce "Grid spacing must be greater than zero." and change nothing. Exactly three numeric arguments, or more than four, SHALL produce a usage error.

A value given to `grid multiple` outside 1–255 SHALL report a usage error and SHALL NOT be stored, whatever the `magic-compat` setting; the previous multiple SHALL remain. Magic stores such a value modulo 256, but the stored value is only read back by the query and never by drawing code, so Hefesto does not reproduce this (see Q-CMDEDIT-7, resolution fix, consistent with Q-UI-21). The user grid is per window. A new window's grid SHALL be `(0,0)–(1,1)`, off.

Compatibility: behavioral

(Magic ref: commands/CmdFI.c:1273-1469, commands/CmdFI.c:1407-1413, dbwind/DBWprocs.c:114-117; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-7)

#### Scenario: Set grid with origin
- **GIVEN** units `internal`
- **WHEN** `grid 10 20 5 5` is run and then `grid what` is run in scripting mode
- **THEN** the result SHALL be `5 5 15 25`
- **AND** the grid SHALL be displayed

#### Scenario: Zero spacing rejected
- **WHEN** `grid 0 5` is run
- **THEN** "Grid spacing must be greater than zero." SHALL be reported
- **AND** the grid SHALL be unchanged

#### Scenario: Out-of-range grid multiple not stored
- **GIVEN** `grid multiple 4` was run
- **WHEN** `grid multiple 300` is run and then `grid multiple` is run in scripting mode
- **THEN** a usage error SHALL be reported for `300`
- **AND** the result of the query SHALL be `4`

### Requirement: Scalegrid command

`scalegrid a b`, `scalegrid a/b` or `scalegrid a:b` SHALL rescale the internal grid relative to lambda by the factor `a/b`. After the command, one lambda SHALL contain `b/a` times as many internal units as before. `a` and `b` SHALL be strictly positive decimal integers. Any other form SHALL report `Usage:  scalegrid a b, where a and b are strictly positive integers`.

If `a = b`, nothing SHALL change. Otherwise:

1. The fraction `a/b` SHALL be reduced by its greatest common divisor.
2. If the new internal unit would be finer than the output style's grid limit, the command SHALL fail with "Grid scaling is finer than limit set by the process!" and change nothing. With output-style scale `s`, expander `e` and grid limit `g` (`g = 1` if 0), the check is: fail if `floor(10·s·a / (g·e·b)) = 0` or if `(10·s·a) mod (g·e·b) ≠ 0`.
3. Every coordinate in every loaded cell SHALL be multiplied by `b/a` with floor division: `v' = floor(v·b / a)`, computed in 64-bit arithmetic. Technology distances (DRC, extraction, wiring, LEF, router, CIF/GDS input and output scales) SHALL be rescaled as specified in capability `tech-loading-and-scaling`.
4. The lambda ratio SHALL become `L0' = L0·a`, `L1' = L1·b`, reduced to lowest terms.
5. The box corners SHALL be scaled with the same floor rule. Window views SHALL be adjusted to keep the same visible area.
6. The undo history SHALL be discarded. The operation itself is not undoable.

The command SHALL then report `<L1> Magic internal unit[s] = <L0> Lambda` (singular `unit` when L1 = 1). The report is also printed when `a = b`.

Scaling is cumulative: running `scalegrid 1 2` twice gives 4 internal units per lambda.

The rescaling procedure itself (which tables are rescaled, coordinate rounding including the unscaled infinity markers, and the scale update) is also specified in capability `tech-loading-and-scaling` requirement "scalegrid command"; that requirement governs the rescaling, and this one the command surface.

Compatibility: exact

(Magic ref: commands/CmdRS.c:265-394, database/DBcellsrch.c:1457-1509, cif/CIFtech.c:466-488)

#### Scenario: Halve the internal unit
- **GIVEN** `Λ = 1` and a box (0,0)–(3,5) and a metal rectangle (−3,0)–(3,4)
- **WHEN** `scalegrid 1 2` is run
- **THEN** the box SHALL become (0,0)–(6,10)
- **AND** the rectangle SHALL become (−6,0)–(6,8)
- **AND** the message `2 Magic internal units = 1 Lambda` SHALL be printed

#### Scenario: Coarsening uses floor
- **GIVEN** `Λ = 2` and a coordinate 3 and a coordinate −3
- **WHEN** `scalegrid 2 1` is run
- **THEN** the coordinates SHALL become 1 and −2

#### Scenario: Bad arguments
- **WHEN** `scalegrid 0 2` is run
- **THEN** the usage message SHALL be reported and nothing SHALL change

### Requirement: Reported value formatting

When a command reports a length, coordinate or area, the value SHALL be formatted according to `units` as follows. `v` is the value in internal units. All scaling arithmetic SHALL be done in IEEE-754 single precision, then printed with C `printf` rounding.

| units | Length | Area (square) | Suffix when print flag set |
|---|---|---|---|
| `default` | `%d` of `v` | `%d` of `v` | none (never) |
| `internal` | `%d` of `v` | `%d` of `v` | `i` / `i^2` |
| `lambda` | `%.3f` of `v·(L0/L1)` | `%.6f` of `v·(L0/L1)²` | `l` / `l^2` |
| `microns` | `%.3f` of `v·U/1000` | `%.6f` of `v·(U/1000)²` | `um` / `um^2` |
| `user` | `%.3f` of `v / pitch` (x pitch for x values, y pitch for y values) | `%.6f` of `v / (xpitch·ypitch)` | `gx` or `gy` / `gx*gy` |

After `%.3f` or `%.6f` formatting, trailing zeros SHALL be removed. If that leaves a trailing decimal point, it SHALL be removed too. For example, `7.500` becomes `7.5` and `100.000` becomes `100`.

If units is `user` but no layout window can be determined, the value SHALL be formatted as for `default`.

For values held in output-style units (CIF/GDS database units) rather than internal units, the value SHALL first be multiplied by the conversion `dscale = (1/(e·100)) / (10·s/(e·1000))` before the scaling above. In `internal` mode such values SHALL be printed with `%.3f` (or `%.6f` for areas) and trimmed.

Compatibility: exact

(Magic ref: dbwind/DBWprocs.c:657-688, dbwind/DBWprocs.c:735-980)

#### Scenario: Lambda with suffix
- **GIVEN** `Λ = 2` (L1 = 2, L0 = 1) and `units lambda print`
- **WHEN** a box width of 15 internal units is reported
- **THEN** the text SHALL be `7.5l`

#### Scenario: Microns
- **GIVEN** `U = 10` nm and `units microns`
- **WHEN** a value of 150 internal units is reported
- **THEN** the text SHALL be `1.5`
- **AND** an area of 20000 square internal units SHALL be reported as `2`

#### Scenario: Default units are raw integers
- **GIVEN** a fresh session
- **WHEN** `box width` is run in scripting mode with a 10-unit wide box
- **THEN** the result SHALL be `10`

### Requirement: Root and edit coordinate frames

Commands SHALL interpret and report coordinates in root coordinates (the top cell of the window), unless the command defines an edit-coordinate form. For edit-coordinate forms (for example the `-edit` switch of `box`, `pick x y`, `polygon`, and the `to x y` forms of `copy`/`move`), the edit-to-root transform of the current edit cell SHALL be applied.

An editing command that modifies paint, labels or subcells SHALL operate on the current edit cell. The area it uses SHALL be the box transformed into edit coordinates. This requires that the box lies in a window whose root cell is the root of the edit cell's window. Otherwise the command SHALL fail with "The box isn't in a window on the edit cell." If there is no box, it SHALL fail with "Box must be present".

Compatibility: behavioral

(Magic ref: dbwind/DBWtools.c:320-352, commands/CmdSubrs.c:1116-1186)

#### Scenario: Box outside edit window
- **GIVEN** two windows showing different top cells, with the box in the window that does not contain the edit cell
- **WHEN** `paint metal1` is run
- **THEN** the command SHALL fail with "The box isn't in a window on the edit cell."
- **AND** no paint SHALL change

### Requirement: Message texts

The exact wording of error and informational messages quoted in the `cmd-*` capabilities is documented for reference. Hefesto MAY use different wording, but it SHALL report the same condition, and in scripting mode SHALL signal failure where Magic prints an error.

Compatibility: informative

(Magic ref: commands/CmdSubrs.c:146, commands/CmdTZ.c:888, commands/CmdRS.c:976)

#### Scenario: Equivalent diagnostics
- **WHEN** a coordinate cannot be parsed in Hefesto
- **THEN** Hefesto SHALL report a diagnostic that names the bad argument
- **AND** it SHALL substitute 0 exactly as Magic does
