# Extract Devices

## Purpose

Defines how the extractor recognizes devices in the paint of one cell, chooses the device record (model) among the candidates declared in the tech file, assigns gate, terminal and substrate nodes, computes device geometry (length, width, area, perimeter, value) for each device class, and writes `device`, legacy `fet` and `parameters` records. Device declarations are parsed by `extract-tech-section`; record syntax is collected in `extract-file-format`; substrate nodes in `extract-substrate`.

Terminology: the *device type* is a tile type with at least one device record; the *gate node* is the electrical node of the device's own tiles (for capacitors the top plate, for resistors the resistor body); *terminals* are the non-gate connections (source/drain, emitter, plate, resistor ends); records of a type are tried in list order, which is reverse declaration order (last declared first).

## Requirements

### Requirement: Device regions

Device regions SHALL be the connected components of device-type paint under the device connectivity (types declared in the same device line connect; see `extract-tech-section`). Each region SHALL have: a representative tile (the first tile visited), a reference point computed like a node's (lowest plane, minimum left x, then minimum bottom y; see `extract-nodes-and-labels`), a reference plane, and an area equal to the summed area of its tiles on its lowest plane (split tiles: area/2 by integer division). Regions SHALL be processed and written in region-list order: the reverse of discovery order (planes in increasing order, tiles in plane enumeration order; see Q-EXTRACT-5).
Compatibility: exact

(Magic ref: extract/ExtBasic.c:241-250, extract/ExtBasic.c:4976-5050)

#### Scenario: Two-plane device area
- **GIVEN** a device line whose type list contains `mimcap` (plane metal2) only and a mimcap rectangle (0,0)-(10,8)
- **WHEN** the cell is extracted
- **THEN** the region area is 80 and its reference point is (0,0)

### Requirement: Subcell device priority

A device region SHALL be discarded (no record written) if, at the 1×1 square whose lower-left is the lower-left corner of the region's representative tile, any subcell at any depth contains a device type of the region's reference plane.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:251-283, extract/ExtBasic.c:173-182)

#### Scenario: Capacitor split across hierarchy
- **GIVEN** a parent cell painting `mimcap` exactly over a child instance that also contains `mimcap` at the same location
- **WHEN** the parent is extracted
- **THEN** the parent writes no device record for that region; the child's `.ext` contains it

### Requirement: Terminal discovery on the device plane

For each tile of a region, each boundary segment against a neighbor of a different device-connectivity class SHALL be examined, with the current record initially the type's list head. With inside type i, outside type o and o's node N:
Compatibility: exact

1. If o is space, the segment is not a terminal.
2. Otherwise, for the current record, if o is in terminal mask k: for symmetric records (one terminal mask) the terminal index is that of an existing terminal with node N, else the next free index; for asymmetric records the index is k. If that index holds a different node, the next candidate record SHALL be tried (Requirement: Device record selection). On success, the segment length SHALL be added to the terminal's length; its direction SHALL update the terminal's traversal vector (LEFT +len in y, TOP +len in x, RIGHT −len in y, BOTTOM −len in x); the terminal's position (plane, lower-left of the outside tile) SHALL be updated to the minimum by plane, then left x, then (for equal x) bottom y; terminal-attribute labels (`$`) attached to the gate node whose lower-left point lies on the segment SHALL be bound to this terminal; if the index ≥ the record's terminal count, another record with more terminals SHALL be sought (`Warning:  Device has more terminals than defined for type!` when none).
3. Else, if o connects to i in the node connectivity, the segment length SHALL be added to the gate length.
4. Else the next candidate record SHALL be tried; if none, the original record is kept.

Every segment length SHALL be added to the device perimeter, except segments where either side is a contact type and the segment is not on the device's reference plane. Diagonal (split) boundaries add `(int)sqrt(w²+h²)` to the perimeter but are not terminals. Gate-attribute labels (`^`) attached to the gate node whose rectangle overlaps or touches a device tile SHALL be bound to the gate.

(Magic ref: extract/ExtBasic.c:3542-3570, extract/ExtBasic.c:3949-4210, extract/ExtPerim.c:82-137)

