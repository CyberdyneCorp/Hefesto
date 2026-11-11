# Scripting API

## Purpose
Defines the scripting surface that Hefesto exposes to scripts. This is the surface that Magic exposes to Tcl when it is built as the `tclmagic` extension. It covers command binding and namespaces, command results, error signalling, name-collision handling, command tag callbacks, window-directed commands, the Tcl global variables that the layout engine reads, output and prompt routing, and the result formats that third-party scripts depend on. The most important of these scripts are the open-PDK startup files and device generators (see capabilities `scripting-pdk-integration` and `pcell-toolkit`).

Hefesto hosts the scripts in an embedded, sandboxed Tcl interpreter on every platform, including iPadOS (decision X-3; capability `hefesto-scripting`). Any script written against Magic's Tcl API MUST observe the results, side effects and variables specified here, except for operations the sandbox denies (process execution, sockets, loading native extensions; capability `hefesto-security`). Command semantics (what `box`, `paint`, `select` and so on do to the database) belong to the command capabilities. This spec covers only how those commands bind to the scripting layer and what values they return to it. Where an approved decision resolves a Magic quirk as *compat* (replicate under magic-compat, fix with it off), the requirement states both modes, and the `magic-compat` setting ON (the default) gives Magic's behavior.

PDK references have the form `(PDK ref: <repo> <path>:<line>)` and were verified against open_pdks `RTimothyEdwards/open_pdks` @ `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607` (`open_pdks` paths are relative to the repository root) and IHP-Open-PDK `IHP-GmbH/IHP-Open-PDK` @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6` (`IHP` paths are relative to `ihp-sg13g2/libs.tech/magic/`), as in capability `scripting-pdk-integration`.

## Requirements

### Requirement: Scripting host choice is abstracted behind a command API
Hefesto SHALL implement the scripting API in this capability as a host-independent command interface. A command is a name plus a vector of string arguments. It returns a status (ok or error) and a string result. Hefesto SHALL bind this interface to the embedded Tcl interpreter on every platform (capability `hefesto-scripting`). Every other caller of the command interface (the GUI, the AI agent, plugins) SHALL get identical results for identical command vectors.
Compatibility: behavioral

Magic implements this as the shared library `tclmagic` loaded into `tclsh`/`wish`/`tkcon`. The library provides the Tcl package `Tclmagic` at the Magic version (for example `8.3`). It requires the Tcl stubs API at version 8.5, or 9.0 when built against Tcl 9. The `MagicLEF`, `Exttospice`, `Exttosim` and `Plot` packages each require `Tclmagic` at the same version. (Magic ref: tcltk/tclmagic.c:57-65,1603-1673, lef/tcllef.c:44-60, ext2spice/ext2spice.c:209-226)

#### Scenario: Same command vector, same result regardless of host
- **GIVEN** a layout whose cursor box is (0,0)-(10,20) in internal units and units set to `internal`
- **WHEN** the command vector `["box","values"]` is executed through the Tcl binding
- **AND** the same vector is executed directly through the command interface
- **THEN** both SHALL return status ok with result `0 0 10 20`

### Requirement: Command registration in the magic namespace
For every command of every window client (layout, window-manager, color, netlist, 3D and the others), the system SHALL register a script command named `magic::<fullname>`. `<fullname>` is the first whitespace-delimited word of the command-table entry, for example `magic::box`, `magic::*bypass`, `magic::*flags`. Registration SHALL happen after the layout engine has finished its core initialization. Only full names SHALL be registered. Abbreviated command names SHALL NOT be callable as script commands.
Compatibility: exact

The following script commands SHALL also exist:
- `magic::initialize ?args?`: performs argument parsing and core init. On a fatal init error it sets the result to `Magic initialization encountered a fatal error.` and returns an error.
- `magic::startup`: runs the startup files and loads the initial cell. If the startup files have problems, it sets the result to `Magic encountered problems with the startup files.` but still returns ok.
- `magic::display`: returns the graphics type name, for example `NULL`, `XWIND`, `OGL`.
- `magic::tag` (see "Command tag callbacks").
- `magic::*flags flag ?value?`, where flag is one of debug, recover, silent, window, console, printf. Unique-prefix matching applies. The value is one of off|no|false|0|on|yes|true|1. With no value, it returns a boolean.

(Magic ref: tcltk/tclmagic.c:665-686,692-799,848-951,959-1058,1617-1633)

#### Scenario: Wizard command prefix
- **WHEN** a script runs `magic::*flags deb`
- **THEN** the result SHALL be `0` if debug mode is off

#### Scenario: Abbreviation is not a script command
- **WHEN** a script runs `magic::bo values`
- **THEN** the interpreter SHALL report an unknown command error, because only `magic::box` is registered

### Requirement: Global import of magic commands
After the extension loads, the startup script SHALL export every `magic::*` command and import it into the global namespace, but only if no global command of the same name already exists. In the import pattern, a literal `*` in a command name (wizard commands such as `*bypass`) SHALL be escaped so that it is not treated as a glob. Commands whose names collide with pre-existing global commands SHALL remain reachable only by their qualified names, except for the six colliding names handled by the next requirement.
Compatibility: behavioral

(Magic ref: tcltk/magic.tcl.in:23-38,331-332)

#### Scenario: Unqualified call
- **WHEN** a script runs `paint m1` after startup
- **THEN** it SHALL be equivalent to `magic::paint m1`

#### Scenario: Pre-existing global command wins
- **GIVEN** a global proc `echo` that was defined before import
- **THEN** `echo` SHALL keep calling the global proc, and Magic's command of that name, if any, SHALL be reachable only as `magic::echo`

### Requirement: Name collisions with Tcl built-ins
Before the import, the startup script SHALL rename these Tcl/Tk built-ins, skipping any that are already renamed: `clock`→`orig_clock`, `flush`→`tcl_flush`, `load`→`tcl_load`, `array`→`tcl_array`, `label`→`tcl_label` (only if Tk defines it), `grid`→`tcl_grid` (only if Tk defines it). The unqualified Magic commands `clockwise`, `flush`, `load`, `label`, `array` and `grid` SHALL then dispatch as follows:
1. Strip a single leading `::` from the invoked name, then match it against the list (clockwise, flush, load, label, array, grid) by case-sensitive unique prefix. If there is no match, go to step 4.
2. Invoke the renamed built-in (orig_clock, tcl_flush, tcl_load, tcl_label, tcl_array, tcl_grid) with the same arguments. If it returns ok, return its result. The Magic command SHALL NOT run.
3. If the built-in failed and the name is `load`, the first argument exists, and its final path component (after the last `/`) contains a `.` whose suffix after the last `.` is neither `mag` nor `gz`, then return the built-in's error.
4. Otherwise reset the result and run the Magic command.

A name qualified as `magic::load` (and so on) SHALL go directly to the Magic command.
Compatibility: exact

(Magic ref: tcltk/magic.tcl.in:324-329, tcltk/tclmagic.c:390-499)

#### Scenario: Tcl array semantics preserved
- **WHEN** a script runs `array set a {x 1}`
- **THEN** the Tcl built-in SHALL handle it and no Magic `array` command SHALL run

#### Scenario: Magic array fallback
- **GIVEN** a selected cell instance arrayed 1×4
- **WHEN** a script runs `array -list count`
- **THEN** `tcl_array -list count` SHALL fail, the Magic `array` command SHALL run, and the result SHALL be `0 0 0 3` (xlo xhi ylo yhi)

#### Scenario: Shared library load
- **WHEN** a script runs `load /opt/x/libfoo.so`
- **THEN** the Tcl built-in result SHALL be returned, and the Magic `load` SHALL NOT run
- **AND** in Hefesto that result SHALL be the sandbox error, because loading native extensions is denied (capability `hefesto-scripting`)

#### Scenario: Cell load with dotted directory
- **WHEN** a script runs `load ../lib.v2/inv` and the built-in fails
- **THEN** because the final component `inv` has no dot, the Magic `load` SHALL load cell `inv`

#### Scenario: Compressed cell
- **WHEN** a script runs `load cells/inv.mag.gz`
- **THEN** because the suffix is `gz`, the Magic `load` SHALL run after the built-in fails

### Requirement: Unknown-command fallback for renamed built-ins
When the startup script is not running under the tkcon console, it SHALL rename `unknown` to `tcl_unknown` and install an `unknown` handler. For an unknown command `cmd`, if a command `tcl_<cmd>` exists, the handler SHALL evaluate `tcl_<cmd>` with the remaining arguments and return its result, or re-raise its error with the same error code. Otherwise it SHALL delegate to `tcl_unknown`. Under tkcon, the tkcon commands `edit`, `dump` and `what` SHALL be renamed to `orig_edit`, `orig_dump` and `orig_what` so that the Magic commands of those names are imported.
Compatibility: informative

(Magic ref: tcltk/magic.tcl.in:289-316)

#### Scenario: Renamed cd is still reachable
- **WHEN** no `foo` command exists and `tcl_foo` exists
- **THEN** calling `foo 1 2` SHALL behave as `tcl_foo 1 2`

### Requirement: Option keyword matching
Inside every Magic command, option keywords (for example `cellname list top`, `cif scale out`, `tech layer`, `tech lock`) SHALL be matched against the command's option table as follows:
- An entry's significant part is the text up to its first space character. Whether a tab also ends it depends on the `magic-compat` setting (Q-SCRIPT-14, resolution: compat): with it ON (the default) a tab does NOT end it, as in Magic; with it OFF any whitespace ends it.
- Matching is case-insensitive.
- An exact match of the significant part wins immediately.
- Otherwise a unique prefix matches.
- A prefix of two or more entries is ambiguous.
- No match is an error.
- A leading `::magic::` or `magic::` on the keyword is ignored.

Scripts rely on this, for example `cellname list top` selects `topcells`, `cif scale out` selects `output`, `tech layer "*"` selects `layers`, `tech lock X` selects `locked`, and `tech unlock X` selects `unlocked`.

Only two entries of Magic's tables have a tab after the keyword and a prefix sibling: the layout-window aliases `get` (of `getcell`) and `id` (of `identify`). With `magic-compat` ON both are ambiguous (`get` is a prefix of `get`, `getcell` and `getnode`; `id` of `id` and `identify`), so `get` and `id` always fail as ambiguous command names, and `magic::get` and `magic::id` always return error status. With it OFF they are exact matches and run `getcell` and `identify`. Every other tab-terminated keyword behaves the same in both settings.
Compatibility: exact

(Magic ref: utils/lookup.c:58-125, commands/CmdCD.c:1139-1166, commands/CmdTZ.c:184-196)

#### Scenario: Prefix selection
- **WHEN** a script runs `cellname list top`
- **THEN** the option `topcells` SHALL be selected and the result SHALL be the list of top-level cell names

#### Scenario: Case-insensitive keyword
- **WHEN** a script runs `units MICRONS`
- **THEN** units SHALL be set to microns

#### Scenario: Tab-terminated alias with magic-compat ON
- **GIVEN** `magic-compat` ON and one selected instance `x1`
- **WHEN** a script runs `magic::id x2`
- **THEN** the command SHALL return error status as an ambiguous command name and `x1` SHALL keep its name

#### Scenario: Tab-terminated alias with magic-compat OFF
- **GIVEN** `magic-compat` OFF and one selected instance `x1`
- **WHEN** a script runs `magic::id x2`
- **THEN** it SHALL run as `identify x2` and the instance SHALL be named `x2`

### Requirement: Command status and error signalling
A Magic command invoked from a script SHALL return error status only when dispatch itself fails:
- the command is not valid for the window client that receives it;
- the command name is ambiguous;
- an input grab is in effect.

Magic's Tcl build also fails dispatch for more than 200 arguments (`Error: number of command arguments exceeds 200!`) and for arguments that, joined with separators, reach 2048 characters (`Error: command length exceeds 2048 characters!`). Hefesto has no such limits in any mode, so these are not dispatch failures (capability `ui-command-dispatch`, requirement "Command line splitting and quoting"; Q-UI-10, resolution: fix).

When dispatch fails through the script namespace, the diagnostic messages SHALL be suppressed (quiet dispatch), and the error result SHALL be empty unless a tag callback sets it.

A command that runs but rejects its arguments or fails internally (usage errors, missing cells, unreadable files) SHALL print its diagnostic on the error stream and SHALL return ok status. Its result is whatever it set, often empty. Two documented exceptions return ok status but set an explanatory result string: `tech load` with an unreadable file (result `Technology file does not exist or is not readable\n`) and `tech load` with a parse failure (result `Error in loading technology file\n`). Commands invoked through a window path (see "Window-directed commands") SHALL NOT return error status for dispatch failures. They print the diagnostic instead.

Scripts MUST therefore detect failures by inspecting results, not by catching errors. Hefesto SHALL reproduce this ok-status convention by default. A script can opt in to script strict mode, in which such failures raise Tcl errors, through the project setting `script-strict` or the per-interpreter command `hefesto::strict on|off` (capability `hefesto-diagnostics`, Requirement "Script strict mode switch"). See Q-SCRIPT-2.
Compatibility: exact

(Magic ref: tcltk/tclmagic.c:496-528,538-578, textio/txCommands.c:1491-1575, textio/txcommands.h:38-39, windows/windSend.c:86-287, commands/CmdTZ.c:430-450)

#### Scenario: Bad usage is not a Tcl error
- **WHEN** a script runs `catch {box frobnicate} err`
- **THEN** catch SHALL return 0, and a usage message SHALL appear on the error stream

#### Scenario: Missing cell on load -fail
- **WHEN** a script runs `load nosuchcell -fail -silent`
- **THEN** status SHALL be ok, and the cell SHALL NOT be created (see `pcell-toolkit` netlist import, which relies on this)

#### Scenario: Command for another window type
- **GIVEN** only a layout window exists
- **WHEN** a script runs `magic::shownet` (a netlist-window command)
- **THEN** status SHALL be error with an empty message

### Requirement: Post-command bookkeeping
After a command dispatched from a script, the system SHALL:
- increment the command sequence number, except for `*bypass`;
- record the command in the command log if logging is enabled;
- refresh displays if dispatch succeeded;
- interrupt any background DRC pass in progress, except for `*bypass` and `windownames`;
- reschedule background DRC to run when idle.

Background DRC SHALL be held off for the duration of the command. `*bypass <command>` SHALL run `<command>` without resetting a pending interactive command-line redirection and without advancing the command number. Its purpose is GUI callbacks that must not disturb selection-cycling state.
Compatibility: behavioral

(Magic ref: textio/txCommands.c:1534-1575, windows/windCmdAM.c:354-395)

#### Scenario: Bypass keeps the command count
- **GIVEN** the command number is N
- **WHEN** `*bypass box values` runs
- **THEN** the command number SHALL still be N afterwards

### Requirement: Command tag callbacks
The system SHALL maintain a table that maps a bare command name to a script (a "tag"):
- `magic::tag <name>` SHALL return the current tag script for `<name>`, or the empty string. A query MAY create an empty entry.
- `magic::tag <name> <script>` and `magic::tag replace <name> <script>` SHALL set the tag. An empty `<script>` removes it.
- `magic::tag add <name> <script>` SHALL append to the existing tag as `<old> ; <new>`. If there is no existing tag, it acts as replace. Appending an empty script is a no-op.
- `add`/`replace` SHALL be matched as Tcl index keywords (unique prefix, exact case).
- Any argument count other than 2, 3, or 4 (with add/replace) SHALL return an error with no message.

When looking up a command's tag, a leading `::` and then a leading `magic::` SHALL be stripped from the invoked name.
Compatibility: exact

(Magic ref: tcltk/tclmagic.c:85-101,300-383)

#### Scenario: Appending tags
- **GIVEN** `tag select` is `magic::gencell_update %1`
- **WHEN** `magic::tag add select "magic::mgrselect %r"` runs
- **THEN** `magic::tag select` SHALL return `magic::gencell_update %1 ; magic::mgrselect %r`

#### Scenario: Removing a tag
- **WHEN** `magic::tag load {}` runs
- **THEN** `magic::tag load` SHALL return the empty string and no callback SHALL run after `load`

### Requirement: Tag callback execution and substitution
After a script-dispatched command completes with ok status, the system SHALL evaluate its tag script, if any. Before evaluation, the `%` escapes in the script SHALL be substituted in one left-to-right pass. Substituted text SHALL NOT be rescanned. Each substituted value SHALL be inserted as one list element, quoted as the script language's `list` command quotes it, so a value containing spaces, `$`, `[`, `]`, `"` or `\` stays one word and is not evaluated:
- `%W`: the Tk path of the window the command applied to. If unknown and exactly one on-screen layout window exists, that window's path. Otherwise the empty string, inserted as `{}`.
- `%r`: the command's current result.
- `%R`: like `%r`, and in addition the tag script's own result SHALL replace the command's result.
- `%0`..`%5`: argument N of the command, where `%0` is the command name. If N is not less than the argument count, the empty string, inserted as `{}`.
- `%%`: a single `%`.
- Any other `%x`, and a `%` at the end of the script: copied unchanged.

