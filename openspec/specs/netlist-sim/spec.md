# netlist-sim

## Purpose

This capability defines the `.sim` netlist generator (Magic's `ext2sim` / `exttosim` command) and its companion `.al` (alias) and `.nodes` (label) files, in the MIT, LBL and SU dialects. Reading and flattening are owned by the `ext-flattening-*` capabilities. Name text follows the `.sim` conversion of capability `ext-flattening-names`. Output is always flat.

## Requirements

### Requirement: Command options and defaults
The command SHALL take an optional first word, matched case-insensitively with unique-prefix matching, from `alias labels default format rthresh cthresh merge extresist help`. Any other first word SHALL be treated as run arguments. The options are:

| Option | Effect | Default |
|---|---|---|
| `alias on\|off` | Write aliases to `<cell>.al` instead of the `.sim` file | off |
| `labels on\|off` | Write `<cell>.nodes` | off |
| `format MIT\|LBL\|SU` | Dialect. An unknown value prints `exttosim: output formats are MIT, LBL, or SU`. | MIT |
| `cthresh <fF>\|inf…` | Capacitance threshold | 2 |
| `rthresh <Ω>\|inf…` | Resistance threshold, an integer | 10 |
| `merge none\|conservative\|aggressive` | Parallel-device merging | none |
| `extresist on\|off` | Read `.res.ext` overlays | off |

`default` SHALL reset cthresh to 2 and rthresh to 10, clear all name-conversion and short flags, reset the scale, and clear `-T` and `-p`. Run flags SHALL be the common arguments of capability `ext-flattening-hierarchy` plus:

| Flag | Effect |
|---|---|
| `-A` | No alias file |
| `-B` | No attributes |
| `-F` | Only nodes touched by devices |
| `-L` | No label file |
| `-M` | Aggressive merge |
| `-m` | Conservative merge |
| `-a <file>` | Alias file name |
| `-l <file>` | Label file name |
| `-o <file>` | Output file name |
| `-f mit\|lbl\|su` | Dialect |
| `-J hier\|flat` | Hierarchical source/drain area/perimeter default |
| `-y <n>` | Deprecated; prints `Cap accuracy option -y is deprecated.` |

The default file names SHALL be `<cell>.sim`, `<cell>.al` and `<cell>.nodes`. With extresist on they SHALL be `<cell>.ext.sim`, `<cell>.ext.al` and `<cell>.ext.nodes`.
Compatibility: behavioral
(Magic ref: ext2sim/ext2sim.c:62-100, ext2sim/ext2sim.c:255-600, ext2sim/ext2sim.c:870-970)

#### Scenario: Extresist file naming
- **GIVEN** `ext2sim extresist on`
- **WHEN** it is run on `inv`
- **THEN** the output is written to `inv.ext.sim`

### Requirement: File structure and order
The `.sim` file SHALL begin with `| units: <EFScale %g> tech: <tech> format: <MIT|LBL|SU>`. It SHALL then contain:
1. device lines, in the device visit order of capability `ext-flattening-hierarchy`, after a merge pre-pass if merging is on
2. coupling capacitor lines, if cthresh was finite when the command options were set
3. resistor lines
4. node lines, in node visit order

```ebnf
simfile   = header , { device | cap | res } , { nodeline } ;
header    = "| units: " , number , " tech: " , word , " format: " , ( "MIT" | "LBL" | "SU" ) , NL ;
cap       = "C " , name , " " , name , " " , fixed1 , NL ;
res       = "r " , name , " " , name , " " , gnum , NL ;
nodeline  = [ "C " , name , "0 " , fixed1 , NL ] , [ "R " , name , " " , int , NL ]
            , [ "A " , name , " " , attr , { "," , attr } , NL ] , { "= " , name , " " , name , NL } ;
```

Merging SHALL ignore device class and type. Two devices merge when they have the same gate and substrate nodes, equal truncated `l·s` and `w·s` (W equality only in conservative mode), and the same or swapped source/drain nodes (Q-NETLIST-17). The surviving device's W SHALL be multiplied by the accumulated multiplier `m = m + W_new/W_kept`, truncated to an integer.
Compatibility: exact
(Magic ref: ext2sim/ext2sim.c:640-700, ext2sim/ext2sim.c:1700-1866)

#### Scenario: Header
- **GIVEN** EFScale 5 and technology `scmos`
- **WHEN** a MIT file is written
- **THEN** the first line is `| units: 5 tech: scmos format: MIT`

### Requirement: Device lines
Devices with fewer than 2 terminals SHALL be skipped. A device line SHALL be built as follows:

1. **Class letter.**
   - bjt → `b`
   - diode, pdiode, ndiode → `x`
   - devres → `r`
   - devcap, devcaprev → `c`
   - mosfet, asymmetric, fet → the first character of the type name if it is `n` or `p`. Otherwise use the first character of the device tile type's long name if that is `n` or `p`. Otherwise use `n`, with an error message.
   - subckt-family and veriloga → `x`
   - any other class → the first character of the type name

2. **Terminals.**
   - ` <t0>`, except for devres.
   - ` <t1>`.
   - For bjt with a substrate: ` <substrate>`. Otherwise, if there are more than 2 terminals: ` <t2>`.
   - ` <tk>` for every k ≥ 3.
   - An unresolvable terminal SHALL be written as ` GND`.

3. **Substrate field.** For non-diodes, ` <substrate>` SHALL follow when the device is subckt-family and has a substrate, or in LBL format when the device has a substrate.

4. **Values.**
   - bjt: ` <cl> <cw> <x> <y>`. With the substrate node's total area `a` and half-perimeter `h` summed over classes, `cl = 0.5(h + sqrt(h² − 4a))` and `cw = a/cl`, both truncated with `%d`. `x` and `y` are `%g` of the transformed device lower-left coordinates × s.
   - devres: ` %f` of the resistance.
   - devcap/devcaprev: ` %f` of the capacitance.
   - x-class: optional ` s=<attrs>` and ` d=<attrs>`, then ` l=%g w=%g x=%g y=%g`.
   - Others: ` %g %g %g %g` of `l·s`, `w·s`, `x·s`, `y·s`. Then, unless `-B`, the attributes per Requirement "Transistor attributes".

5. **Trailer.** x-class devices SHALL end with ` <type>`.
Compatibility: exact
(Magic ref: ext2sim/ext2sim.c:1023-1340, ext2sim/ext2sim.c:1493-1544)

#### Scenario: MIT nfet
- **GIVEN** an `nfet` with gate `in`, source `GND`, drain `out`, L=2, W=8 and lower-left (10,20), at top level, MIT format
- **WHEN** it is written
- **THEN** the line is `n in GND out 2 8 10 20`

#### Scenario: Resistor device
- **GIVEN** a `None` devres of 25.5 Ω between `a` and `b`
- **WHEN** it is written
- **THEN** the line is `r a b 25.500000`

### Requirement: Transistor attributes
For MOS-like devices, the attribute fields SHALL be written as follows:
- **Gate.** ` g=<gate attrs>` if the gate has attributes. In SU format, the substrate text follows: after `,` if gate attributes were written, otherwise after ` g=`.
  - If the substrate is the default substrate name, the text is `S_<name>`, with trailing `!`/`#` trimmed per the active flags.
  - Otherwise, if the gate attributes match `ext:aps` (case-insensitive), the text is `A_<a>,P_<p>,S_<node>`, or `A_0,P_0,S_<node>` if the type has no substrate class.
  - Otherwise it is `S_<node>`.
- **Source.** ` s=<source attrs>` if present. In SU format, `,` (or ` s=` if there were no source attributes) followed by `A_<a>,P_<p>` for the source node and class.
- **Drain.** The same with `d=` for the drain.

The values are `a = (int)(area·s²)` and `p = (int)(perim·s)`, clamped at 0. A node/class pair already written yields `A_0,P_0`, as does an undefined class. Source/drain attributes matching `ext:aph` or `ext:apf` SHALL select hierarchical or flat area accounting for that terminal. `-J` sets the default.
Compatibility: exact
(Magic ref: ext2sim/ext2sim.c:1255-1330, ext2sim/ext2sim.c:1343-1490)

#### Scenario: SU fields
- **GIVEN** SU format and an nfet whose default substrate is `GND`, source node with area 40 and perimeter 26, drain node with area 30 and perimeter 16, and no attributes
- **WHEN** it is written
- **THEN** the line ends ` g=S_GND s=A_40,P_26 d=A_30,P_16`

### Requirement: Parasitic and node records
The parasitic and node records SHALL be written as follows:
- **Coupling capacitor.** C aF SHALL be written as `C <n1> <n2> %.1lf` of C/1000, only if C/1000 > cthresh.
- **Resistor.** Every explicit resistor SHALL be written as `r <n1> <n2> %g` of R/1000.

For each visited node (skipped under `-F` if no device referenced it):
- If C/1000 > cthresh: `C <name>` immediately followed by `0 ` and `%.1f` of C/1000. There is no space between the name and `0`; this is a regression (Q-NETLIST-15).
- If `(R + 500)/1000` (integer division, R being the lumped resistance in mΩ) > rthresh: `R <name> <that integer>`.
- If attributes exist and `-B` is not set: `A <name> <a1>,<a2>,…`.
- For each non-canonical name of the node, skipping it when both it and the canonical name are global: `= <canonical> <alias>`. This goes to the alias file when alias is on, otherwise to the `.sim` file.
- When labels are on, `<name> <x> <y> <layer>` SHALL be written to the label file, using the node location and layer name.
Compatibility: exact
(Magic ref: ext2sim/ext2sim.c:1546-1720)

#### Scenario: Node capacitance line
- **GIVEN** node `out` with 5000 aF and cthresh 2
- **WHEN** node lines are written
- **THEN** the line is `C out0 5.0`

#### Scenario: Lumped resistance
- **GIVEN** node `w` with lumped resistance 25600 mΩ and rthresh 10
- **WHEN** node lines are written
- **THEN** the line `R w 26` is written

#### Scenario: Alias in sim file
- **GIVEN** alias off and node `a` that also has the name `u1/x`
- **WHEN** node lines are written
- **THEN** the line `= a u1/x` is written to the `.sim` file

### Requirement: Name conversion state
Names SHALL be written with the `.sim` conversion of capability `ext-flattening-names`, using the session's current conversion flags. Magic does not reset these flags at the start of `ext2sim`, so flags left set by a previous `ext2spice` run (trim-global, convert-equal, convert-comma) stay in effect (Q-NETLIST-16). Hefesto SHALL reproduce this session-state dependence by default.
Compatibility: behavioral
(Magic ref: ext2sim/ext2sim.c:430-445, ext2spice/ext2spice.c:1081-1084, extflat/EFvisit.c:878-920)

#### Scenario: After ext2spice
- **GIVEN** a session in which `ext2spice` ran, followed by `ext2sim` with a node named `vdd!`
- **WHEN** the `.sim` file is written
- **THEN** the node is written as `vdd`