#### Scenario: Rectangular MOSFET terminals
- **GIVEN** `device mosfet nfet nfet ndiff None GND!`, connect section linking poly and nfet, nfet (0,0)-(2,4), ndiff (−3,0)-(0,4) labeled `s` and (2,0)-(5,4) labeled `d`, poly (0,−2)-(2,0) and (0,4)-(2,6) labeled `g`
- **WHEN** the cell is extracted
- **THEN** the gate length is 4, the two terminals have length 4 each, and the device perimeter is 12

### Requirement: Device record selection

Candidate records of a device type SHALL be tried cyclically starting after the current record. A candidate SHALL be acceptable only if it agrees with the current record on every property already matched: identifier types (if matched), substrate types (if matched), and, for each already-found terminal index, an equal non-empty terminal mask. While examining tiles the extractor SHALL also:
Compatibility: exact

- Substrate: let U be the union of substrate masks of all records of the type. If U is non-empty and no substrate node is yet known, search under the tile (expanded by 1 unit) on the planes of U's non-space types for a tile of U with a node (warning `Warning:  Split substrate under device at (x y)` when two different nodes are found). If none is found and U contains space, use the cell's substrate node. If a node was found on layer L not in the current record's substrate mask, switch to a candidate containing L (`No matching device for <type> with substrate layer <L>`; device dropped when none). If no node was found and (the style declares substrate types or the record has no substrate name), report `Device <type> does not have a compatible substrate node!` and drop the device.
- Identifier: let I be the union of identifier masks. If I is non-empty, search the tile's area on I's planes for any identifier type. With none found but the current record requiring one, switch to a record with no identifier (`No matching device for <type> with no ID layer`, drop when none); with one found that the current record lacks, switch to a record listing it, or keep the current record with `ID layer <L> on non-matching device <type> was ignored.`

(Magic ref: extract/ExtBasic.c:3457-3540, extract/ExtBasic.c:3577-3720, extract/ExtBasic.c:3311-3430)

#### Scenario: Thick-oxide variant by identifier layer
- **GIVEN** `device mosfet nfet nfet ndiff pwell VSUB` declared before `device mosfet nfet_hv nfet ndiff pwell VSUB +thkox`, and an nfet covered by `thkox`
- **WHEN** the cell is extracted
- **THEN** the record uses model `nfet_hv`; an nfet without `thkox` uses model `nfet`

### Requirement: Terminals off the device plane

After the perimeter scan, for each terminal index k < the record's terminal count not yet found, whose mask contains no type on the device's reference plane or contains space: search under the representative tile (expanded by 1) for a node of a mask type, then (if the region has more area than that tile) under every tile of the region; if none and the mask contains space, the terminal SHALL be the cell's substrate node; if none otherwise, the next candidate record SHALL be tried. A found terminal's length SHALL be 0, its area and perimeter SHALL be those of the connected same-plane material of mask types found under the device (0,0 for the substrate), and its sharing count 1. A mask with only types on the device plane that is not found SHALL also cause the next candidate to be tried. A record whose remaining terminal masks are empty while terminals remain unfound (e.g., tied source/drain) SHALL be accepted.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:2405-2555)

#### Scenario: Capacitor bottom plate
- **GIVEN** `device capacitor None mimcap metal2 10 2`, mimcap (0,0)-(10,10) on plane mimcap over a metal2 node (−1,−1)-(11,11)
- **WHEN** the cell is extracted
- **THEN** the terminal is the metal2 node with length 0, area 144 and perimeter 48

### Requirement: Terminal area, perimeter and sharing

For each terminal found on the device plane, starting from its first terminal tile, the extractor SHALL flood the terminal's same-plane material (global connectivity, restricted to tiles of the terminal node) and compute: area = sum of tile areas (split tiles area/2), perimeter = sum of lengths of tile sides against types that do not connect to the terminal type (including device types; split tiles add `(int)sqrt(w²+h²)` for the diagonal and skip the two sides on the other half), and sharing = 1 + the number of distinct other device regions whose device-type tiles border that material. The written terminal area and perimeter SHALL be `area / sharing` and `perim / sharing` (integer division).
Compatibility: exact

