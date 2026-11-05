# IRSIM / Rsim Simulator Interface and Node Naming

## Purpose

Defines how Hefesto names electrical nodes under selected paint (`getnode`) consistently with Magic's extractor naming, and the interface Magic offers to the switch-level simulators rsim/IRSIM (`rsim`, `startrsim`, `simcmd`, and the Tcl `irsim` command). Node naming is exact because the names must match `.sim`/`.ext`-derived netlists (see capabilities `extract-nodes-and-labels` and `netlist-sim`). The simulator process mechanics (fork/exec, pipes, environment variables, Tcl package loading) are host-specific and informative; iPadOS cannot fork processes, so a platform decision is logged (Q-LEGACY-71).

## Requirements

### Requirement: getnode command forms
`getnode` SHALL accept:

| Form | Effect |
|---|---|
| `getnode` | Print the node name of every distinct node under the selected paint. |
| `getnode fast` | Same, but stop at the first name found per node and name transistor-adjacent nodes by device position (see "Fast node names"). |
| `getnode alias` | Print `Aliases on` or `Aliases off`. |
| `getnode alias on\|off` | Enable/disable reporting of all names (aliases) found for each node. |
| `getnode global[s]` | Print `Node names ending in ! are global (on)` or `... are local (off)`. Any word starting with `global` is accepted (see Q-LEGACY-73). |
| `getnode global[s] on\|off` | `on` (default): names ending in `!` are global; `off`: treat them as local. |
| `getnode abort` | Clear the set of abort names. |
| `getnode abort <name>` | Add `<name>` to the set of abort names (searching stops when a node with that local name is reached). |

Any other form prints `Usage: getnode [abort [str]]`, `   or: getnode alias [on | off]`, `   or: getnode globals [on | off]`, `   or: getnode fast`. The forms that query names require the cursor in a layout window (`Put the cursor in a layout window`). If the selection contains no paint: `You must select paint (not a cell) to use getnode.`. Output: one line `node name : %s` per node (`short node name : %s` for `fast`); in scripting builds the names are returned as a list instead. Nodes are listed in reverse order of discovery (most recently found first); a name is listed once (positional `@=` names are never de-duplicated).

Compatibility: behavioral
(Magic ref: commands/CmdFI.c:1152-1270, sim/SimSelect.c:734-826)

#### Scenario: Query alias mode
- **WHEN** `getnode alias` is executed with default settings
- **THEN** `Aliases off` is printed

#### Scenario: Nothing selected
- **GIVEN** an empty selection
- **WHEN** `getnode` is executed
- **THEN** `You must select paint (not a cell) to use getnode.` is printed

### Requirement: Node discovery from the selection
For every paint tile in the selection (skipping tiles whose types are not visible in the window, and tiles already attributed to a found node), the system SHALL take a 1×1 probe at the tile's lower-left (for a diagonal half-tile, at a corner belonging to the painted half), select all selection paint connected to it, and search the layout hierarchy (all subcells expanded) for all paint connected to the probe's type, computing a candidate name in each cell where connected paint is found (next requirement). The node's name is the best candidate under the preference order. The search stops early when an abort name or a positional name is reached, or on interrupt.

Compatibility: behavioral
(Magic ref: sim/SimSelect.c:262-440, sim/SimDBstuff.c:79-330, sim/SimDBstuff.c:743-860)

#### Scenario: One node for connected paint
- **GIVEN** a selection of two touching metal1 rectangles
- **WHEN** `getnode` runs
- **THEN** exactly one node name is printed

### Requirement: Hierarchical node names
Within a cell reached through use path `u1/u2/.../uk/` (instance ids as printed by the database, including array subscripts such as `u[1,2]`; the top-level cell contributes no prefix), the local node name SHALL be the extractor's node name for the connected region of that cell (the region's preferred label text if labeled, otherwise the generated name of the form used by the extractor, e.g. `<cellname>_<x>_<y>#`; see capability `extract-nodes-and-labels`), and the candidate is `path + localname`. With aliases on, each distinct `path + label` of the region is reported as `alias: %s` (or appended to the script result). After the search, if globals are on and the best name ends in `!`, the path prefix is stripped (text after the last `/`) and the trailing `!` is removed.

Compatibility: exact
(Magic ref: sim/SimExtract.c:723-846, sim/SimSelect.c:129-182, sim/SimDBstuff.c:780-800)

#### Scenario: Global name stripped
- **GIVEN** connected paint whose best name is `u1/u2/vdd!` and globals on
- **WHEN** `getnode` runs
- **THEN** `node name : vdd` is printed

### Requirement: Node name preference order
Between two candidate names a and b, a SHALL be preferred iff the first applicable rule holds:
1. a starts with `@=` → a; else b starts with `@=` → b.
2. If globals are on: if both end in `!`, prefer fewer `/` characters, then shorter length, then the lexicographically GREATER string (`strcmp(a,b) > 0`); else the one ending in `!`.
3. A name not ending in `#` beats one ending in `#` (labels beat generated names).
4. Fewer `/` characters; then shorter length; then lexicographically greater (`strcmp(a,b) > 0`).
Lengths are measured excluding the final character (equivalent to full length comparison). The first candidate becomes best; later candidates replace it when preferred.

