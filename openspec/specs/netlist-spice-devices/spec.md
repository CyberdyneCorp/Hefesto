# netlist-spice-devices

## Purpose

This capability defines the exact SPICE text for each extracted device: the instance name and counters, the terminal order per device class, geometry and parameter formatting under both scale modes, source/drain area and perimeter sharing, substrate naming, resistor tee and pi handling, CDL variations, and parallel-device merging. Notation: `SI(v)` is the formatting of capability `netlist-spice-options`, `name(n)` is the node naming of the same capability, `s` is the per-definition scale passed by the device visit (normally 1.0), and `E` is the length unit `S_out/100` µm (`S_out` is the global output scale of capability `ext-flattening-read`) used when scale mode is off.

## Requirements

### Requirement: Device instance names
Each emitted device SHALL start with a class letter:

| Letter | Classes |
|---|---|
| `M` | mosfet, asymmetric, fet |
| `Q` | bjt, or any device whose type name is `npn` |
| `D` | diode, pdiode, ndiode |
| `R` | devres |
| `C` | devcap, devcaprev |
| `V` | vsource |
| `N` | veriloga |
| `X` | subckt, rsubckt, csubckt, dsubckt, msubckt |

If the gate terminal (terminal 0) has an attribute string, that string SHALL follow the letter. In flat mode it is preceded by the raw `/`-joined instance path without a trailing separator (Q-NETLIST-34). In hierarchical mode no path is added. Otherwise the letter SHALL be followed by a counter:
- `R` and `D` each have their own counter.
- `C` shares its counter with parasitic capacitors.
- `V` has its own counter.
- `X` and `N` share the subcircuit counter.
- `M` and `Q` share a counter that starts at 1000.

With `resistor tee on`, `R` names and `rsubckt` names SHALL get the suffix `A`.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:2782-2890, ext2spice/ext2hier.c:780-860)

#### Scenario: First transistors
- **GIVEN** a flat netlist with two MOSFETs and one unmodeled capacitor device
- **WHEN** they are written
- **THEN** they are named `M1000`, `M1001` and `C0`

### Requirement: Device eligibility and annotations
A device SHALL be skipped in the following cases:
- In flat mode it has no terminals. In hierarchical mode it has at most one terminal.
- It was merged away (see Requirement "Parallel device merging").
- It is a non-subcircuit device with fewer than 2 terminals. Diodes with a substrate are exempt. Skipping reports `Device other than subcircuit has only one terminal` or `Diode has only one terminal`.

Before the line, `** SOURCE/DRAIN TIED` SHALL be written for a MOSFET-class device whose source and drain are the same terminal, which happens with 2 terminals. `** SHORTED DEVICE` SHALL be written for another class when the gate and source are the same terminal. In ngspice each such comment is prefixed with `; `. When terminal 1's attribute is exactly `D` or terminal 2's attribute is exactly `S`, terminals 1 and 2 SHALL be swapped before output.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:2730-2780, ext2spice/ext2spice.c:2657-2665)

#### Scenario: Two-terminal MOS capacitor
- **GIVEN** a mosfet with terminals gate `g` and source `x` only, in ngspice format
- **WHEN** it is written
- **THEN** the line `; ** SOURCE/DRAIN TIED` precedes its `M` line, which lists `x g x`

### Requirement: MOSFET lines
A mosfet, asymmetric or fet device SHALL be written as follows:

```
M<id> <name(d)> <name(g)> <name(s)>[ <sub>] <type> w=<W> l=<L><params>[ M=<m>]
+ <dAP><sAP>[ * <subAP>]
[**devattr[ g=<ga>][ s=<sa>][ d=<da>]]
```

- **Width and length.** In scale mode `<W>` and `<L>` are `%g` of `w·s` and `l·s`. Otherwise they are `SI(1e-6·w·s·E)` and `SI(1e-6·l·s·E)`.
- **Parameters.** `<params>` follows Requirement "Device parameter output".
- **Multiplier.** ` M=%g` is written only when the merge multiplier ≠ 1.
- **Area/perimeter line.** The second line starts with `+ ` and contains the drain `ad`/`pd` pair, then the source `as`/`ps` pair, per Requirement "Terminal area and perimeter".
- **Substrate area.** ` * ` and `asub=`/`psub=` follow only when the gate attributes match `ext:aps` (case-insensitive glob). When the type has no substrate resistance class, or the substrate is the default name, the text is `asub=0 psub=0`.
- **Attribute line.** It is written unless `-B` is given and only when the gate has attributes or source/drain have non-empty attributes.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:3333-3420, ext2spice/ext2hier.c:1180-1260)