(Magic ref: extract/ExtBasic.c:2349-2390, extract/ExtBasic.c:3800-3947)

#### Scenario: Shared diffusion between two fingers
- **GIVEN** two nfet regions with different gate nodes sharing a middle ndiff region of area 24 and perimeter 20
- **WHEN** the cell is extracted
- **THEN** each device writes `12,10` for that terminal

### Requirement: Missing terminals and rejected devices

If fewer terminals than the record's count were found, the message `device missing N terminal(s)` (with `;\n connecting remainder to node <name>` when at least one was found) SHALL be given as feedback when the `fets` warning is enabled, and the missing terminals SHALL be copies of the last found node with length, area, perimeter and sharing 0. A device with no terminal found SHALL NOT be written. A device with no acceptable record SHALL be reported as `Warning:  No matching extraction type for device at (x y)` (representative tile lower-left) and not written. A record whose model name is `Ignore` SHALL produce no output.
Compatibility: exact (output); informative (messages)

(Magic ref: extract/ExtBasic.c:2557-2610, extract/ExtBasic.c:2636-2640)

#### Scenario: Transistor with one diffusion side
- **GIVEN** a symmetric mosfet record and an nfet with ndiff only on its left
- **WHEN** the cell is extracted
- **THEN** both terminals of the written record name the left diffusion node, the second with `... "<node>" 0 0`

### Requirement: Device record header and substrate field

Each written device SHALL start with `device <class> <model> <x> <y> <x+1> <y+1>` where (x,y) is the region reference point and class is one of `fet, mosfet, asymmetric, bjt, devres, devcap, devcaprev, vsource, diode, pdiode, ndiode, subckt, rsubckt, msubckt, csubckt, dsubckt, veriloga`; records of the legacy `fet` class SHALL instead start with `fet <model> <x> <y> <x+1> <y+1> <area> <perimeter> "<substrate>"`. The substrate field SHALL be the name of the found substrate node when the record has substrate types and one was found; otherwise the record's substrate name; when that is absent, `None` where the class requires the field.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:2612-2667, extract/ExtBasic.c:54-61)

#### Scenario: Legacy fet line
- **GIVEN** `fet nfet ndiff 2 nfet GND! 0 0` and a 2×4 nfet with perimeter 12
- **WHEN** the cell is extracted
- **THEN** the line begins `fet nfet 0 0 1 1 8 12 "GND!"`

### Requirement: MOSFET-family length and width

For classes mosfet, asymmetric, bjt, subckt, msubckt and veriloga, with n = number of terminal slots (including copied missing ones), W0 = sum of terminal lengths over non-null terminals, G = gate length:
Compatibility: exact

1. While n > 0 and terminal n−1 has a node with length, area and perimeter all 0, decrement n.
2. If n > 0: `W = W0 / n`; `L = G / n` if n > 1; else `L = G` if G < W, else `L = (G − W) / 2` (integer division). If n = 1 and L = 0 and G = 0: L = x-extent and W = y-extent of the bounding box of the region's tiles.
3. If n = 0: L = W = 0 and, with warnings enabled and a non-zero terminal count, feedback `Could not determine device boundary`.
4. If n > 0, the effective-dimension algorithm (Requirement: Effective length and width of non-rectangular gates) SHALL be run with the terminal boundary lists of this device and n regions; when it yields L > 0 and W > 0 those values replace L and W.

Classes mosfet, asymmetric and bjt SHALL write ` <L> <W>` after the header; the others write no positional dimensions.

(Magic ref: extract/ExtBasic.c:2668-2846)

#### Scenario: Rectangular MOSFET record
- **GIVEN** the layout of Requirement: Terminal discovery on the device plane
- **WHEN** the cell is extracted
- **THEN** the record is `device mosfet nfet 0 0 1 1 2 4 "GND!" "g" 4 0 "s" 4 12,14 "d" 4 12,14`

