# UI Wizard and Debug Commands

## Purpose
Documents Magic's wizard commands. These are the commands whose names begin with `*`. They exist for developers: debugging, statistics and internal dumps. They are listed here for reference. Hefesto MAY implement any of them differently or not at all, unless a requirement below says otherwise.

## Requirements

### Requirement: Wizard command visibility
Commands whose names begin with `*` SHALL be dispatched like any other command. Plain `help` SHALL exclude them. `help wizard` SHALL list only them (see ui-command-dispatch). Client names that begin with `*` SHALL be hidden from window-type listings.

Compatibility: behavioral
(Magic ref: windows/windSend.c:402-452, windows/windMain.c:396-420)

#### Scenario: Hidden from help
- **WHEN** `help` is executed in a layout window
- **THEN** `*watch` SHALL NOT be listed
- **AND** `help wizard` SHALL list it

### Requirement: Window-client wizard commands
The `*window` client's wizard commands SHALL be treated as informative. Magic provides:

| Command | Effect |
|---|---|
| `*bypass CMD…` | Tcl build only. Runs a command without changing the command counter, the selection state or the input redirection. Default button bindings use it so that clicks do not break background DRC. It is never logged. |
| `*crash` | prints `OK -- crashing...` and aborts with a core dump |
| `*files` | lists the status of file descriptors 0–19 and prints `N open files, M unopened file descriptors left` |
| `*grstats N [STYLE]` | redraws the window N times, or N times a 15-pixel box pattern on a 20-pixel pitch in STYLE, then prints `[stats]` and `R rectangles, U uS, P uS/rectangle, S rects/sec` |
| `*pause [args]` | updates the display, prints the args, then `Pausing: type <cr> to continue: ` and waits for a line |
| `*winddebug` | toggles printing of every dispatched command and prints `Window command debugging set to TRUE|FALSE` |
| `*winddump` | prints the internal window list |

Hefesto MUST provide an equivalent of `*bypass` semantics wherever its default bindings rely on it. That is, a pointer action that runs a command SHALL NOT be recorded in the command log, and SHALL NOT count as a user command for purposes such as interrupting background DRC.

Compatibility: informative (`*bypass` effect: behavioral)
(Magic ref: windows/windClient.c:680-828, windows/windCmdAM.c:375-459, windows/windCmdAM.c:591-645, windows/windCmdAM.c:689-916, windows/windCmdNR.c:151-202, textio/txCommands.c:1491-1594)

#### Scenario: Bypass not logged
- **GIVEN** command logging is on
- **WHEN** a pointer click runs `*bypass box move bl cursor`
- **THEN** no `*bypass` line SHALL be written to the log

### Requirement: Layout wizard commands
Hefesto SHALL NOT be required to match the layout client's wizard commands; they MAY be implemented as developer tools. Magic provides (informative):

| Command | Effect |
|---|---|
| `*coord` | prints the point, box, root cell and edit cell rectangles. Each is given in root coordinates (`r=(x,y)::(x,y)`) and, when the edit cell is in that root, in edit coordinates (`e=…`). |
| `*extract [args]` | extractor debugging (owned by the extraction capability) |
| `*plow cmd [args]` | plow debugging (owned by `plow`) |
| `*psearch plane count` | times point searches over the box area |
| `*tsearch plane count` | times area searches over the box area |
| `*showmem [file]` | prints internal memory use |
| `*showtech [file]` | dumps the internal technology tables |
| `*tilestats [file]` | prints tile-use statistics |
| `*watch [plane] [demo] [types]` | tile-structure overlay (see Requirement: Tile structure overlay) |

With the router modules, these are added: `*garoute`, `*groute`, `*iroute`, `*mzroute` and `*seeflags` (owned by the router capabilities).

Compatibility: informative
(Magic ref: dbwind/DBWcommands.c:200-230, dbwind/DBWcommands.c:670-720, commands/CmdWizard.c:86-1300)

#### Scenario: coord output
- **GIVEN** a box at (0,0)-(10,10) in the edit cell's root, with the edit cell at the root
- **WHEN** `*coord` is executed
- **THEN** a line starting with `Box:` and containing `r=(0,0)::(10,10)` and `e=(0,0)::(10,10)` SHALL be printed

### Requirement: Tile structure overlay
`*watch PLANE [demo] [types]` SHALL make the pointed layout window draw, on top of the layout, the corner-stitched tile structure of PLANE in the edit cell, or in the root if there is no edit cell. Each tile SHALL be outlined in the tile-debug style (19) and labeled at its center:
- with `types`: the tile type's short name
- without `demo`: the tile's address

Without `demo`, the four corner stitches SHALL be shown as text near the corners, 12 pixels in from them. With `demo`, they SHALL be shown as arrows.

`*watch` with no plane SHALL turn the overlay off. An unknown plane SHALL list the legal plane names. While the overlay is on, any change in the window SHALL redraw the whole window. Hefesto MAY implement this as a developer overlay with different visuals.

Compatibility: informative
(Magic ref: commands/CmdWizard.c:1245-1300, dbwind/DBWdisplay.c:156-574, dbwind/DBWdisplay.c:1080-1294)

#### Scenario: Watch off
- **GIVEN** `*watch metal1` is active
- **WHEN** `*watch` is executed with no arguments
- **THEN** the tile overlay SHALL disappear at the next redraw