This is Q-SCRIPT-3, resolution: fix in both `magic-compat` settings. Magic inserts `%r` as `"<result>"` with no escaping and the arguments and window path raw, restarts scanning at the beginning of the string after each replacement, and loops forever (the editor hangs) on `%%` or on any other `%x`. For every value without `"`, `$`, `[`, `]`, `\` or `%`, Hefesto inserts the word Magic's substitution produces, so a tag script that works in Magic keeps its meaning.

The command's result SHALL be preserved across a successful tag evaluation unless `%R` was used. If the tag script raises an error, the command SHALL return error status with the tag's error message. The command sequence number SHALL be restored after the tag runs. Commands dispatched through a window path SHALL substitute `%W` with that path and SHALL number `%0` from the command name, excluding the path. See Q-SCRIPT-3 for the unescaped `%r` quoting and for rescanning.
Compatibility: exact

(Magic ref: tcltk/tclmagic.c:107-294,528,576-577)

#### Scenario: Argument substitution with placeholder
- **GIVEN** `tag box` is `magic::boxview %W %1`
- **AND** one layout window `.layout1.pane.top.magic` exists
- **WHEN** the script runs `box`
- **THEN** the evaluated tag SHALL be `magic::boxview .layout1.pane.top.magic {}`

#### Scenario: Result passthrough
- **GIVEN** `tag cellname` is `magic::mgrupdate %W %1`
- **WHEN** `cellname list top` returns `{inv nand2}`
- **THEN** after the tag runs, the result seen by the caller SHALL still be `inv nand2`

#### Scenario: Result with spaces stays one word
- **GIVEN** `tag cellname` is `set ::r %r`
- **WHEN** `cellname list top` returns `inv nand2`
- **THEN** `::r` SHALL be `inv nand2`

#### Scenario: Literal percent in a tag script
- **GIVEN** `tag box` is `set ::p 50%%`
- **WHEN** a script runs `box`
- **THEN** `::p` SHALL be `50%` and the command SHALL complete (Magic hangs; golden deviation)

#### Scenario: Tag error propagates
- **GIVEN** `tag paint` is `error boom`
- **WHEN** `paint m1` runs successfully
- **THEN** the paint SHALL remain applied and the script SHALL receive error `boom`

### Requirement: Window-directed commands
Every layout or special window SHALL be registered as a script command named by its Tk path, for example `.layout1.pane.top.magic`. `<path> <command> ?args?` SHALL execute `<command>` in the context of that window. If no pointer position is recorded for that window, the reference point SHALL be (20,20) in window pixel coordinates. The window manager's close request on that path SHALL invoke `magic::closewindow <path>`. Window-path commands accept any command valid for the window's client, including abbreviations.
Compatibility: behavioral

(Magic ref: tcltk/tclmagic.c:538-600, doc/html/tk_path_name.html)

#### Scenario: Directed box query
- **WHEN** a script runs `.layout1.pane.top.magic box values`
- **THEN** the result SHALL be the box values with respect to that window's root cell

### Requirement: Output, flushing and escaping
All engine text output SHALL be emitted as a script-level `puts -nonewline stdout "<text>"` or `puts -nonewline stderr "<text>"` call, so that scripts or consoles that redefine `puts` capture it. Before emission, the characters `"`, `[`, `]`, `\` and `$` in the text SHALL be backslash-escaped, so the text prints literally whether or not a script variable named after a `$` exists (Q-SCRIPT-4, resolution: fix in both `magic-compat` settings; Magic leaves `$<name>` unescaped when a variable `<name>` exists in the printing interpreter, so the variable's value is printed). Output is evaluated in the console interpreter only when the `printf` runtime flag is set. Flushing SHALL call the original Tcl `flush` (`::tcl_flush stdout|stderr`), not the Magic `flush` command, and SHALL preserve the interpreter result.
Compatibility: informative