#### Scenario: Default-scale nfet
- **GIVEN** scale mode on, `nfet` with W=8 and L=2, drain `out`, gate `in`, source `GND`, substrate default `GND`, no declared parameters, and drain area 40 and perimeter 26 (class not yet visited)
- **WHEN** it is written
- **THEN** the first line is `M1000 out in GND GND nfet w=8 l=2` and the second starts `+  ad=40p pd=26u as=`

#### Scenario: Scale off
- **GIVEN** scale mode off with E = 0.05 µm and the same device
- **WHEN** it is written
- **THEN** the first line ends `nfet w=0.4u l=100n` and the drain values are `ad=0.1p pd=1.3u`, because 1e-7 is below 1.0001e-7 and so takes the `n` suffix

### Requirement: Terminal area and perimeter
For a terminal written with names `<a>`/`<p>`, the text SHALL be ` <a>=<SI>` and ` <p>=<SI>`, either of which may be absent. The values SHALL be chosen as follows:
1. If the terminal's attribute string contains a substring that parses as `<int>,<int>` (scanning from the start and from just after each comma), those are area A and perimeter P. The attribute string is truncated at that point.
2. Otherwise, A and P are the node's area and perimeter for the type's source resistance class (or drain class for terminal index > 1). If the class is undefined (−1), or that node and class were already written by an earlier device, the values are 0. Otherwise the node and class are marked as written.

In scale mode the output is `SI(1e-12·A·s²/m)` and `SI(1e-6·P·s/m)`, where m is the merge multiplier. Otherwise it is `SI(1e-12·A·s²·E²)` and `SI(1e-6·P·s·E)`, with no division by m (Q-NETLIST-7). With `-d` (distributed junctions), the node values are instead multiplied by `w / Σw`, where Σw is the total width of devices on that node and class, and no node is zeroed.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:3658-3790, ext2spice/ext2spice.c:3792-3885)

#### Scenario: Shared diffusion counted once
- **GIVEN** two MOSFETs that share drain node `x` with area 30 and perimeter 16, in scale mode
- **WHEN** both are written
- **THEN** the first has `ad=30p pd=16u` and the second has `ad=0 pd=0`

#### Scenario: Terminal attribute area
- **GIVEN** a source terminal whose attributes are `12,14`
- **WHEN** it is written in scale mode
- **THEN** its values are `as=12p ps=14u` and no `s=` attribute is written

### Requirement: Device parameter output
For each parameter declared for the device type (see capability `ext-flattening-read`), in stored order, the output SHALL be as follows. In every case below, "scale mode" means the `%g` form is used; scale `k ≠ 1` means the `%g` form is used; otherwise the SI form is used.

| Key | Scale mode | Scale `k ≠ 1` | Otherwise |
|---|---|---|---|
| `a`/`a0` | ` <n>=%g` of `A·s²` | `%g` of `A·s²·E²·k·1e-12` | `SI(1e-12·(A+off)·s²·E²)` |
| `p`/`p0` | `%g` of `P·s` | `P·s·E·k·1e-6` | `SI(1e-12·(P+off)·s·E)` in flat mode (Q-NETLIST-19); `SI((P+off)·s·E·1e-6)` in hierarchical mode |
| `l`/`l0` | `%g` of `l·s` | as above with k | `SI(1e-6·(l+off)·s·E)` |
| `w`/`w0` | `%g` of `w·s` | as above with k | `SI(1e-6·(w+off)·s·E)` |
| `x`/`y` | the device's lower-left x/y coordinate, scaled like `l` | | |

The remaining keys SHALL be handled as follows:
- **`a<n>`/`p<n>`** (n ≥ 1): the area/perimeter of terminal n, clamped to the last terminal, per Requirement "Terminal area and perimeter". An `a<n>` immediately followed by `p<n>` with the same n (or the reverse) is written as one pair, area first.
- **`l<n>`/`w<n>`**: taken from a verbatim device parameter `l<n>=<int>` / `w<n>=<int>`, scaled like `l`. That verbatim entry is then blanked.
- **`s`**: ` <n>=` followed by the substrate text, unless the substrate was already written.
- **`r`, `c` with a name**: ` <n>=SI(R)` / ` <n>=SI(C)`, using the raw device value.

