# Tcl Integration

## Purpose
In the default build, Magic is a Tcl extension (`tclmagic`) loaded by `magic.tcl`. Every Magic command becomes a Tcl command in the `magic::` namespace. Output, prompts and command callbacks are routed through Tcl. The optional Tk "wrapper" GUI adds a toolbar, menus and managers for cells, libraries, DRC and text.

## Requirements

### Requirement: Tcl package and command registration
`Tclmagic_Init` SHALL require Tcl stubs version 8.5, or 9.0 when built against Tcl 9. It SHALL provide the package `Tclmagic` at the Magic major.minor version. It SHALL register every command of every window client as `magic::<command>`. (src: tcltk/tclmagic.c:57-65,755-770,1604-1673)

#### Scenario: Calling a magic command from Tcl
- **WHEN** a Tcl script runs `magic::box 0 0 10 10`
- **THEN** the layout `box` command SHALL run with those arguments

#### Scenario: Global import of commands
- **WHEN** `magic.tcl` finishes loading
- **THEN** each `magic::x` that has no global Tcl command of the same name SHALL be callable as `x` (src: tcltk/magic.tcl.in:23-38,331-332)

### Requirement: Command name collision handling
`magic.tcl` SHALL rename the Tcl built-ins `clock`, `flush`, `load`, `array`, `label` and `grid` to `orig_clock`, `tcl_flush`, `tcl_load`, `tcl_array`, `tcl_label` and `tcl_grid`.

When one of these names is called, dispatch SHALL first try the Tcl built-in and SHALL fall back to the Magic command if the built-in fails. For `load` with a file extension other than `.mag`/`.gz`, the Tcl error SHALL be returned instead. (src: tcltk/magic.tcl.in:324-329, tcltk/tclmagic.c:407-492)

#### Scenario: load of a Tcl shared library
- **WHEN** `load /path/lib.so` is executed
- **THEN** the Tcl built-in `load` SHALL handle it, and the Magic cell `load` SHALL NOT run

#### Scenario: load of a layout cell
- **WHEN** `load mycell` is executed
- **THEN** the Tcl built-in SHALL fail, and the Magic `load` command SHALL load cell `mycell`

### Requirement: Command tag callbacks
`magic::tag <cmd> [script]` and `magic::tag add|replace <cmd> <script>` SHALL attach a Tcl script that runs after the named command. `add` SHALL append the script with " ; ". An empty script SHALL remove the tag.

The following substitutions SHALL be applied to the script:
- `%W`: the Tk window path
- `%r`: the quoted result
- `%R`: the quoted result, and the result is reset
- `%0`–`%5`: the command arguments
- `%%`: a literal `%`

(src: tcltk/tclmagic.c:85-383)

#### Scenario: Wrapper updates on box change
- **GIVEN** the wrapper is open
- **WHEN** the `box` command runs
- **THEN** the wrapper's tag callback SHALL refresh the box display in the titlebar (src: tcltk/wrapper.tcl:567-583)

### Requirement: Output and prompts through Tcl
All Magic text output SHALL be written with `puts -nonewline stdout|stderr`. With tkcon, interactive prompts SHALL go through `magic::dialog`; otherwise through `gets stdin`. (src: tcltk/tclmagic.c:1230-1283,1410-1490)

#### Scenario: Error output
- **WHEN** a command reports an error via `TxError`
- **THEN** the text SHALL appear on the Tcl `stderr` channel

### Requirement: Console modes
By default the `magic` script SHALL run inside the tkcon console. `-noconsole` SHALL use the terminal instead: standard input SHALL be replaced by a terminal channel, and buffered input SHALL be drained into the command queue. (src: tcltk/magic.sh.in:33-72, tcltk/tclmagic.c:984-1055)

#### Scenario: tkcon exit
- **WHEN** the user closes the tkcon console
- **THEN** magic SHALL run `quit` through the slave interpreter (src: tcltk/tclmagic.c:787-791, tcltk/console.tcl:51)

### Requirement: Wrapper GUI
When Tk is available and `-nowrapper` is not given, `wrapper.tcl` SHALL define `magic::openwrapper`. That command SHALL create a `.layoutN` toplevel with:
- a titlebar
- File and Options menus
- a toolbar
- a command entry

The system SHALL source the cell, library and DRC managers, the layer toolbar and the text helper inside `catch`, so a failing component does not abort startup. (src: tcltk/wrapper.tcl:20-26,389-409,1052-1512)

#### Scenario: Wrapper startup
- **WHEN** `magic -T scmos` starts under X11
- **THEN** a wrapper window SHALL open, `techmanager initall` SHALL run, and the command-line cells SHALL be loaded by extension (src: tcltk/magic.tcl.in:339-470)

### Requirement: Bundled Tcl tools
If the following files are present, `magic.tcl` SHALL source them: `tools.tcl`, `mazeroute.tcl`, `toolkit.tcl` (the parameterized-cell "gencell" framework) and `readspice.tcl`.

`readspice <file>` SHALL annotate cell port order from SUBCKT definitions. If the given file is not found, it SHALL try the extensions `.spi`, `.spc`, `.spice`, `.sp`, `.ckt` and `.cdl`. (src: tcltk/magic.tcl.in:242-280, tcltk/readspice.tcl:16-40)

#### Scenario: readspice port order
- **WHEN** `readspice cells.spice` is run and the file contains a `.subckt inv A Y VDD VSS` definition
- **THEN** the ports of cell `inv` SHALL be renumbered to the order A, Y, VDD, VSS

### Requirement: Optional socket command server
`socketcmd.tcl` SHALL be installed but SHALL NOT be sourced automatically. When the user sources it, it SHALL listen on TCP port 12946 and SHALL `eval` each line it receives. It has no access control. (src: tcltk/socketcmd.tcl:10-27)

#### Scenario: Not active by default
- **WHEN** magic starts normally
- **THEN** no TCP port SHALL be opened