(Magic ref: tcltk/tclmagic.c:1335-1490,1504-1550)

#### Scenario: Dollar in a cell name
- **GIVEN** no Tcl variable `abc` exists
- **WHEN** the engine prints `cell a$abc`
- **THEN** the text `cell a$abc` SHALL appear unchanged on stdout

#### Scenario: Dollar naming an existing variable
- **GIVEN** `set VDD VPWR`
- **WHEN** the engine prints `net $VDD`
- **THEN** the text `net $VDD` SHALL appear unchanged on stdout

### Requirement: Prompts and dialogs through the script layer
When the engine needs a line of user input:
- with the console: it SHALL evaluate `magic::dialog "" "<prefix>"`, or `magic::dialog` with no prefix;
- without the console: it SHALL print the prefix, flush, and evaluate `gets stdin`.

The returned string, with one trailing newline removed, is the answer. An empty answer means "no input". Answers longer than the buffer are truncated to max-1 characters.

For multiple-choice questions, it SHALL evaluate `tk_dialog .dialog "Dialog" "<escaped prompt>" {} <default> "<choice1>" ...` and use the integer result, or -1 if the result is not an integer.

In batch mode (no console), `magic::dialog`, `magic::suspendout`, `magic::resumeout`, `magic::consolegeometry` and `magic::consolefocus` SHALL be defined as no-ops.
Compatibility: behavioral