After the declared parameters, every verbatim device parameter SHALL be written as ` <text>`. A blanked entry yields a lone space (Q-NETLIST-32).
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:2200-2519, ext2spice/ext2hier.c:227-360)

#### Scenario: Reversed declaration order
- **GIVEN** `parameters nfet l=l w=w` and a device with L=2 and W=8 in scale mode
- **WHEN** parameters are written after the fixed `w=8 l=2`
- **THEN** the appended text is ` w=8 l=2`, with `w` first because the list is reversed

### Requirement: Resistor lines
A `devres` device SHALL be written as follows. Terminal 0 is the resistor body; terminals 1 and 2 are its ends.
- **Unmodeled** (type `None`): `R<id> <t1> <t2> %f` of `R/dscale/m`, then parameters.
- **Modeled**: `R<id> <t1> <t2>`, then for CDL the unnamed value parameters (see below) and the substrate parameter, then ` <type>`, then the geometry:
  - In scale mode, flat: ` w=%g l=%g` of `w·s` and `l·s/dscale`. Hierarchical: ` w=%d l=%d` of the truncated values.
  - Otherwise: ` w=SI(1e-6·w·s·E) l=SI(1e-6·l·s·E/dscale)`.
  - Then parameters, then ` M=%g` if m ≠ 1.
- **Unnamed value parameters**: an `r` or `c` parameter with an empty name SHALL be written as ` SI(value)` right after the terminals. The value is the device resistance, or for `c` the capacitance. Flat mode wrongly writes the resistance for `c` (Q-NETLIST-4).

Without a tee, `dscale = 1`. Half of the body node's capacitance SHALL then be moved to each end node, and the body node's capacitance set to 0. With `resistor tee on`, two lines SHALL be written with `dscale = 2`: `R<n>A <t0> <t1> …` and `R<n>B <t0> <t2> …`. The `rsubckt` class follows Requirement "Subcircuit-class device lines".
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:2521-2582, ext2spice/ext2spice.c:3104-3126, ext2spice/ext2spice.c:3887-3935, ext2spice/ext2hier.c:513-588)

#### Scenario: Unmodeled resistor
- **GIVEN** a `None` resistor of 25.5 Ω between `a` and `b`
- **WHEN** it is written
- **THEN** the line is `R0 a b 25.500000`

#### Scenario: Tee split
- **GIVEN** `resistor tee on` and a modeled `rpoly` resistor with body `m`, ends `a` and `b`, W=2 and L=10, in scale mode
- **WHEN** it is written
- **THEN** the lines are `R0A m a rpoly w=2 l=5` and `R0B m b rpoly w=2 l=5`

### Requirement: Capacitor, diode, BJT and voltage source lines
These classes SHALL be written as follows:
- **Capacitor** (`devcap`): `C<id> <t0> <t1>`. Unmodeled, the value is ` SI(1e-15·m·C)` followed by parameters. Modeled, the output is the unnamed value parameters, the CDL substrate parameter, ` <type>`, the w/l geometry as for resistors with dscale 1, parameters, and ` M=`. `devcaprev` writes `<t1> <t0>`.
- **Diode / pdiode**: `D<id> <t0> <t1 or substrate>`. **Ndiode**: `D<id> <t1 or substrate> <t0>`. Both are followed by the CDL substrate parameter, ` <type>` and parameters, with no `M=`.
- **BJT**: `Q<id> <substrate> <t0> <t1>`, or `<t2>` in place of `<t1>` when t0 and t1 are the same node name. Then the CDL substrate parameter, ` <type>` and parameters.
- **Voltage source**: `V<id> <t1> <t2> 0.0`.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:3127-3332)

#### Scenario: Unmodeled capacitor
- **GIVEN** a `None` capacitor of 12.5 fF between `top` and `bot`
- **WHEN** it is written
- **THEN** the line is `C0 top bot 12.5f`

