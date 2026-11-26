# extresist-command

## Purpose

This capability defines the `extresist` command (full R-C extraction): its options and persistent settings, the inputs it reads, which nets it processes and the cutoff arithmetic, the choice of driver, and the per-cell flow. Network construction and simplification belong to capability `extresist-network`. File formats belong to capability `extresist-output`. Units: resistance in mΩ, capacitance in aF, delay in zeptoseconds (zs = mΩ·aF; 1 ps = 1e9 zs).

## Requirements

### Requirement: Command options
The first word SHALL be matched case-insensitively, with unique-prefix matching, against `all threshold minresist mindelay tolerance simplify extout lumped silent debug skip force ignore include box cell blackbox fasthenry geometry stats help`. With no word, the command runs. An ambiguous word SHALL print `Ambiguous option: <w>`; an unknown word SHALL print `Unknown option: <w>`. Settings SHALL persist for the session.

| Option | Effect | Default |
|---|---|---|
| `threshold [v]` | Lumped-resistance cutoff `rthresh` in mΩ, float. A negative value prints usage. | 10000 |
| `minresist [v]` | Simplification tolerance `minres` in mΩ | 1000 |
| `mindelay [v]` | Delay cutoff, given in ps and stored ×1e9 zs. The query returns the value in ps, and any text output prints ps, in both `magic-compat` settings (Q-NETLIST-28, resolution: replicate, as Magic's normative Tcl build). Informative: Magic's non-Tcl build reports it unconverted; this is not reproduced. | 1 ps |
| `tolerance` | Prints `Note:  This option has been deprecated and is unused.` | — |
| `simplify on\|off` | Network simplification | on |
| `extout on\|off` | Write `.res.ext` | on |
| `lumped on\|off` | Write `.res.lump` | off |
| `silent on\|off` | Suppress per-net messages | off |
| `debug on\|off` | Diagnostics | off |
| `stats on\|off` | Statistics | off |
| `blackbox on\|off` | Do not descend into subcells that have ports | off |
| `skip <type>[,<type>…]` | Remove each listed layer type from all net-copy connectivity masks and clear its own mask. With no argument, restore the masks to the technology connectivity. | — |
| `force <net>\|none` | Always process the net; `none` clears the list | empty |
| `ignore <net>\|none` | Never process the net; `none` clears the list | empty |
| `include <net>\|all` | Process only the listed nets; `all` clears the list | empty |
| `all` | Run once on every net, ignoring cutoffs | — |
| `cell` | Run on the selected cell | — |
| `box <type>` | Extract the net under the box on `<type>` and print the network | — |
| `geometry` | Run with geometry output only (see capability `extresist-output`) | — |
| `fasthenry [freq]` | Run with FastHenry output only | — |
| `help` | List the options | — |

`skip` SHALL accept a comma-separated list of types, as documented, in both `magic-compat` settings (Q-NETLIST-23, resolution: fix; Magic accepts one type and writes out of bounds on a list or a bad name). A single type behaves exactly as in Magic. An unknown or ambiguous type reports `Unrecognized layer (type) name "<t>"` or `Ambiguous layer (type) name "<t>"` and leaves the masks unchanged.

A negative `threshold`, `minresist` or `mindelay` value SHALL print `Usage:  extresist threshold|minres|mindelay [value]`. The value is then stored with `magic-compat` ON, as Magic does, and the previous value is kept with it OFF (Q-NETLIST-65, resolution: compat, approved 2026-11-25). `force`, `ignore` and `include` without a name list the names, each followed by a space, in the order of Magic's string hash table.

Only `on` or `off` (prefix allowed) SHALL be accepted by the on/off options; any other value prints `Value must be either "on" or "off".` and changes nothing. Exception, with `magic-compat` ON only (Q-NETLIST-66, resolution: compat, approved 2026-11-25): Magic checks no value for `debug` and `stats`, so any other word turns them on, as in Magic. `geometry` and `fasthenry` SHALL do the following for that run only, restoring the previous settings afterwards:
- turn on "all"
- turn off `.res.ext`, `.res.lump` and simplification

A frequency argument that does not parse prints `Cannot parse frequency value.  Assuming default`. The frequency default intended by the source (10 MHz) is never stored (Q-NETLIST-21).
Compatibility: behavioral
(Magic ref: resis/ResRex.c:266-700, resis/ResRex.c:371-390, resis/ResRex.c:572-588, resis/ResRex.c:202-262, resis/resis.h:506-518, doc/html/extresist.html)

#### Scenario: Threshold in milliohms
- **GIVEN** `extresist threshold 50000`
- **WHEN** a later run evaluates a net with a lumped resistance of 40 Ω
- **THEN** the net is not processed by the resistance cutoff

#### Scenario: Include list overrides ignore
- **GIVEN** `extresist include clk` and `extresist ignore clk`
- **WHEN** a run evaluates net `clk`
- **THEN** `clk` is considered for processing, because the include list is consulted instead of the ignore list

#### Scenario: Skip list
- **GIVEN** a technology with types `mcon` and `via1`
- **WHEN** `extresist skip mcon,via1` is issued
- **THEN** both types are removed from every net-copy connectivity mask and their own masks are cleared

#### Scenario: Negative threshold
- **WHEN** `extresist threshold -5` is issued
- **THEN** `Usage:  extresist threshold [value]` is printed
- **AND** with `magic-compat` ON the threshold becomes -5, as in Magic; with it OFF it keeps its previous value

#### Scenario: Skip with an unknown type
- **WHEN** `extresist skip mcon,nosuchtype` is issued
- **THEN** an error is reported and no connectivity mask changes

#### Scenario: Any word for debug with magic-compat ON
- **GIVEN** `magic-compat` ON
- **WHEN** `extresist debug maybe` is issued
- **THEN** debug output is turned on, as in Magic (Q-NETLIST-66)

#### Scenario: Any word for debug with magic-compat OFF
- **GIVEN** `magic-compat` OFF
- **WHEN** `extresist debug maybe` is issued
- **THEN** `Value must be either "on" or "off".` is printed and debug keeps its value (Q-NETLIST-66)

### Requirement: Run flow
A run SHALL proceed as follows:
1. Use the cell under the box (or the selected cell for `cell`) as the target. If there is no target, print `Couldn't find def corresponding to box` and stop.
2. Unless blackbox is on, first process every subcell in the target's hierarchy that is a subcircuit (has ports), each once.
3. Prepare the substrate.
4. Process the target cell.

For each processed cell:
1. Read `<cell>.ext` (see Requirement "Input model").
2. Open the outputs: `<cell>.res.ext` in the extraction output directory, `<cell>.res.lump` in the current directory, and `<cell>.fh`.
3. Write the `.res.ext` header.
4. Process nets in the order described below.
5. Append the device records.
6. Print `Total Nets: <t>\nNets extracted: <e> (<e/t %f>)\nNets output: <o> (<o/t %f>)`, or `Total Nodes: 0`. This message is informative.

Nets SHALL be processed in reverse order of their first mention while reading the `.ext` file. A run SHALL stop processing nets when interrupted; the interrupted cell's `.res.ext` and `.res.lump` are then not written (capability `extresist-output`, Q-EXTRACT-27). FastHenry output for a non-subcircuit cell SHALL skip net processing.

With `magic-compat` ON `extresist cell` runs on the cell under the box, as Magic does (its box lookup replaces the selected cell); with it OFF it runs on the selected cell (Q-NETLIST-67, resolution: compat, approved 2026-11-25). A cell whose `.ext` file cannot be opened prints `Cannot open file <cell>.ext` and the run goes on.
Compatibility: behavioral
(Magic ref: resis/ResRex.c:700-800, resis/ResRex.c:86-200, resis/ResRex.c:1323-1467, resis/ResReadExt.c:1078-1109)

#### Scenario: Net order
- **GIVEN** an `.ext` file whose `node` records are `a`, then `b`, then `c`, with no earlier mentions
- **WHEN** nets are processed
- **THEN** the order is `c`, `b`, `a`

#### Scenario: extresist cell with magic-compat ON
- **GIVEN** `magic-compat` ON, cell `a` selected and the box over cell `b`
- **WHEN** `extresist cell` runs
- **THEN** it processes `b`, the cell under the box, as Magic does (Q-NETLIST-67)

#### Scenario: extresist cell with magic-compat OFF
- **GIVEN** `magic-compat` OFF, cell `a` selected and the box over cell `b`
- **WHEN** `extresist cell` runs
- **THEN** it processes `a`, the selected cell (Q-NETLIST-67)

### Requirement: Input model
`<cell>.ext` SHALL be read with the tokenizer of capability `ext-flattening-read`, using only these records:
- **`node`/`substrate`**: create the net. Its location and layer type are set. Its resistance is `R × resistScale` mΩ, where `R` is the record's integer and a negative value means infinite. Its capacitance gains `C` aF.
- **`device`/`fet`**: record gate, source, drain and substrate nodes and attributes, and `W/L`. W is the larger of the source and drain terminal lengths, L is the gate terminal length, and W/L is 0 when L = 0.
- **`port`**: add a drive point; the net is flagged forced, drive-located and port.
- **`connect`**: a connection to a child cell; add a sink point.
- **`cap`**: add C to both nets' capacitance.
- **`attr`**: the node it applies to depends on the `magic-compat` setting (Q-NETLIST-29, resolution: compat): with it ON (the default) the most recently read node record, not the named node, as in Magic; with it OFF the node named in the record, and an unknown name prints a warning and the record is ignored. In both settings an `attr` before any node record is ignored with a warning, and `res:min=` values of any length are accepted (Magic reads an uninitialised node and overflows a 20-byte buffer). Recognized texts:
  - `res:skip` marks the net skipped.
  - `res:force` marks it forced.
  - `res:min=<v>` sets a minimum.
  - `res:drive` adds a drive point at the attribute location.

In both settings, beyond Magic (Q-NETLIST-68, resolution: fix, approved 2026-11-25): a negative node resistance is infinite (Magic converts INFINITY to an integer); a device terminal naming a net with no earlier record creates the net, which has no node record (Magic dereferences a null net); a device whose model the style does not declare has no tile type and drives nothing (Magic dereferences a null device line). Likewise (Q-NETLIST-63, resolution: fix, approved 2026-11-25): a device whose substrate terminal names a net with no node record, such as a global `GND!` that no `node` record declares, creates that net, which has no node record, and the run continues (Magic adds the device to a null node and `extresist` crashes).

The `.ext` files of all parent cells SHALL be read for `connect` records into this cell. Those connection rectangles are transformed into this cell's coordinates and added as drive points, flagging the net forced and drive-located.
Compatibility: exact
(Magic ref: resis/ResReadExt.c:131-575, resis/ResReadExt.c:139, resis/ResReadExt.c:204-210, resis/ResReadExt.c:655-1075, resis/ResReadExt.c:1000-1060)

#### Scenario: Node resistance scaling
- **GIVEN** an extraction style with resistScale 1000 and the record `node n 25 3000 0 0 m1`
- **WHEN** it is read
- **THEN** net `n` has a resistance of 25000 mΩ and a capacitance of 3000 aF

#### Scenario: Substrate terminal on a net with no node record
- **GIVEN** `top.ext` with the nodes `s` and `g` and the record `device mosfet nfet … "GND!" "g" … "s" … "s" …`, and no `node` record for `GND!`
- **WHEN** `extresist all` runs, with `magic-compat` ON or OFF
- **THEN** the run completes and writes `top.res.ext`, and the summary line is `Total Nets: 2`, as decided by Q-NETLIST-63

#### Scenario: Attribute for an earlier node
- **GIVEN** the records `node a …`, `node b …` and then `attr a 0 0 0 0 m1 "res:skip"`
- **WHEN** they are read
- **THEN** with `magic-compat` ON net `b` is marked skipped, as in Magic
- **AND** with `magic-compat` OFF net `a` is marked skipped

### Requirement: Net selection and first cutoff
A net SHALL be skipped in these cases:
- The include list is non-empty and the net is not in it.
- The include list is empty and the net is in the ignore list.
- It has no `node` record.
- It was already absorbed into an earlier network.
- It is marked skip and "all" is off.

The driver location SHALL be chosen as follows:
1. Find the device with the largest summed W/L over a group of consecutive parallel devices (same gate, same or swapped source/drain), considering only mosfet, fet, asymmetric, msubckt and bjt devices where this net is source or drain. Use its location.
2. If the net is drive-located, use its first drive point instead, or failing that its first sink point.
3. If there is still no driver, use the first drive point, else the first sink point.
4. If there is still no driver, skip the net. A forced net reports an error.

The devices of a net are ordered as ResSortByGate orders them: entries on a source or drain first, grouped by gate net and then by the other source or drain net, entries on a gate last. Magic compares net addresses; Hefesto compares the order in which the nets were first mentioned, with a stable sort, so the order is the same on every platform, in both `magic-compat` settings (Q-NETLIST-69, resolution: fix, approved 2026-11-25).

The net SHALL be processed when any of the following holds:
- "all" is on.
- `(rthresh = 0 or R > rthresh) and (mindelay = 0 or C = 0 or R·C > mindelay)`.
- The net is in the force list.
Compatibility: exact
(Magic ref: resis/ResRex.c:1066-1320, resis/ResRex.c:1891-1996)

#### Scenario: Delay cutoff
- **GIVEN** rthresh 10000, mindelay 1e9 zs, and a net with R = 20000 mΩ and C = 30000 aF
- **WHEN** it is evaluated
- **THEN** R·C = 6e8 < 1e9, so the net is not processed

#### Scenario: Resistance cutoff passes
- **GIVEN** the same thresholds and a net with R = 20000 and C = 100000
- **WHEN** it is evaluated
- **THEN** R·C = 2e9, so the net is processed

### Requirement: Second cutoff after extraction
After the network is built and simplified (see capability `extresist-network`), a net SHALL be withheld from output, unless it is in the force list, in either of these cases:
- `rthresh > 0` and `maxres < rthresh`, where maxres is the largest shortest-path resistance from the driver.
- `mindelay > 0`, `C > 0`, Tdi was computed, and the largest Elmore delay `Tdi < mindelay`.

Withheld nets still count as extracted but not as output.
Compatibility: exact
(Magic ref: resis/ResRex.c:2093-2180, resis/ResRex.c:1270-1300)

#### Scenario: Short network withheld
- **GIVEN** rthresh 10000 and a network whose maxres is 8000 mΩ
- **WHEN** output is decided
- **THEN** nothing is written for the net, unless it is forced

### Requirement: Lumped resistance file
With `lumped on`, each processed net that was extracted without error SHALL append `R <net> <r>` to `<cell>.res.lump`. When mindelay > 0, `r = (int)((Tdi / C_total) / 1000)`, or 0 if the capacitance is 0. Otherwise `r = (int)(maxres / 1000)`. In both cases r is in Ω.
Compatibility: exact
(Magic ref: resis/ResRex.c:1998-2030)

#### Scenario: Lumped by resistance
- **GIVEN** mindelay 0 and maxres 12345 mΩ for net `a`
- **WHEN** the lump file is written
- **THEN** the line is `R a 12`