### Requirement: Boundary lists for dimension computation

For dimension computation the device perimeter SHALL be re-enumerated into one list of segments per terminal: a segment is added to terminal k's list when its outside type is in one of the record's terminal masks (outside space maps to the substrate node) and its outside node is terminal k's node, or, when the outside type is in no terminal mask but some terminal mask has no type on the device plane, to the list of that off-plane terminal. A new segment collinear with, in the same direction as, and abutting an existing segment of the list SHALL extend it (joining two segments when it bridges them). If the last list is empty, the segments of list 0 SHALL be split: the first segment moves to the last list and segments sharing endpoints with the growing chain move after it, repeatedly, until no more connect.
Compatibility: exact

For non-annular resistors (Requirement: Resistor dimensions and value), the list SHALL instead collect, into list 0, segments whose outside type is neither a device-connected type nor in terminal mask 0, then be split as above.

(Magic ref: extract/ExtBasic.c:4229-4587, extract/ExtBasic.c:1581-1695)

#### Scenario: MOS capacitor
- **GIVEN** a symmetric mosfet record and an nfet whose left and right diffusions are the same node (tied)
- **WHEN** dimensions are computed
- **THEN** list 0 contains both sides, list 1 is empty and is filled by splitting list 0 into the two disconnected sides

### Requirement: Effective length and width of non-rectangular gates

Given n lists, the algorithm SHALL: choose p = the first list with the most segments; set L = 0, W = 0; for each Manhattan segment s of list p with opposite direction o:
Compatibility: exact

1. `loc = min` over segments t of direction o in the other lists (all lists when n = 1, excluding p when n > 1) lying on the far side of s (LEFT: t.xbot > s.xbot, d = t.xbot − s.xbot; RIGHT: t.xtop < s.xtop, d = s.xtop − t.xtop; TOP: t.ytop < s.ytop, d = s.ytop − t.ytop; BOTTOM: t.ybot > s.ybot, d = t.ybot − s.ybot) and whose perpendicular span satisfies `t.lo < s.hi + d` and `t.hi > s.lo − d`, of d increased by the perpendicular gap when t lies beyond s's span. If none, skip s.
2. If L = 0 set L = loc; else if loc ≠ L: if loc < L then `W = W·loc / L` and L = loc; note "multiple lengths".
3. Over the same candidate segments t with exact distance `loc`: `sp = min(s.hi, t.hi)`, `sn = max(s.lo, t.lo)`, `w = sp − sn`; if w > 0 add 2w to `lw`; if `w <= −loc` skip t; add `min(max(s.hi,t.hi) − max(sp,sn), loc)` and `min(min(sp,sn) − min(s.lo,t.lo), loc)` to `cw` when positive.
4. `lw += (int)(0.5 + cw · chop)` (single precision; chop = corner-chop of the device type); if loc ≠ L then `lw = lw · L / loc`; `W += lw`.

Then W SHALL be increased by `Σ (int)(0.5 + sqrt(dx² + dy²))` over all diagonal segments of all lists. If L > 0 and W > 0 the result SHALL be L and `W >> 2` when n = 1, else `W >> 1`; and `Device has multiple lengths:  scaling all widths to length L` SHALL be printed when step 2 found differing lengths. Otherwise the caller's estimates stand. All divisions are C integer divisions.

(Magic ref: extract/ExtBasic.c:1259-1579)

#### Scenario: Rectangle
- **GIVEN** a 2×4 gate with source on the left (list 0) and drain on the right (list 1)
- **WHEN** the algorithm runs with n = 2
- **THEN** loc = 2, lw = 8, W = 8 >> 1 = 4, L = 2

#### Scenario: Annular gate (verify against Magic)
- **GIVEN** a square ring gate of inner side 4 and ring width 2 with the inner diffusion as terminal 0 and the outer diffusion as terminal 1, chop 1.0
- **WHEN** the algorithm runs
- **THEN** L = 2 and W = (4 inner sides · 2·4 + corner extensions 8·2) >> 1 = 24