Compatibility: exact
(Magic ref: sim/SimDBstuff.c:431-540, sim/SimDBstuff.c:120-135)

#### Scenario: Fewer path components win
- **GIVEN** candidates `u1/net3` and `top_net`
- **WHEN** the preference is evaluated
- **THEN** `top_net` is chosen

#### Scenario: Lexicographic tie-break
- **GIVEN** candidates `u1/b` and `u1/a`
- **WHEN** the preference is evaluated
- **THEN** `u1/b` is chosen (greater string wins)

### Requirement: Fast node names
In `getnode fast` mode (and for simulator selection commands), when the connected region in a cell contains a transistor gate, the name SHALL be `@=g<x>,<y>`, and when it touches a transistor source/drain (and no gate), `@=s<x>,<y>` or `@=d<x>,<y>` for the first or second terminal; (x, y) is the lower-left-most point of the transistor's gate region transformed to root coordinates. Such names terminate the search for that node.

Compatibility: exact
(Magic ref: sim/SimExtract.c:340-720)

#### Scenario: Gate node
- **GIVEN** selected poly that forms the gate of a transistor whose gate region's lower-left is (100,50) in root coordinates
- **WHEN** `getnode fast` is executed
- **THEN** `short node name : @=g100,50` is printed

### Requirement: Abort names
When abort names are set, the search SHALL stop as soon as a cell-local name (before path prefixing) equal to an abort name is computed, printing once per name per invocation `Node name search aborted on "%s"`, and the best name found so far is reported.

Compatibility: behavioral
(Magic ref: sim/SimExtract.c:765-780, commands/CmdFI.c:1160-1230)

#### Scenario: Abort on vdd
- **GIVEN** `getnode abort vdd!` was executed
- **WHEN** `getnode` is run on paint connected to a large `vdd!` net
- **THEN** the search stops when `vdd!` is reached and `Node name search aborted on "vdd!"` is printed

### Requirement: Simulator command semantics
The simulator interface SHALL provide: `rsim [options] file` — start the simulator on `file` with options (if not already running; `usage: rsim [options] file` without args when not running; `Simulator already running.  You cannot start another.`) and enter an interactive loop forwarding typed lines to it until it exits; `startrsim [options] file` — start it and return to the editor; `simcmd cmd [args]` — send a command (`You must first start the simulator by using the rsim command.` if not running). Commands whose name is a unique prefix of `? ! analyzer d h l path t u w x` are applied to each node of the selection (`cmd nodename args`); for `?` (query) the replies set temporary labels `RSIM=1`, `RSIM=0` or `RSIM=X` on the node and the first `time = ` reply is printed once; unknown nodes print `%s not recognized in sim file`. Other commands are sent once and all reply lines are printed. Node names sent are the selection node names computed as in getnode.

Compatibility: behavioral
(Magic ref: commands/CmdRS.c:40-230, sim/SimRsim.c:100-140, sim/SimSelect.c:443-620)

#### Scenario: Query values
- **GIVEN** a running simulator and a selection over node `out`
- **WHEN** `simcmd ? ` is executed
- **THEN** the query is sent for `out`, and a label `RSIM=1`, `RSIM=0` or `RSIM=X` is shown on the node according to the reply

### Requirement: Simulator host mechanics
Magic's mechanics SHALL be documented as informative: the simulator binary is located through the `RSIM` environment variable or the installation binary directory; it is started with fork/exec (vfork on some hosts) connected through two pipes; Magic sends `has_coords` after the hello message; closing the pipes terminates the simulator; `<Simulator is dead>`, `Simulator interrupted.`, `Simulator terminated abnormally.` report its state. The Tcl build instead provides an `irsim` script command that loads the Tcl IRSIM package into the same interpreter, generating the `.sim` file with `ext2sim` if missing. The rsim commands exist only when built with the rsim module.

Compatibility: informative
(Magic ref: sim/SimRsim.c:1-600, doc/html/irsim.html, dbwind/DBWcommands.c:615-630)

#### Scenario: Missing simulator binary
- **GIVEN** no rsim executable can be found
- **WHEN** `rsim file.sim` is executed in Magic
- **THEN** an error such as `can not execute '<path>'` is printed and no simulator runs

### Requirement: Hefesto simulator integration intent
Hefesto SHALL expose the simulator integration as an abstract capability: given a simulator session object (in-process library, remote service, or child process where the host allows it), it SHALL send per-node commands using node names computed exactly as specified above and display returned logic values on nodes. On hosts without process creation (iPadOS), the integration MUST use an in-process or remote backend; see Q-LEGACY-71.

Compatibility: behavioral
(Magic ref: sim/SimRsim.c:140-280, sim/SimSelect.c:443-620)

#### Scenario: No process creation available
- **GIVEN** Hefesto running on iPadOS
- **WHEN** the user starts a simulation
- **THEN** Hefesto uses a non-forking backend (or reports that none is configured) instead of attempting fork/exec