(Magic ref: tcltk/tclmagic.c:1064-1102,1230-1283, tcltk/magic.tcl.in:65-71, tcltk/console.tcl:11-46)

#### Scenario: Batch mode confirmation
- **GIVEN** magic runs with `-dnull -noconsole` and stdin is a pipe whose next line is `yes`
- **WHEN** a command asks for a line of input
- **THEN** the answer SHALL be `yes`

### Requirement: Terminal input queue without console
When running without the console, the system SHALL replace the standard input channel with one that serves bytes queued by the layout window first (keystrokes typed after the `:` macro) and only then reads the terminal. Bytes already buffered by the original channel at the time of replacement SHALL be preserved ahead of any queued input. Read interruptions (EINTR) SHALL be retried. Escape in a layout window during `:` entry SHALL cancel the partial command:
- With the console, the pending line is deleted.
- Without the console, the incomplete last line is removed from the queue (complete lines are kept), the typed characters are erased with backspaces, and the prompt is restored to `% `.
Compatibility: informative

(Magic ref: tcltk/tclmagic.c:984-1055,1180-1224,1554-1599)

#### Scenario: Piped startup commands are not lost
- **WHEN** `printf 'box 0 0 1 1\nquit -noprompt\n' | magic -dnull -noconsole` runs
- **THEN** both commands SHALL execute