### Requirement: Resistor dimensions and value

For classes resistor (`devres`) and rsubcircuit, with P = device perimeter and n = number of terminal slots: if n = 0, L = W = 0. Else if the region has more than one tile: the device is *annular* when some terminal has a zero traversal vector and is not the same node as the previous terminal; annular devices SHALL use terminal boundary lists with the effective algorithm assigning (L, W); non-annular devices SHALL use side boundary lists (Requirement: Boundary lists for dimension computation) with the algorithm's outputs swapped (its length is W, its width is L); an empty list gives L = W = 0 with feedback `Could not determine resistor boundary`. Else (single tile): `W = (Σ terminal lengths) >> 1` and `L = (P − Σ terminal lengths) >> 1`, summing until the first zero-length terminal.
Compatibility: exact

If W ≠ 0 the value SHALL be `R = sheet(t) · L / W` (double, mΩ; t the device type, sheet from its `resist` class) with feedback `Resistor has N terminals: extracted ...` when more than 2 terminals and warnings on; else R = 0 with feedback `Resistor has zero width`. Output after the header: rsubcircuit — nothing; model ≠ `None` — ` <L> <W>`; model `None` — ` <R/1000>` in `%g` (ohms). Then parameters; then the substrate field: rsubcircuit always (`None` when absent); models ≠ `None` only when a substrate name exists; model `None` never.

(Magic ref: extract/ExtBasic.c:2988-3120)

#### Scenario: Poly resistor without model
- **GIVEN** `resist rpoly 2000`, `device resistor None rpoly poly`, rpoly (0,0)-(10,2) with poly ends (−2,0)-(0,2) labeled `a` and (10,0)-(12,2) labeled `b`, plane `active`
- **WHEN** the cell is extracted
- **THEN** the record is `device devres None 0 0 1 1 10 "active_0_0#" 0 0 "a" 2 4,8 "b" 2 4,8`

### Requirement: Capacitor dimensions and value

For classes capacitor (`devcap`), capreverse (`devcaprev`) and csubcircuit: if the model is `None`, the record SHALL write ` <C/1000>` in `%g` (fF) with `C = gccap · area + gscap · perimeter` (aF, device area and perimeter) and no parameters or substrate field. Otherwise, with n = leading non-null terminals: n = 0 SHALL give feedback `Capacitor has zero size` and write ` 0 0` (see Q-EXTRACT-8); n > 0 SHALL compute L = y-extent and W = x-extent of the bounding box of the terminal-0 boundary list (when empty: `extract:  Can't get capacitor L and W` and L = W = (int)(sqrt(area of terminal 0) + 0.5)). Non-csubcircuit records SHALL write ` <W> <area/W>` when L·W exceeds the device area (feedback `L,W estimated for non-rectangular capacitor.` with warnings on), else ` <L> <W>`; then parameters; then the substrate field (csubcircuit always, `None` when absent; others only when a name exists).
Compatibility: exact

(Magic ref: extract/ExtBasic.c:3121-3215, extract/ExtBasic.c:1207-1235)

#### Scenario: MiM capacitor without model
- **GIVEN** `device capacitor None mimcap metal2 10 2`, mimcap (0,0)-(10,10) labeled top `t`, over metal2 node `b` (−1,−1)-(11,11)
- **WHEN** the cell is extracted
- **THEN** the record is `device devcap None 0 0 1 1 0.6 "t" 0 0 "b" 0 144,48` (C = 2·100 + 10·40 = 600 aF)

### Requirement: Diode records

For classes diode, pdiode, ndiode and dsubcircuit the record SHALL have no positional dimensions. If the selected record has an output parameter whose key begins with `l` or `w`, L and W SHALL be computed as for capacitors from the terminal-0 boundary list; then parameters; then the substrate field only when a substrate name exists.
Compatibility: exact

See Q-EXTRACT-13.

(Magic ref: extract/ExtBasic.c:2930-2987)

