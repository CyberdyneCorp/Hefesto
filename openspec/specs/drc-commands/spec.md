# DRC Commands

## Purpose

Defines the user-facing `drc` command of a Magic-compatible editor: option parsing, every option and its effect, printed output, and the values returned in scripting mode (`drc list …` / `drc listall …`), including the error counting, error finding and "why" reporting semantics and the why-text substitution. The checks themselves are defined in `drc-engine`, `drc-rule-semantics` and `drc-cif-rules`. Hefesto offers every option with the same semantics; exact text output is required where marked. Where an approved decision resolves a Magic quirk as *compat* (replicate under magic-compat, fix with it off), the requirement states both modes, and the `magic-compat` setting ON (the default) gives Magic's output.

## Requirements

### Requirement: Command syntax and option matching
The command `drc [list|listall] option [args]` SHALL select the option by case-insensitive unique-prefix match against the option names below (an exact match wins; ambiguity or no match reports `<arg> isn't a valid drc option.` and prints help), where a first argument beginning with `list` sets list mode and one beginning with `listall` additionally sets list-all mode, and the option names `*flatcheck`, `*halo`, `*showint`, `*stepsize` include the leading asterisk (wizard options).
Compatibility: behavioral

Options: `*flatcheck`, `*halo [d]`, `*showint radius`, `*stepsize [d]`, `catchup`, `check`, `count [total]`, `euclidean on|off`, `find [nth|text]`, `help [wizard]`, `ignore [text|none]`, `off`, `on`, `status`, `style [name]`, `printrules [file]`, `rulestats`, `statistics`, `why`.