### Requirement: Directory change rebases cell paths
The script-level `cd` SHALL be replaced by a version that calls the original `cd` (renamed `tcl_cd`) and, on success, rebases the stored file paths of loaded cells that were relative to the previous working directory, so that they remain valid. In Hefesto `cd` is allowed only within the sandbox's granted roots (capability `hefesto-scripting`, decision D-33); outside them it fails and nothing is rebased.
Compatibility: behavioral

(Magic ref: tcltk/tclmagic.c:817-842,1635-1644)

#### Scenario: Relative cell path survives cd
- **GIVEN** a project in `/w`, and cell `inv` was loaded from `./lib/inv.mag` with the working directory `/w`
- **WHEN** the script runs `cd /w/sub` (a project subfolder) and then `save inv`
- **THEN** the file `/w/lib/inv.mag` SHALL be written

### Requirement: Script variables read by the engine
The engine SHALL read these global script variables at the moment of use:

| Variable | Use |
|---|---|
| `CAD_ROOT` | Install root. If unset at load time, it is set from the environment variable `CAD_ROOT`, or else the compiled default. |
| `VDD`, `GND` | Power and ground net names: LEF/DEF power nets, ground recognition in substrate-compatibility extraction, CDL global list when the value ends in `!`. |
| `SUB` | Substrate global for CDL output when the value ends in `!`. |
| `globals(<name>)` | A node name is global iff this array element exists. In Hefesto, `globals`, `VDD`, `GND` and `SUB` read and write the project setting "global nets and power names" (capability `hefesto-settings`, decision D-14). |
| `<name>` | A tech-file substrate or default node written `$<name>` (single `$`) is replaced by the value of `<name>`. If `<name>` is undefined, the `$` is stripped. `$$` is left alone. |
| `PDK_ROOT`, `PDKROOT`, `PDK_PATH`, `PDKPATH` | Technology auto-discovery after the corresponding environment variables (see `scripting-pdk-integration`), and shortening of saved cell paths to `$<VAR>/...`. |
| `Opts(tool)` | Name of the active macro/button table. |
| `batch_mode` | If defined at init, interrupt handling is set up for batch operation. |