#### Scenario: Diode with area parameter
- **GIVEN** `device diode dn ndiff pwell a=area p=pj` and an ndiff region in pwell of area 20 and perimeter 18
- **WHEN** the cell is extracted
- **THEN** the record contains ` a=20 p=18` before the terminals

### Requirement: Device parameter values

After the class-specific fields, each parameter of the selected record with an output name SHALL be written in list order, using its key characters K (first character preserved in case, compared case-insensitively):
Compatibility: exact

| key | written |
|---|---|
| `a`, `a0` | ` K0=<device area>` (only the first key character is written) |
| `p`, `p0` | ` K0=<device perimeter>` |
| `l`, `l0` | ` K0=<L>` |
| `lN` (N=1..9) | ` KN=<area of terminal N / W>` (0 if W = 0) |
| `w`, `w0` | ` K0=<W>` |
| `wN` (N=1..9) | ` KN=<(int)(0.25·(sqrt(p² − 16a) + p))>` with a, p terminal N's area and perimeter; this value also replaces W for later parameters |
| `c` | ` K0=<gccap·area + gscap·perimeter>` in `%g` (aF) |
| `r` | ` K0=<R>` in `%g` of a single-precision value, see below |
| `s`, `x`, `y`, `aN`, `pN` | nothing |
| any other | ` K0=` (empty value) |

For `r`: if the record has a `devresist` value under `area`, `R = area_res / device_area` (integer division), combined in parallel with `perimeter_res / device_perimeter` when present (`R = (int)(1/(1/Rp + 1/R))`); otherwise `R = (int)(sheet(type) · L / W)` plus `terminal_res · W` when a `terminal` value exists. Then, for every gate-attribute label (`^`) bound to this device whose text contains `=`, the text without its last character SHALL be written preceded by a space, and the label SHALL no longer be treated as a gate attribute.

See Q-EXTRACT-15 for zero width or area.

(Magic ref: extract/ExtBasic.c:1845-2042)

#### Scenario: Width and length parameters
- **GIVEN** `device msubcircuit nfet_01v8 nfet ndiff ndiff None VSUB l=l w=w` and a 2×4 gate
- **WHEN** the cell is extracted
- **THEN** the record reads `device msubckt nfet_01v8 0 0 1 1 l=2 w=4 "VSUB" ...`

#### Scenario: Gate attribute parameter
- **GIVEN** a gate label `mult=2^` touching the gate tile
- **WHEN** the cell is extracted
- **THEN** ` mult=2` is appended to the parameter list and the gate terminal shows no attribute

### Requirement: Model selection by parameter ranges

After dimensions are computed, the selected record SHALL be checked against its parameter ranges (min ≤ max): `a`/`a0` against L·W, `aN` against terminal N's area, `p`/`p0` against 2(L+W), `pN` against terminal N's perimeter, `l`/`l0` against L, `lN` against terminal N's length, any `w*` against W. If any check fails, the next record in list order SHALL be tried if it has the same class, terminal count, terminal masks, substrate types and identifier types; the first passing record SHALL be used; if the list ends or an incompatible record is reached, the original record SHALL be used with `Device parameters do not match any extraction model.`
Compatibility: exact

(Magic ref: extract/ExtBasic.c:2060-2200)

#### Scenario: Two models by length
- **GIVEN** records declared `device mosfet nshort nfet ndiff None GND! l<5` then `device mosfet nlong nfet ndiff None GND! l>=5` (so `nlong` is tried first)
- **WHEN** a gate of length 2 is extracted
- **THEN** `nlong` fails its range and `nshort` is used

### Requirement: Terminal output and ordering

After the parameters and substrate field, the record SHALL list the gate as ` "<gate node name>" <G>` followed by its attributes or ` 0`, then each terminal k as ` "<node name>" <len_k>` followed by its attributes and, when area and perimeter are both non-zero, `<sep><area/shared>,<perim/shared>`; a terminal with neither attributes nor area/perimeter writes ` 0`. Attributes SHALL be written as `"<text-without-last-char>"`, the first preceded by a space and the following by commas; `sep` is a space if no attribute was written, else a comma. For classes fet, mosfet and msubcircuit with a single (symmetric) terminal mask, terminals SHALL first be sorted ascending by (plane, x, y) of their terminal positions (stable bubble sort; equal positions are reported as `Extract error:  Duplicate tile position, ignoring` and not swapped), carrying attributes with them. Asymmetric and other classes keep discovery order.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:3216-3245, extract/ExtBasic.c:4589-4653, extract/ExtBasic.c:1115-1199)