#### Scenario: Shorted ports voltage source
- **GIVEN** `short voltage` and shorted ports `A` and `B`
- **WHEN** the netlist is written
- **THEN** the line `V0 A B 0.0` appears

### Requirement: Subcircuit-class device lines
Classes subckt, csubckt, veriloga, rsubckt, msubckt and dsubckt SHALL write `X<id>` (or `N<id>` for veriloga), followed by these terminals in order:
- msubckt and dsubckt: first the drain terminal (terminal 2, or terminal 1 when there are only 2).
- All except rsubckt: then terminal 0.
- subckt, csubckt, veriloga and rsubckt: then terminal 1 if present.
- msubckt: then terminal 1 if there are more than 2 terminals.
- rsubckt with tee: then terminal 0, with the length halved.
- Other classes except dsubckt: then terminal 2 if present. For rsubckt this also applies the pi capacitance split.
- Then terminals 3 … n−1.

If the type declares no `s` parameter and the device has a substrate, ` <substrate>` SHALL follow. Then ` /` in CDL, ` <type>`, parameters, and ` M=%g` if m ≠ 1. In hierarchical mode with tee, a second line `X<n>B <t0> <t2>[ <sub>] <type>…` follows, and when a substrate pin is written the body node's ground capacitance is set to 0. In flat mode that second line is never written (Q-NETLIST-5).
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:2950-3100, ext2spice/ext2hier.c:880-1040)

#### Scenario: Generic subcircuit device
- **GIVEN** a `subckt` device `sky130_fd_pr__cap_mim` with terminals `c1`, `c2` and no substrate, in CDL format
- **WHEN** it is written
- **THEN** the line is `X0 c1 c2 / sky130_fd_pr__cap_mim`

### Requirement: Substrate terminal text
The substrate SHALL be written as follows. If the substrate node's local name equals the type's default substrate name (case-insensitive), the local name SHALL be written after these conversions: trailing `!` removed under trim-global, trailing `#` removed under trim-local, `,`→`|`, brackets→`_` under convert-brackets, `=`→`:`. Otherwise the SPICE name of the instance-path-qualified node SHALL be written, or `errGnd!` if it does not resolve. Writing a substrate SHALL mark the node as connected.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:163-200, ext2spice/ext2spice.c:3424-3483)

#### Scenario: Default substrate name trimmed
- **GIVEN** default substrate `VSUBS!`, a device whose substrate node is `VSUBS!`, and SPICE3 format
- **WHEN** it is written
- **THEN** the substrate text is `VSUBS`

### Requirement: Parallel device merging
With `merge conservative` (`-m`) or `merge aggressive` (`-M`), a pre-pass in device output order SHALL compare each device with the previously kept devices, newest first, and merge it into the first one that is parallel. Two devices are parallel only if they have the same class and type and one of these holds:
- **MOSFET/fet/msubckt**: same substrate, same gate, equal L·s, equal W·s (conservative only), and either same source and drain or swapped source and drain.
- **Asymmetric**: as MOSFET, but only with the same source and drain.
- **Devcap/devcaprev**: same terminal 0 and terminal 1. Unmodeled capacitors need equal capacitance (conservative). Modeled capacitors need equal L and W (conservative).

No other class merges. On a merge, the kept device's multiplier m SHALL become:
- **MOS**: `m + W_new/W_kept`.
- **Unmodeled capacitor**: `m + C_new/C_kept`.
- **Modeled capacitor**: `m + (L·W)_new/(L·W)_kept`.

The merged device SHALL be dropped. In flat mode, devices whose source/drain hierarchical-area attributes conflict across different instances are not merged, and a warning is given. `Devs merged: <n>` is informative. In flat mode, devices with exactly one terminal desynchronize the multiplier list (Q-NETLIST-6).
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:4514-4888, ext2spice/ext2hier.c:1361-1484)

#### Scenario: Conservative merge
- **GIVEN** two identical nfets with W=8, L=2 and the same nodes, `merge conservative`, and scale mode
- **WHEN** the netlist is written
- **THEN** one line `M1000 … nfet w=8 l=2 M=2` is written and its areas are divided by 2

#### Scenario: Aggressive merge of unequal widths
- **GIVEN** nfets with W=8 and W=4, the same L and nodes, and `merge aggressive`
- **WHEN** the netlist is written
- **THEN** one device with `w=8` and `M=1.5` is written