In addition, `$NAME` and `${NAME}` in search paths and file names SHALL be expanded from the script variable of that name, and from `CAD_ROOT`'s compiled default as a last resort. Whether a later change of the variable is seen depends on the `magic-compat` setting (Q-SCRIPT-5, resolution: compat): with it ON (the default) the first resolved value of each name is cached for the session, as in Magic, and a name that does not resolve is retried on its next use; with it OFF each expansion reads the current value from the path-variable table (Q-CELL-17). In both settings, when an expansion finds that a name's current value differs from its first resolved value, Hefesto SHALL warn once per name. Unresolvable names are left unexpanded.
Compatibility: behavioral

(Magic ref: tcltk/tclmagic.c:1658-1669, utils/path.c:268-330, database/DBio.c:600-700,2585-2610,6065-6090, extract/ExtBasic.c:900-916,2655-2665, ext2spice/ext2spice.c:950-1050, extflat/EFname.c:95-150, lef/lefWrite.c:1085-1100, lef/defWrite.c:320-345, dbwind/DBWbuttons.c:265-277, utils/main.c:612-617, tcltk/magicdnull.c:39-43)

#### Scenario: Substrate variable substitution
- **GIVEN** the extract style declares substrate node `$SUB` and the script has run `set SUB VSUBS`
- **WHEN** extraction names the substrate node
- **THEN** the name SHALL be `VSUBS`

#### Scenario: Undefined substrate variable
- **GIVEN** the substrate node is declared `$SUB` and `SUB` is undefined
- **THEN** the name SHALL be `SUB`

#### Scenario: Path variable from script
- **GIVEN** `set PDKPATH /pdk/sky130A`
- **WHEN** `addpath $PDKPATH/libs.ref/mag/sky130_fd_sc_hd` runs
- **THEN** the search path SHALL contain `/pdk/sky130A/libs.ref/mag/sky130_fd_sc_hd`

#### Scenario: Changed path variable with magic-compat ON
- **GIVEN** `magic-compat` ON, `set LIB /a`, and a cell has been read whose `use` line names the file `$LIB/c`
- **WHEN** `set LIB /b` runs and a cell is read whose `use` line names `$LIB/d`
- **THEN** the child SHALL be read from `/a/d`, and a warning naming `LIB` SHALL be printed once

#### Scenario: Changed path variable with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the session of the previous scenario
- **WHEN** `set LIB /b` runs and a cell is read whose `use` line names `$LIB/d`
- **THEN** the child SHALL be read from `/b/d`, and a warning naming `LIB` SHALL be printed once

### Requirement: Numeric value formatting in results
Every result value that is a layout distance or coordinate SHALL be formatted according to the current `units` setting, as follows:
- **default** (initial state): a decimal integer of internal units (`%d`).
- **internal**: `%d`.
- **lambda**: `v × λnum/λden` computed in IEEE single precision, printed `%0.3f`.
- **microns**: `v × s` in single precision, where s is the CIF output scale in µm per internal unit, printed `%0.3f`.
- **user/grid**: `v / g` in single precision, where g is the grid pitch in that axis, printed `%0.3f`.

For the fixed-point formats, trailing zeros and then a trailing `.` SHALL be removed. When the units print flag is on, the suffix `i`, `l`, `um`, `gx` or `gy` SHALL be appended. Squared values use `%0.6f` and the suffixes `i^2`, `l^2`, `um^2`, `gx*gy`.

`units` with no argument or with `list` SHALL return `default`, or a two-element list `<type> print|noprint` where type ∈ {internal, lambda, user, microns}. `units <type> ?print|noprint?` SHALL set the type. Without a second argument, the previous print flag is kept, except when coming from `default`, where printing is off. `units default` restores the startup behavior. The idiom `set u [units]; units internal; ...; units {*}$u` SHALL restore the prior state exactly.
Compatibility: exact

(Magic ref: dbwind/DBWprocs.c:680-686,736-907, commands/CmdTZ.c:873-990)

#### Scenario: Microns formatting
- **GIVEN** the CIF output scale is 0.005 µm per internal unit, units are `microns noprint`, and the box is (0,0)-(30,100)
- **WHEN** `box values` runs
- **THEN** the result SHALL be `0 0 0.15 0.5`

#### Scenario: Units round trip
- **GIVEN** units are `lambda print`
- **WHEN** a script runs `set u [units]` and then `units internal` and then `units {*}$u`
- **THEN** `units` SHALL return `lambda print`

#### Scenario: Default units
- **GIVEN** a fresh session
- **WHEN** `units` runs
- **THEN** the result SHALL be `default`

### Requirement: Result formats of commands used by scripts
The following commands SHALL return these results. "dist" means a value formatted per "Numeric value formatting in results". "List" means a script list.