#### Scenario: Source attribute
- **GIVEN** a terminal-attribute label `S$` attached to the gate node with its lower-left point on the left diffusion boundary
- **WHEN** the cell is extracted
- **THEN** that terminal is written as `"s" 4 "S",12,14`

### Requirement: Parameters records

Before the node records, for every device type present among the cell's kept device regions (and the type named by the cell's `device` property), for every record of that type except model `Ignore`, that has at least one output-named parameter, the extractor SHALL write `parameters <model>` followed, for each named parameter in list order, by ` K=NAME`, ` K=NAME*<scale %g>` when scale ≠ 1, or ` K=NAME<offset %+d>` when the offset ≠ 0 (K one or two key characters).
Compatibility: exact

(Magic ref: extract/ExtBasic.c:1697-1843)

#### Scenario: Scaled parameter
- **GIVEN** `device msubcircuit sky130_fd_pr__nfet_01v8 nfet ndiff ndiff pwell VSUB l=l w=w*1e6`
- **WHEN** a cell with an nfet is extracted
- **THEN** the line `parameters sky130_fd_pr__nfet_01v8 l=l w=w*1e+06` is written

### Requirement: Device cell property

When the cell has a property `device`, all node resistances and capacitances SHALL be zeroed and coupling records suppressed. If the value starts with `primitive `, `parameters :<cellname> <rest>` SHALL be written and, after the nodes, the line `primitive`. Otherwise the value SHALL be parsed as `CLASS MODEL LLX LLY URX URY [D1 D2] REST...`: CLASS by prefix lookup in the class-name table, coordinates as integers multiplied by units-per-lambda (truncated to int), URX/URY incremented by 1 when not greater than LLX/LLY; D1 D2 present for classes fet (area·upl², perimeter·upl), mosfet/asymmetric/bjt (width·upl, length·upl), and devres/devcap/devcaprev with model ≠ `None` (as width/area·upl^k, length/perimeter·upl); the line `device <class> <model> <llx> <lly> <urx> <ury>` (or `fet ...`) plus ` D1 D2` and the remaining tokens verbatim SHALL replace normal device output. Parse errors SHALL report `Extract error:  "device" property has unknown device type: <t>` or `Extract error:  "device <class>" property syntax error` and normal device output SHALL be used.
Compatibility: exact

D1 and D2 SHALL be written in the property's own token order, in both `magic-compat` settings (Q-EXTRACT-16, resolution: replicate): Magic's code reads them as width then length for mosfet, asymmetric, bjt and modelled resistor, and as area then perimeter for fet and modelled capacitor/caprev, and there is no reordering to the `L W` order of normal device records. Lengths, widths and perimeters SHALL be scaled by units-per-lambda (upl) and areas by upl², each truncated to int. After scaling, URX SHALL be incremented when URX ≤ LLX, and URY when URY ≤ LLY.

(Magic ref: extract/ExtBasic.c:353-556, extract/ExtBasic.c:1716-1745)

#### Scenario: Primitive cell
- **GIVEN** a cell `cap1` with property `device` = `primitive m=1`
- **WHEN** it is extracted
- **THEN** its `.ext` contains `parameters :cap1 m=1` and a `primitive` line, and every node record has R 0 and C 0

#### Scenario: Property dimensions kept in token order
- **GIVEN** units-per-lambda 1.5 and a cell with property `device` = `mosfet nfet 0 0 2 2 3 5 VSS`
- **WHEN** it is extracted
- **THEN** the line `device mosfet nfet 0 0 3 3 4 7 VSS` is written: coordinates and the two dimensions scaled by 1.5 and truncated, kept in the property's order
