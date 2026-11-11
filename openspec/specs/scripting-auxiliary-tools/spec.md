# Auxiliary Scripting Tools

## Purpose
Documents the auxiliary Tcl scripts and launchers that are installed with Magic:
- the socket command server;
- the BSI menu helpers;
- the reference-library stripper;
- the netlist maze-router front end;
- the tkcon console glue and the minimal Tk shell;
- the legacy revision-0 gencell toolkit;
- the technology-builder prototype;
- the `magicexec`/`magicdnull` interpreter launchers.

Most of this capability is `informative`. Hefesto MAY omit these tools. Where Hefesto provides an equivalent, the data-affecting behavior marked `behavioral` or `exact` (file rewriting, routing order) SHALL match.

## Requirements

### Requirement: Socket command server is opt-in
The system SHALL NOT open any network listener during normal startup. The script `socketcmd.tcl` is installed but never sourced automatically. When a user sources it, it SHALL:
- listen on TCP port 12946 on all interfaces;
- configure each accepted connection as non-blocking, line-buffered, with CRLF translation;
- for every complete line received, echo the line to stdout and evaluate it as a script with full privileges;
- close the connection on end-of-file.

It performs no authentication. In Hefesto the script cannot run, because the Tcl sandbox denies `socket` (capability `hefesto-scripting`). Any remote-control equivalent is governed by capability `hefesto-security` (Requirement "No remote command server by default"); see Q-SCRIPT-12.
Compatibility: informative

(Magic ref: tcltk/socketcmd.tcl:1-27, tcltk/magic.tcl.in:239-280)

#### Scenario: No port at startup
- **WHEN** magic starts with default options
- **THEN** no TCP port SHALL be listening

#### Scenario: Remote command after opt-in
- **GIVEN** `source $CAD_ROOT/magic/tcl/socketcmd.tcl` was run
- **WHEN** a client connects to port 12946 and sends `box 0 0 10 10\r\n`
- **THEN** the box SHALL become (0,0)-(10,10)

### Requirement: Reference-library stripper
`strip_reflibs.tcl` is a standalone script, not loaded by the editor. It SHALL process every `*.mag` file in the current directory. For each file it SHALL remove every line beginning with `string GDS_START` or `string GDS_END`, keep all other lines (including `string GDS_FILE`), and write the result back in place through a temporary file `tmp.mag`. If a file cannot be opened, it SHALL print `Error: can't open file <name>` and continue with the next file. The effect is that cells become library references whose GDS content is not copied on output (see capability `gds-read`).
Compatibility: exact

(Magic ref: tcltk/strip_reflibs.tcl:1-41)

#### Scenario: Strip offsets
- **GIVEN** `inv.mag` contains the lines `string GDS_FILE cells.gds`, `string GDS_START 1024` and `string GDS_END 4096`
- **WHEN** the script runs in that directory
- **THEN** `inv.mag` SHALL contain `string GDS_FILE cells.gds` and neither of the other two lines

### Requirement: BSI menu helpers
`bsitools.tcl` is installed but not loaded. If it is sourced, it SHALL define:
- `magic::add_menu F tool text`: a menu button `F.titlebar.mbuttons.<tool>` with a menu `...<tool>.toolmenu`;
- `magic::add_menu_command F tool text command`;
- `magic::add_menu_separator F tool`.

These behave like the toolkit menu hooks in `pcell-toolkit`.
Compatibility: informative

(Magic ref: tcltk/bsitools.tcl:1-34, tcltk/magic.tcl.in:263-272)

#### Scenario: Not loaded by default
- **WHEN** magic starts
- **THEN** `info commands magic::add_menu` SHALL be empty

### Requirement: Netlist maze-routing script
`mazeroute.tcl` is loaded at startup when present. It defines `mazeroute <netfile>`. If `<netfile>` cannot be opened, the command SHALL try `<netfile>.net`. If that also fails, it SHALL print `Can't read netlist file <netfile>` and return `1`. Otherwise it SHALL:
1. Discard the first line of the file.
2. Treat blank lines as net separators.
3. Within each net, route consecutive terminal pairs as a chain: for each terminal after the first, run `iroute route -dlabel <previous> -slabel <current> -timeout 3`.