- `box values` → `{llx lly urx ury}` (dist). `box position` → `{llx lly}`. `box size` → `{w h}`. `box width` / `box height` → dist. `box exists` → boolean `1`/`0`. All other box forms return empty.
- `units` → see above. `snap` (no argument) → `internal`, `lambda` or `user`. PDK generators save and restore it with `set s [snap] ; … ; snap $s`.
- `cellname list exists|self <c>` → `<c>` if loaded, `0` if not. A loaded cell with no name gives `1`. With no `<c>`: the selected child cells' names, or the edit root.
- `cellname list children|parents|instances <c>` → list of names, empty if none.
- `cellname list topcells|allcells|modified` → list. `cellname list window` → the window's root cell name.
- `cellname list flags <c>` → list of flag words (for example `not-found`).
- `cellname list property <c> <key>` → value, or empty if undefined (no error message).
- `cellname list writeable <c>` → `writeable`|`read-only`. `cellname list filepath <c>` → directory, or `default`.
- `instance list celldef|exists|self <i>` → name, or empty if `<i>` is not found in the edit cell. A name containing `/` is resolved hierarchically.
- `instance list children <i>` → list. `instance list orientation` → one of `0 90 180 270 0h 90h 180h 270h`, or `N E S W FN FE FS FW` with `-def`. `instance list abutment` → `{llx lly urx ury}` (dist; from FIXED_BBOX if present, else the bounding box, transformed to the parent).
- `what -list` → three lists: `{paint-types} {labels} {cells}`. Each label is `{text type cellname-or-{}}`. Each cell is `{use-id def-name}`.
- `array -list count` → `{xlo xhi ylo yhi}` (integers) per use. `array -list pitch` → `{xsep ysep}` (dist). `array -list position` → `{x y}` (dist). With `magic-compat` ON the result is the flat list of the last use reported, as in Magic; with it OFF it is a list with one such sublist per selected array use (capability `cmd-selection`, requirement "Array command"; Q-CMDEDIT-63).
- `getcell <name> ...` → the new instance id.
- `goto <label>` → the long name of the tile type under the label, or empty if not found.
- `port first` → lowest port index among ports, or -1. `port last` → depends on the `magic-compat` setting (Q-SCRIPT-6, resolution: compat): with it ON (the default), the highest port index stored on any label of the edit cell, ports or not, as in Magic (a label that never was a port has index 0, and a removed port keeps its index until the cell is reloaded); with it OFF, the highest index among the current ports, or -1. `port <ref> next` → the next higher port index, or -1. `port <ref> name` → label text. `port <ref> index` → integer. `port <ref> exists` → boolean. `port <ref> class|use|shape` → keyword. A trailing `-quiet` suppresses "not found" diagnostics and yields an empty result.
- `property <key>` → value, or empty with an error message if undefined. `property` → all properties. Values: string as-is, integers as an int or list, dimensions as dist (list), doubles as wide integers.
- `tech name` → name. `tech version` → the version string followed by the description appended as one list element. `tech lambda` → `{num den}` integers. `tech layers *` → a space-separated list of type names: for each type, its longest non-alias name, tech-dependent types first. `tech locked` / `tech unlocked` → lists.
- `cif scale output|input` → a double. With `magic-compat` ON (the default) it is the single-precision quotient 10·scalefactor/(expander·1000) widened to double, as in Magic; with it OFF it is the same quotient computed in double precision (Q-SCRIPT-7, resolution: compat). The PDK helpers round the value to four decimals, so PDK geometry and generated cell names are the same in both settings. `cif list istyle|ostyle` → current style. `cif listall istyle|ostyle` → list.
- `extract list|listall style`, `drc list|listall style` → current style / list.
- `drc status` → boolean. `drc list count` → list of `{cell count}`. `drc list count total` → integer. `drc listall why` → flat list `{rule {{llx lly urx ury} ...} rule ...}`. The rules are in the iteration order of a Magic string hash table (capability `hefesto-output-determinism`, requirement "Magic string hash table"; requested size 16) keyed by the substituted why text and filled in the order the errors are found, and each rule's rectangles are in the order found (Q-SCRIPT-8, resolution: replicate; decision X-1).
- `macro list ?-reverse?` → flat list `key text ...` (`text key ...` with `-reverse`). `macro list <key>` → that macro's text.
- `windownames <client>` → list of window paths. `windownames` → a single path if exactly one layout window, else a list.
- `windowcaption` → the caption string.
- `view get` / `view bbox` → `{llx lly urx ury}`.
- `cursor ?units?` → `{x y}`.
- `path search|cell|sys` → the path string.

Commands not listed SHALL return an empty result unless their own capability specifies otherwise.
Compatibility: exact

(Magic ref: commands/CmdAB.c:200-500,760-790,920-1070, commands/CmdCD.c:1080-1500,4180-4265,4530-4540,1930-1956, database/DBcellname.c:455-660,895-945,1135-1230,1480-1530,1640-1690, commands/CmdTZ.c:168-300,1196-1460,1634-1651, commands/CmdLQ.c:1523-1830,2324-2525, commands/CmdFI.c:1067-1130,1839-1890, database/DBtechname.c:433-520, cif/CIFmain.c:139-146,200-225, drc/DRCmain.c:403-461,646-700, windows/windCmdAM.c:151-175,1180-1330, windows/windCmdSZ.c:481-560,798-870)

#### Scenario: Existence test idiom
- **GIVEN** cell `inv` is loaded and `nand2` is not
- **THEN** `cellname list exists inv` SHALL return `inv` and `cellname list exists nand2` SHALL return `0`

#### Scenario: Instance lookup failure
- **WHEN** `instance list celldef nosuch` runs
- **THEN** the result SHALL be the empty string with no message

