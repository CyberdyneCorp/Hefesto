# Netlists, Simulation Hooks and Optional Interpreters

## Purpose
This spec covers four related features:
- Magic's netlist (`.net`) files and the netlist window that edits and verifies them.
- Hooks to the IRSIM switch-level simulator.
- The optional Scheme interpreter (`lisp/`), which is used instead of Tcl when configured.
- The OpenAccess interface (`oa/`), which is a stub.

## Requirements

### Requirement: Netlist file format
A `.net` file SHALL begin with the line ` Netlist File`; the older ` Net List File` SHALL also be accepted. After that line, the format SHALL be:
- one terminal name per line
- a blank line, or a line starting with a space, separates nets
- lines starting with `#` are comments

(src: netmenu/NMnetlist.c:439-499,804-821)

#### Scenario: Missing file
- **WHEN** a netlist that does not exist is selected
- **THEN** the system SHALL report "Netlist file %s.net couldn't be found." and create a new, empty netlist

#### Scenario: Terminal in two nets
- **WHEN** a terminal appears in two nets
- **THEN** the system SHALL warn, and only its last appearance SHALL be used

### Requirement: Netlist editing and verification
The netlist window (`specialopen netlist`) SHALL provide these commands (src: netmenu/NMmain.c:460-551, commands/CmdLQ.c:3003-3044):
- `add`, `cleanup`, `cull`, `dnet`, `dterm`, `extract`, `find`, `flush`, `joinnets`
- `netlist`, `print`, `ripup`, `savenetlist`, `shownet`, `showterms`, `trace`, `verify`, `writeall`

The `netlist` button tool SHALL bind the mouse buttons as follows:
- left: select a net
- middle: join
- right: toggle a terminal

#### Scenario: Verify wiring
- **WHEN** `verify` is run and all nets are wired as listed
- **THEN** the system SHALL report "No wiring errors found."

### Requirement: Node name queries for simulation
`getnode [alias on|off] [globals on|off] [fast] [abort [name]]` SHALL report the hierarchical node names of the selected material. In non-Tcl builds with rsim enabled, `rsim`, `startrsim` and `simcmd` SHALL fork IRSIM (`$RSIM` or the installed `irsim`) and send node commands to it. In Tcl builds, the `irsim` Tcl proc SHALL load Tcl-based IRSIM 9.6 or newer from `$CAD_ROOT`. (src: commands/CmdFI.c:1152-1272, commands/CmdRS.c, sim/SimRsim.c, tcltk/magic.tcl.in:94-104)

#### Scenario: Node of selection
- **WHEN** a piece of metal is selected and `getnode` is run
- **THEN** the hierarchical node name of that metal SHALL be printed

### Requirement: Optional Scheme interpreter
When configured with `--with-interpreter=scheme`:
- Tcl SHALL be disabled.
- Each command line SHALL be evaluated as a Scheme expression.
- `default.scm` and `layout.scm` SHALL be loaded at startup.
- Magic-specific builtins SHALL be available: `getpoint`, `getbox`, `getpaint`, `getselpaint`, `getlabel`, `getsellabel`, `getcellnames`, `magic`.

The interpreter SHALL NOT be built by default. (src: scripts/configure.in:528-551, lisp/lispEval.c:51-145, lisp/lispMain.c:57-135)

#### Scenario: Default build
- **WHEN** magic is configured with defaults
- **THEN** no Scheme interpreter SHALL be included

### Requirement: OpenAccess interface is a stub
The OpenAccess module SHALL be compiled only with `--with-openaccess=DIR`. It currently provides only Tcl query commands (`open_cell`, `get_bounding_box`, and so on). Its cell search SHALL be a placeholder that returns no cells. (src: oa/oa.c:79-131, oa/magicInit.cpp:250-311)

#### Scenario: OA cell lookup
- **WHEN** OA support is compiled in and a cell is looked up through OA
- **THEN** no cell SHALL be found (placeholder behavior)