- Only `printrules`, `find`, `*showint`, `help`, `euclidean`, `*stepsize`, `*halo`, `count`, `style` and `ignore` accept an extra argument; extra arguments for other options print `Wrong arguments in "drc <arg>" command:`, `    drc <option table line>` and `Try "drc help" for more help.` and do nothing.
- With no option: `No option given in ":drc" command.` followed by help. `drc list` or `drc listall` with no option does the same, in both `magic-compat` settings (Magic then reads a missing argument; see Q-DRC-22, resolution: fix).
- Options that operate on "the box" use the box of the window containing the box (capability `cmd-box`); if there is no such window the command does nothing.
- Help output (exact): `DRC commands have the form ":drc [list|listall] option",` (informative: Magic's non-Tcl build prints `DRC commands have the form ":drc option",`), then ` where option is one of:` and one line `    <option table entry>` per option in the order above, skipping `*` entries unless `help wizard`.

(Magic ref: commands/CmdCD.c:4054-4150, commands/CmdCD.c:4448-4465, utils/lookup.c)

#### Scenario: Ambiguous prefix
- **GIVEN** the command `drc c`
- **WHEN** executed
- **THEN** `c isn't a valid drc option.` is printed followed by the help text, since `catchup`, `check` and `count` all match

#### Scenario: Wizard option requires asterisk
- **GIVEN** the command `drc halo`
- **WHEN** executed
- **THEN** `halo isn't a valid drc option.` is printed
- **AND** `drc *halo` prints the halo

### Requirement: Background checker control
`drc on` SHALL enable background checking (only if it is currently off), `drc off` SHALL abort any in-progress background check and disable background checking, and `drc status` SHALL report whether it is enabled, treating the transient "not set" state as on.
Compatibility: behavioral

Output: the boolean `1`/`0` is returned as the command result (informative: Magic's non-Tcl build prints `DRC checker is on.` / `DRC checker is off.`). `drc off` also restores the normal prompt. While off, edits still mark check areas, which are processed when re-enabled or by `drc catchup`.

(Magic ref: commands/CmdCD.c:4513-4542)

#### Scenario: Status in scripting mode
- **GIVEN** background DRC enabled
- **WHEN** `drc off` then `drc status` are executed
- **THEN** the result of `drc status` is `0`

### Requirement: drc catchup and drc check
`drc catchup` SHALL run the background checker to completion (all pending cells and check areas) regardless of the enabled state and restore that state afterwards; `drc check` SHALL mark the box area for rechecking in every cell of the hierarchy under the box as defined in `drc-engine` ("Forced recheck of an area"), without running the check itself. When `drc catchup` finds pending cells, its result is the Tcl `after` identifier `after#N` that Magic's checker leaves behind (it schedules `magic::drcstate busy` and `magic::drcstate idle` with `after idle`, so N grows by 2 per such run, starting at 2 in a fresh session); with nothing pending the result is empty.
Compatibility: behavioral

(Magic ref: drc/DRCmain.c:1007-1025, drc/DRCcontin.c:487-565, drc/DRCmain.c:784-860, commands/CmdCD.c:4186-4196)

#### Scenario: Batch DRC
- **GIVEN** a script running with background DRC off
- **WHEN** it executes `select top cell`, `drc check`, `drc catchup`, `drc count total`
- **THEN** the count reflects a complete check of the top cell

#### Scenario: catchup result
- **GIVEN** a fresh session with one edited cell pending
- **WHEN** `drc catchup` is executed twice
- **THEN** the first result is `after#2` and the second is empty

### Requirement: drc count
`drc count [total]` SHALL count error tiles (tiles of any error type in the error plane, in maximal-horizontal-strip form, over the cell's whole bounding box) of the root cell of the command's window — or, in list-all mode, of the root cell and every loaded cell reached recursively through uses overlapping the window area (the window's visible area if the command came from a window, else the box) — each distinct cell counted once.
Compatibility: exact

Output and results:
- Without `total`, for each cell with a non-zero count: `Cell <name> has <n> error tiles.` or, for n = 1, `Cell <name> has just one error tile.`; cells with zero errors are not listed. Magic's cell order comes from a table keyed by memory addresses, so Hefesto SHALL list the cells in the deterministic order of capability `hefesto-output-determinism` (by cell name; decision X-1), and golden comparison SHALL canonicalize that order.
- Then, when not in list mode: with `total`, `Total DRC errors found: <sum>`; without `total`, if background DRC is not off, a total line is printed whose value depends on the `magic-compat` setting (see Q-DRC-14, resolution: replicate under magic-compat, fix with it off): with it ON (the default) `Total DRC errors found: 0` regardless of the per-cell counts, as in Magic; with it OFF `Total DRC errors found: <sum of the per-cell counts>`. With background DRC off nothing follows the per-cell lines.
- List mode (Tcl): without `total`, the result is a list of `{cellname count}` pairs (only cells with count > 0); with `total`, the result is the integer sum.
- Cells not yet loaded are not descended into.

(Magic ref: commands/CmdCD.c:4197-4274, drc/DRCmain.c:870-1000)

#### Scenario: Count in scripting mode
- **GIVEN** a top cell `top` whose error plane holds 3 error tiles
- **WHEN** `drc list count` is executed
- **THEN** the result is `{top 3}`
- **AND** `drc list count total` returns `3`

#### Scenario: Single error message
- **GIVEN** a cell `inv` with exactly one error tile
- **WHEN** `drc count` is executed (non-list)
- **THEN** `Cell inv has just one error tile.` is printed

#### Scenario: Count total line with magic-compat ON
- **GIVEN** `magic-compat` ON, background DRC on, and a top cell `top` with 3 error tiles
- **WHEN** `drc count` is executed (non-list)
- **THEN** `Cell top has 3 error tiles.` and then `Total DRC errors found: 0` are printed (golden DRC-CNT-1)

#### Scenario: Count total line with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the cell of the previous scenario
- **WHEN** `drc count` is executed (non-list)
- **THEN** `Cell top has 3 error tiles.` and then `Total DRC errors found: 3` are printed

### Requirement: drc find
`drc find [nth|text]` SHALL locate an error tile of the root cell (its own error plane only, within its bounding box, in area-enumeration order starting from the top-left), move the box to that tile's rectangle, print `Error area #<k>:` (non-list mode) and report the why-texts in that area as `drc why` does; repeated calls without an argument step to the next error, wrapping to the first after the last.
Compatibility: behavioral

- The index is a persistent counter. With an integer argument n, the counter is set to max(n,1) and that error is shown (no wrap: if there are fewer errors, `There aren't that many errors` is printed without a newline; the Tcl result is `-1` in list mode and empty otherwise, because Magic's console output replaces the result it set). Without an argument the counter is incremented before use; past the end it wraps to 1. Its initial value depends on the `magic-compat` setting (see Q-DRC-15, resolution: replicate under magic-compat, fix with it off): with it ON (the default) it starts at 1, as in Magic, so the first argument-less `drc find` of a session shows error #2 when at least two exist; with it OFF it starts at 0, so the first one shows error #1.
- With a non-integer argument, only rules whose why-text contains the argument (case-sensitive substring) are considered: errors whose area yields no matching why are skipped by advancing the counter until one matches or the search returns to its starting index.
- If the cell has no error tiles: `There are no errors in <cellname>.` (no output in list mode) and Tcl result `0` in list mode, empty otherwise.
- If the why recheck of the located area finds no (non-ignored) error, the counter advances and the next tile is tried.

(Magic ref: commands/CmdCD.c:4290-4412, drc/DRCmain.c:1056-1145)

#### Scenario: Explicit index
- **GIVEN** a top cell with 4 error tiles
- **WHEN** `drc find 3` is executed
- **THEN** the box is moved to the third error tile in enumeration order and `Error area #3:` is printed followed by its why-texts

#### Scenario: Index out of range
- **GIVEN** a top cell with 2 error tiles
- **WHEN** `drc find 5` is executed
- **THEN** `There aren't that many errors` is printed and the box does not move

#### Scenario: First find with magic-compat ON
- **GIVEN** `magic-compat` ON, a fresh session and a top cell with 4 error tiles
- **WHEN** `drc find` is executed without an argument
- **THEN** `Error area #2:` is printed and the box is moved to the second error tile (golden DRC-FIND-1)

#### Scenario: First find with magic-compat OFF
- **GIVEN** `magic-compat` OFF, a fresh session and a top cell with 4 error tiles
- **WHEN** `drc find` is executed without an argument
- **THEN** `Error area #1:` is printed and the box is moved to the first error tile

### Requirement: drc why
`drc why` SHALL recompute DRC (not read the error plane) for the root cell over the box area using the interaction check with the box as both area and erase box, and report each distinct why-text (after substitution) once, in order of first discovery, for every reported error rectangle that overlaps the box with positive area, skipping ignored rules; if none is reported it SHALL print `No errors found.`
Compatibility: exact

- Errors with why index 4 (copied from a subcell) are resolved by re-running the check, with the error rectangle as area, in every cell of the subtree under that rectangle (transformed to each cell's coordinates); the text `See error definition in the subcell` is reported only if that resolution reports no non-ignored error.
- Non-list mode prints each text on its own line. `drc list why` (Tcl) returns the list of distinct texts.
- `drc listall why` (Tcl) returns a flat list alternating why-text and a list of `{llx lly urx ury}` rectangles (root coordinates, each formatted in the current display units, integers for default units) of every reported error of that text, ignoring the ignore list; the order of texts is unspecified (hash order) and rectangles are in discovery order with duplicates kept. If nothing is reported it prints `No errors found.`
- Because `drc why` rechecks, its output can differ from the error plane after edits not yet rechecked.

(Magic ref: drc/DRCmain.c:569-760, drc/DRCmain.c:285-490, commands/CmdCD.c:4595-4608)

#### Scenario: Why under the box
- **GIVEN** a cell with a poly width violation reported as "Poly width < 0.15um" and the box covering it
- **WHEN** `drc why` is executed
- **THEN** `Poly width < 0.15um` is printed once even if several error rectangles carry it

#### Scenario: Touching box does not count
- **GIVEN** an error rectangle [10,12]×[0,10] and a box [12,20]×[0,10]
- **WHEN** `drc why` is executed
- **THEN** that error is not reported (rectangles only touch)

### Requirement: Why-text substitution
When reported, a why-text SHALL have each `%d` replaced by the rule's dist, `%c` by its cdist, and `%a` by its cdist as an area, each formatted as a length (or area) in the current display units, where the default display units print microns with a unit suffix.
Compatibility: exact

- Default units: value × (microns per internal unit) printed with `%.3f` (`%.6f` for areas), trailing zeros and a trailing decimal point removed, followed by `um` (`um^2` for areas). E.g. dist 15 with 0.01 µm per unit → `0.15um`.
- Other units (see capability `cmd-coordinates-and-units`): internal → integer with `i`/`i^2` suffix when unit printing is enabled; lambda → `%.3f` (`%.6f`) with `l`/`l^2`; user grid → value divided by grid pitch with `gx`/`gy` (`gx*gy`).
- For mask-layer rules (`drc-cif-rules`) the value is in mask units and is first converted to internal units.
- The values are the compiled (scaled, rounded) values, not the tech-file literals.
- Any other character after `%` is not a substitution: the `%` and that character are copied literally, and a trailing `%` is copied as is, in both `magic-compat` settings (Magic's output there is undefined: it may copy shifted or uninitialized bytes or end the string; see Q-DRC-16, resolution: fix). This matches Magic wherever Magic's output is defined. Each substituted value is truncated to 19 characters.

(Magic ref: drc/DRCmain.c:183-270, dbwind/DBWprocs.c:680-925)

#### Scenario: Micron substitution
- **GIVEN** a mask output scale of 0.005 µm per internal unit and a width rule with compiled dist 30 and why `"poly width < %d"`
- **WHEN** reported with default units
- **THEN** the text is `poly width < 0.15um`

#### Scenario: Unknown escape copied literally
- **GIVEN** the why-text `"m1 %d wide, 100%x done %"` for a rule with compiled dist 30 and 0.005 µm per internal unit
- **WHEN** reported with default units
- **THEN** the text is `m1 0.15um wide, 100%x done %`

### Requirement: drc ignore
`drc ignore text` SHALL add to the ignore list every why index in the scanned range whose (unsubstituted) text contains `text` as a case-sensitive substring, `drc ignore none` (case-insensitive) SHALL clear the list, and `drc ignore` SHALL print each ignored why-text on its own line (most recently added first) or `(none)`. The scanned range, also used by `drc find text`, depends on the `magic-compat` setting (see Q-DRC-17, resolution: replicate under magic-compat, fix with it off): with it ON (the default) it is 1 up to but excluding the last why index, as in Magic, so the last registered why-text can never be ignored or searched; with it OFF it is 1 up to and including the last index.
Compatibility: behavioral

Ignored rules are suppressed from `drc why`, `drc list why` and `drc find` reporting but still produce error tiles.

(Magic ref: commands/CmdCD.c:4466-4512)

#### Scenario: Ignore a rule class
- **GIVEN** why-texts `"m1 spacing"` and `"m2 spacing"` (not the last-defined text)
- **WHEN** `drc ignore spacing` is executed and `drc why` is run over violations of both
- **THEN** neither text is printed and `No errors found.` is printed

#### Scenario: Last why-text with magic-compat ON
- **GIVEN** `magic-compat` ON and a style whose last registered why-text is `"m3 spacing"`
- **WHEN** `drc ignore spacing` is executed and `drc why` is run over an m3 spacing violation
- **THEN** `m3 spacing` is still printed (golden DRC-IGN-1)

#### Scenario: Last why-text with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the style and layout of the previous scenario
- **WHEN** `drc ignore spacing` is executed and `drc why` is run
- **THEN** `No errors found.` is printed

### Requirement: drc euclidean
`drc euclidean on` SHALL enable Euclidean corner checking (see `drc-rule-semantics`), `drc euclidean <anything else>` SHALL disable it, and `drc euclidean` SHALL print `DRC measurements are Euclidean.` or `DRC measurements are Manhattan.`; the default is Manhattan.
Compatibility: exact

Changing the setting does not mark anything for rechecking.

(Magic ref: commands/CmdCD.c:4275-4289, drc/DRCcontin.c:79)

#### Scenario: Query metric
- **GIVEN** a fresh session
- **WHEN** `drc euclidean` is executed
- **THEN** `DRC measurements are Manhattan.` is printed

### Requirement: drc style
`drc style` SHALL print `The current style is "<name>".`, `drc style name` SHALL select a style as defined in `drc-rule-language`, `drc list style` SHALL return the current style name, and `drc listall style` SHALL return the list of all style names in declaration order.
Compatibility: behavioral

(Magic ref: commands/CmdCD.c:4179-4185, drc/DRCtech.c:170-280)

#### Scenario: List styles
- **GIVEN** styles `drc(full)` and `drc(fast)`
- **WHEN** `drc listall style` is executed
- **THEN** the result is `drc(full) drc(fast)` (a Tcl list: parentheses need no braces)

### Requirement: drc printrules
`drc printrules [file]` SHALL write the compiled rule table of the current style to the file (or standard output) in the format below; if the file cannot be opened it reports `Cannot write file <file>`.
Compatibility: informative

For each type i in index order, for each type j with a non-empty list: the first rule line is `printf("%-8s %-8s  ", name(i), name(j))` and later rules start with 19 spaces; each rule prints `%d x %d   %s (%s)\n` (dist, cdist, OK set, check-plane long name), then 27 spaces and the CORNER set; if any flag is set, a newline and 26 spaces followed by the words ` reverse`, ` both-corners`, ` trigger`, ` area`, ` maxwidth`, ` bends`, ` rect-size`, ` angles_45`, ` angles_90`, ` derived-edge`, ` derived-target` for the flags present; then a newline. A blank line follows each i that printed rules. `name(t)` is the first 8 characters of the short type name, or `space`; a set prints as comma-separated names, `<none>` if empty, truncated at 511 characters with `|`. Then one line `Tile type <a> can't overlap type <b>.` per illegal overlap pair recorded for plane 0, and `Types that must overlap exactly: <set>` if any.

(Magic ref: drc/DRCprint.c:82-199)

#### Scenario: Printrules header
- **GIVEN** `width poly 2 "w"`
- **WHEN** `drc printrules` is executed
- **THEN** the output contains a line starting `space    poly      2 x 2   poly (`

### Requirement: drc rulestats and statistics
`drc rulestats` SHALL print rule-database statistics and `drc statistics` SHALL print checker statistics (recent/total counts, resetting the recent counters) in the formats below.
Compatibility: informative

rulestats: `Total number of rules specifed in tech file: %d`, `Edge rules optimized away: %d`, `Edge rules left in database: %d`, `Histogram of # edges vs. rules per edge:`, eleven lines `  %2d rules/edge: %d.` for 0–10, and ` >%2d rules/edge: %d.` for overflow (histogram over type pairs sharing a plane). statistics: `Design-rule checker statistics (recent/total):` then `    Squares processed: a/b`, `    Tiles processed: a/b`, `    Edges pieces processed: a/b`, `    Constraint areas checked: a/b`, `    Multi-tile constraints: a/b`, `    Interaction areas processed: a/b`, `    Tiles processed for interactions: a/b`, `    Tiles processed for arrays: a/b`.

(Magic ref: drc/DRCtech.c:4496-4545, drc/DRCmain.c:492-560)

#### Scenario: Statistics header
- **GIVEN** any session
- **WHEN** `drc statistics` is executed
- **THEN** the first line is `Design-rule checker statistics (recent/total):`

### Requirement: drc *halo and drc *stepsize
`drc *halo [d]` SHALL set the current halo to min(d, style halo) (d parsed as a distance in the current units) or print it, and `drc *stepsize [d]` SHALL set the current step size to d or print it, with the messages below; a step size d ≤ 0 SHALL be rejected.
Compatibility: behavioral

- A `*stepsize` value ≤ 0, including a word that does not start with a number (which parses as 0), prints `Step size must be a positive integer.` (the tech-file `stepsize` message), prints nothing else and keeps the current step size, in both `magic-compat` settings (Magic stores it and then divides by zero or never ends the check, a crash or a hang; see Q-DRC-24, resolution: fix). `drc style` and `tech load` restore the style's step size.
- A grid rescale scales a `*stepsize` value as it scales the style's step size, rounding down but never below 1 (capability `drc-rule-language`, "Halo and step size"; Q-DRC-25, resolution: fix).

Messages, with DRC units = internal·N/D (integer division): set halo below maximum: `Warning: rulechecking limited to halo of <h> internal units (<u> DRC units).`; otherwise `DRC checks all rules (halo of <h> internal units, or <u> DRC units)`; query: `DRC halo is <h> internal units (<u> DRC units)` plus `Maximum rule distance is <H> internal units (<U> DRC units)` if different. Step size set: `DRC step size is now <s> internal units (<u> DRC units))`; query: `DRC step size is <s> internal units (<u> DRC units)` plus `Recommended step size is <16·H> internal units (<…> DRC units)` when different.

(Magic ref: commands/CmdCD.c:4413-4447, commands/CmdCD.c:4570-4594)

#### Scenario: Reduce halo
- **GIVEN** a style halo of 20 and no scalefactor
- **WHEN** `drc *halo 5` is executed
- **THEN** `Warning: rulechecking limited to halo of 5 internal units (5 DRC units).` is printed

#### Scenario: Step size of zero rejected
- **GIVEN** a style step size of 320, no scalefactor, and `drc *stepsize 100` executed
- **WHEN** `drc *stepsize 0`, `drc *stepsize -5` or `drc *stepsize abc` is executed, in either `magic-compat` setting
- **THEN** only `Step size must be a positive integer.` is printed
- **AND** `drc *stepsize` still reports `DRC step size is 100 internal units (100 DRC units)`

### Requirement: drc *flatcheck and drc *showint
`drc *flatcheck` SHALL run the flat check of `drc-engine` over the box and print `<n> total errors found.`; `drc *showint radius` SHALL compute the interaction area (as in `drc-engine`) of the root cell within the box for the given radius and move the box to it, printing `No interactions in this area for that radius.` when there is none and `Radius must not be negative` for a negative radius.
Compatibility: behavioral

(Magic ref: commands/CmdCD.c:4151-4178, drc/DRCsubcell.c:969-1010)

#### Scenario: No interactions
- **GIVEN** a flat cell without subcells and a box over it
- **WHEN** `drc *showint 10` is executed
- **THEN** `No interactions in this area for that radius.` is printed and the box is unchanged

### Requirement: Scripting results on iPadOS and non-Tcl hosts
Hefesto SHALL expose the list-mode results (`list`/`listall` variants of count, find, why and style) through its scripting interface (capability `scripting-api`) with the same data structure (nested lists of strings and integers) on every platform, including iPadOS, where the sandboxed embedded Tcl interpreter provides it (decision X-3), and through any non-Tcl binding of the command API.
Compatibility: behavioral

The results are Tcl objects with Magic's structure in every `magic-compat` setting. Before the Tcl binding exists (M4) the command API returns the same nested-list value, and the CLI prints it in Tcl list syntax (see Q-DRC-12, resolution: platform).

(Magic ref: commands/CmdCD.c:4197-4274, drc/DRCmain.c:646-720)

#### Scenario: listall why structure
- **GIVEN** a recheck under the box that reports exactly two errors, both with why "w", with rectangles [1,2]×[0,10] and then [6,7]×[0,10]
- **WHEN** `drc listall why` is executed
- **THEN** the result is `w {{1 0 2 10} {6 0 7 10}}` (default units)