#### Scenario: what -list structure
- **GIVEN** the selection contains m1 paint, label `A` on m1 in the edit cell, and instance `x1` of `inv`
- **WHEN** `what -list` runs
- **THEN** the result SHALL be `metal1 {{A metal1 {}}} {{x1 inv}}` (verify against Magic)

#### Scenario: port last with magic-compat ON
- **GIVEN** `magic-compat` ON and a cell with a single non-port label and no ports
- **THEN** `port first` SHALL return `-1` and `port last` SHALL return `0`

#### Scenario: port last with magic-compat OFF
- **GIVEN** `magic-compat` OFF and a cell with a single non-port label and no ports
- **THEN** `port first` SHALL return `-1` and `port last` SHALL return `-1`

#### Scenario: cif scale with magic-compat ON
- **GIVEN** `magic-compat` ON and a cifoutput style with 0.01 µm per internal unit
- **WHEN** `cif scale out` runs
- **THEN** the result SHALL be `0.009999999776482582`

#### Scenario: cif scale with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the same style
- **WHEN** `cif scale out` runs
- **THEN** the result SHALL be `0.01`

### Requirement: Generic Tcl helpers defined at startup
The startup script SHALL define these globally:
- `echo args`: equivalent to `puts stdout $args`.
- `irsim`, `xcircuit`, `netgen`: in Magic each sources `$CAD_ROOT/irsim/tcl/irsim.tcl`, the highest-sorted `$CAD_ROOT/xcircuit*/xcircuit.tcl`, or `$CAD_ROOT/netgen/tcl/netgen.tcl` respectively, and prints a multi-line stderr help when the script is missing. Those packages load native libraries, which the sandbox denies, so Hefesto defines them as follows (decision D-34): `netgen` prints a diagnostic that names Hefesto's built-in `lvs` command (capability `lvs-command`) and does not emulate netgen; `irsim` behaves as capability `irsim-interface` specifies (desktop only); `xcircuit` prints a "not available" diagnostic. None of them sources a file.
- `magic::drcstate {option}`: a no-op, redefined by the GUI.
- `UsingIRSIM`, `UsingXCircuit`, `UsingNetgen`: set to 1 when the namespaces `::irsim`, `::xcircuit` or `::netgen` already exist.

It SHALL set `auto_noexec 1`, so that unknown commands are never executed as shell programs.
Compatibility: informative

(Magic ref: tcltk/magic.tcl.in:57-156,285)

#### Scenario: No implicit exec
- **WHEN** a script calls the unknown command `ls`
- **THEN** it SHALL fail as an unknown command and SHALL NOT run `/bin/ls`

### Requirement: Script-defined procedures in the engine namespace
Scripts SHALL be able to create procedures and variables inside the engine namespace `magic`, either with `proc magic::<name>` or with `namespace eval magic`. Such definitions SHALL be callable as `magic::<name>`. They are not callable as a bare `<name>`, because the global import (see "Global import of magic commands") runs before any startup file and imports only the commands that exist at that moment (verify against Magic). The PDKs always use the qualified names. The editor SHALL NOT define engine commands or framework procedures with the names that the verified PDKs define there: `u2l`, `l2u`, `i2u`, `u2i`, `float2spice`, `spice2float`, `3digitpastdecimal`, `searchcellondisk`, `iscellnameunique`, `query_mylib_ip`, `query_my_projects`, `magceil`, `magfloor`. A PDK redefinition of a framework procedure SHALL replace it for the rest of the session, as a Tcl `proc` does. Inside one interpreter a later definition replaces an earlier one, as in Tcl. Each open project has its own interpreter (capability `hefesto-scripting`, Requirement "One interpreter per open project"), so two projects never share these definitions. When a script sourced from one PDK's directory redefines a `magic::` procedure that a script of a different PDK defined in the same interpreter, Hefesto SHALL warn, naming the procedure and both PDKs, and the new definition SHALL take effect (Q-SCRIPT-41, resolution: replicate Tcl semantics, one interpreter per open project).

Writes to undeclared namespace variables, such as `set magic::ctot_val 1`, SHALL create the variable.
Compatibility: behavioral

(Magic ref: tcltk/tclmagic.c:1603-1673, tcltk/toolkit.tcl:1285-1306) (PDK ref: open_pdks common/pdk.tcl:12-271, IHP ihp-sg13g2-util.tcl:668-835)

#### Scenario: PDK helper callable
- **GIVEN** the sky130A device script (with `common/pdk.tcl` appended) has been sourced
- **WHEN** a script evaluates `magic::3digitpastdecimal 0.12345`
- **THEN** the result SHALL be `0.123`

#### Scenario: Cross-PDK helper redefinition
- **GIVEN** the sky130A device script has been sourced into a project's interpreter
- **WHEN** the sg13g2 helper file `ihp-sg13g2-util.tcl` is sourced into the same interpreter
- **THEN** `magic::i2u 34` SHALL return the sg13g2 result, and a warning naming `magic::i2u`, sky130A and sg13g2 SHALL be printed

#### Scenario: Helper name not reserved
- **GIVEN** a fresh session with no PDK loaded
- **WHEN** `info commands magic::spice2float` is evaluated
- **THEN** the result SHALL be empty