The script also defines an interactive "maze helper" window, with lists of unrouted and routed nets, route/rip-up/verify actions, and saving and loading of failed nets. It is reachable from the wrapper's Options menu as `Maze Router`. The `.net` fallback and the chain order are reproduced in both `magic-compat` settings (Q-SCRIPT-30, resolution: replicate). The script drives `iroute` (capability `router-maze-iroute`), so it is provided and tested together with that capability.
Compatibility: behavioral

(Magic ref: tcltk/mazeroute.tcl:1-675)

#### Scenario: Chain routing order
- **GIVEN** a `.net` file whose first line is a header, followed by the lines `a`, `b`, `c`, an empty line, and `d`, `e`
- **WHEN** `mazeroute f.net` runs
- **THEN** the routes SHALL be attempted in this order: a←b, b←c, d←e (destination←source)

### Requirement: Console integration
When run under the tkcon console, the console interpreter SHALL provide:
- `magic::dialog` (prompt for a line, using `?` as the prompt character), `magic::consolegeometry`, `magic::consolefocus`, `magic::suspendout` and `magic::resumeout` as aliases into the editor interpreter;
- a prompt character set by the engine: `%` normally, `]` while DRC runs, `:` during long-command entry;
- closing the console window SHALL run `magic::quit`, which asks about unsaved cells; the console's `exit` SHALL call the editor's `quit`;
- a console "new console" action SHALL NOT load a second editor instance.

The minimal shell `tkshell` SHALL provide a scrolled text evaluator with history, used by the per-window command entry (see `ui-gui-managers`).
Compatibility: informative

(Magic ref: tcltk/console.tcl:1-135, tcltk/tkshell.tcl:1-202, tcltk/tclmagic.c:787-791,1128-1154)

#### Scenario: Exit from console
- **GIVEN** a modified, unsaved cell
- **WHEN** the console window is closed
- **THEN** the user SHALL be asked whether to discard changes

### Requirement: Interpreter launchers
The `magic` launcher script SHALL choose the interpreter as follows:
1. Without `-noconsole`, it SHALL run tkcon, evaluate `console.tcl` in the console interpreter, and source `magic.tcl` in a child interpreter. With `-dnull`, Tk is not required.
2. With `-noconsole`, it SHALL exec `magicexec`, a Tk shell whose startup file is `magic.tcl`.
3. With `-noconsole -dnull`, `--version`, `--commit` or `--prefix`, it SHALL exec `magicdnull -nowrapper <args>`, a plain Tcl shell whose startup file is `magic.tcl` and which sets the global `batch_mode` to `true`.

Arguments SHALL be passed with their quoting preserved.
Compatibility: informative

(Magic ref: tcltk/magic.sh.in:1-72, tcltk/magicdnull.c:25-61, tcltk/magicexec.c:60-87)

#### Scenario: Batch mode flag
- **WHEN** `magic -dnull -noconsole` starts
- **THEN** the global `batch_mode` SHALL be `true`

### Requirement: Legacy gencell toolkit revision 0
`toolkit_rev0.tcl` is installed but not loaded. It is superseded by `toolkit.tcl`. Its contract SHALL be documented for migration only. Each device provides `<lib>::<type>_params`, `_check` and `_draw`, all taking the cell name `gname`. Parameters are stored as individual cell properties (`cellname property gname <pname> <value>`) and not as one dictionary. Generated cells are named `<type>_<n>`, where n is the first integer, counting up from 1, for which no loaded cell has that name. Control-P is bound to `magic::gen_params`. Hefesto SHALL be able to open cells produced by it as ordinary cells with properties.
Compatibility: informative

(Magic ref: tcltk/toolkit_rev0.tcl:1-284)

#### Scenario: Legacy naming
- **GIVEN** cells `pfet_1` and `pfet_2` exist
- **WHEN** the rev-0 toolkit creates another `pfet`
- **THEN** the new cell SHALL be named `pfet_3`

### Requirement: Technology builder prototype
`techbuilder.tcl` SHALL be treated as an unfinished prototype. It requires the BLT package and creates a tabbed `.techwizard` window with embedded layout windows `tech_layers` and `drc_rules`. It is not loaded by default and has no data-format effects.
Compatibility: informative

(Magic ref: tcltk/techbuilder.tcl:1-113)

#### Scenario: Not loaded by default
- **WHEN** magic starts
- **THEN** no `.techwizard` window SHALL exist
