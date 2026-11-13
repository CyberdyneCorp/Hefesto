# extresist-output

## Purpose

This capability defines the outputs of the `extresist` command (capability `extresist-command`):
- the `.res.ext` overlay file: grammar, naming of new nodes, record order and numeric formatting
- how netlisters consume the overlay, which ties into capabilities `ext-flattening-read` and `netlist-spice-devices`
- the FastHenry `.fh` geometry file
- the on-screen geometry centerlines

The `.res.lump` format is defined in capability `extresist-command`.

## Requirements

### Requirement: Overlay file grammar and order
`<cell>.res.ext` SHALL follow this grammar:

```ebnf
resext   = scale , { netblock } , { devrec } ;
scale    = "scale " , int(resistScale) , " " , int(capScale) , " " , g(unitsPerLambda) , NL ;
netblock = [ "killnode " , q(net) , NL ]
           , { "rnode " , q(name) , " 0 " , g(cap) , " " , int(x) , " " , int(y) , " 0" , NL }
           , [ "resist " , q(net) , " " , q(name) , " 0.0" , NL ]      (* orphan patch *)
           , { "resist " , q(name) , " " , q(name) , " " , g(R) , NL } ;
devrec   = [ "device " ] , class , " " , type , " " , int(x) , " " , int(y) , " " , int(x+1) , " " , int(y+1) , " "
           , [ " " , int , " " , int ] , " " , q(sub) , [ " " , q(g) , " " , int(2L) , " " , attr ]
           , [ " " , q(s) , " " , int(W) , " " , attr ] , [ " " , q(d) , " " , int(W) , " " , attr ] , NL ;
q(x)     = '"' , x , '"' ;
```

`g()` is C `%g`. The values are:
- **rnode capacitance** `cap = C_node / capScale`, where C_node is the node's distributed capacitance in aF. For the node that keeps the original net name, the net's original `node` capacitance is subtracted first.
- **resistance** `R = R_mΩ / resistScale`.
- **net blocks** are written in net processing order. Within a block, rnodes follow the network node list and resistors follow the network resistor list.
- **device records** follow the scale line's net blocks and are written in reverse `.ext` reading order. A device record is written only for devices that have at least one terminal renamed. It starts with `device ` unless the class is `fet`.
- **device fields**:
  - The geometry pair is area and perimeter for `fet`, and length and width for mosfet/asymmetric/bjt. Resistor, capacitor and caprev records depend on the `magic-compat` setting (Q-NETLIST-39, resolution: compat): with it ON (the default) they carry no value or L/W fields and are written with the gate, source and drain terminal layout above, as in Magic, so the reader rejects a resistor record (`… overlaps device with incompatible number of terminals …`) and its rewiring is lost; with it OFF they carry the leading fields the extractor writes (the value, or `L W` when the device has a model) and their two terminals in the extractor's layout (capability `extract-file-format`), so the overlay replaces the original device's terminals. Other classes carry no geometry pair in both settings. The reader is the same in both settings.
  - The substrate is the renamed substrate node if any, else the default substrate name, else `None`.
  - The drain is written only if the device has more than one source/drain terminal.
  - An empty attribute is written as `0`.
Compatibility: exact
(Magic ref: resis/ResRex.c:1385-1415, resis/ResPrint.c:49-335, resis/ResPrint.c:109-185, resis/ResRex.c:2093-2240, extflat/EFbuild.c:1082-1110, extflat/EFbuild.c:1160-1167)

#### Scenario: Simple overlay
- **GIVEN** resistScale 1 and capScale 1, a net `a` split into the terminal node `a.t0` at (0,0) with 1000 aF and the internal node `a.n0` at (50,0) with 500 aF, joined by 2000 mΩ, with no port named `a`
- **WHEN** the overlay is written
- **THEN** the block is `killnode "a"`, `rnode "a.t0" 0 1000 0 0 0`, `rnode "a.n0" 0 500 50 0 0`, `resist "a.t0" "a.n0" 2000`, with the rnode lines in node-list order

#### Scenario: MOSFET device record
- **GIVEN** an nfet at (10,20) with L=2, W=8, substrate `GND`, gate `in`, source `out.t0` and drain `x`, with no attributes
- **WHEN** its record is written
- **THEN** the line is `device mosfet nfet 10 20 11 21  2 8 "GND" "in" 4 0 "out.t0" 8 0 "x" 8 0`, with two spaces before `2 8`

#### Scenario: Resistor device overlay with magic-compat ON
- **GIVEN** a modelled `resistor` device whose end on net `a` is renamed `a.t0` by the overlay
- **WHEN** the `.res.ext` file is written and read back by `ext2spice extresist on` with `magic-compat` ON
- **THEN** the device record carries no `L W` fields, the reader rejects it, and the device keeps its original terminals, as in Magic

#### Scenario: Resistor device overlay with magic-compat OFF
- **GIVEN** the same device and overlay
- **WHEN** the `.res.ext` file is written and read back with `magic-compat` OFF
- **THEN** the device record carries `L W` and the two terminals in the extractor's layout, and the netlisted resistor connects `a.t0`

### Requirement: Naming of network nodes
Let net′ be the net name with one trailing `!` or `#` removed. Nodes SHALL be named as follows:
- **Device terminals.** Visit the net's devices in gate-sorted order. Each device terminal on the net SHALL be named `net′.t<k>`, where k is a per-net counter starting at 0. A terminal node that already carries a name keeps it, and the counter is not advanced for it. The device's terminal is redirected to the new name.
- **Other unnamed nodes.** These SHALL be named `net′.n<k>`, k from 0, in network node-list order.
- **Unnamed resistor ends.** Any resistor end still unnamed SHALL be named `net′.r<k>`.
- **Port and label nodes.** These keep their label text.
- **Drivers and sinks.** They receive no special names (Q-NETLIST-22).

The original net SHALL be killed unless some network node carries exactly the original name. If any device terminal could not be found, the error `Missing <terminal> connection of device at (<x> <y>) on net <n>` is printed. In that case the net is not killed. Instead, after the rnode lines, the patch `resist "<net>" "<first node>" 0.0` is written with the warning `Orphaned node "<net>" arbitrarily attached to "<first>"`.
Compatibility: exact
(Magic ref: resis/ResRex.c:1516-1890, resis/ResPrint.c:213-335)

#### Scenario: Global net name trimmed
- **GIVEN** the net `vdd!` with two unnamed internal nodes
- **WHEN** names are assigned
- **THEN** they are `vdd.n0` and `vdd.n1`

### Requirement: Consumption by netlisters
When a netlister reads with overlays (`extresist on`), it SHALL apply the overlay records with the semantics of capability `ext-flattening-read`:
- `killnode` removes the original node and every device and parasitic attached to it.
- Each `rnode` becomes a node with capacitance `C × capScale`.
- Each `resist` becomes an explicit resistor of `(float)(0.5 + resistScale × R)` mΩ with `magic-compat` ON, and `(float)(resistScale × R)` mΩ with it OFF (Q-NETLIST-12, resolution: compat).
- Device records are keyed by position and type and replace the terminals of the matching original device.

In SPICE output the resistors SHALL appear as parasitic resistor lines `R<k> <n1> <n2> <%g of mΩ/1000>`. With `magic-compat` ON, because of the +0.5 offset, values carry a visible 0.0005 Ω offset, as in Magic; with it OFF they do not.
Compatibility: exact
(Magic ref: extflat/EFread.c:509-520, extflat/EFread.c:596-605, extflat/EFread.c:626-635, ext2spice/ext2spice.c:4041-4060)

#### Scenario: Overlay resistor in SPICE
- **GIVEN** resistScale 1 and the overlay line `resist "a.t0" "a.n0" 2000`
- **WHEN** `ext2spice extresist on` writes the netlist
- **THEN** with `magic-compat` ON the netlist contains the line `R0 a.t0 a.n0 2.0005`
- **AND** with `magic-compat` OFF it contains the line `R0 a.t0 a.n0 2`

### Requirement: FastHenry geometry file
With `fasthenry`, the file `<cell>.fh` SHALL be written. All lengths are multiplied by `o` = the CIF output scale for µm. Its contents are:

1. **Header.**
   - `* FastHenry output for magic cell <cell>`, then an empty line
   - `.Units um`
   - `.Default rho=0.02 nhinc=3 nwinc=3 rh=2 rw=2`, then an empty line
   - `* Reference plane (substrate, ground)`
   - `Gsub x1=%1.2f y1=%1.2f z1=0 x2=%1.2f y2=%1.2f z2=0`, using the bounding box corners lower-left and lower-right
   - `+ x3=%1.2f y3=%1.2f z3=0`, the upper-right corner
   - `+ thick=0.1 seg1=<sx> seg2=<sy>`, where `sx = max(4, width/20)` and `sy = max(4, height/20)`, with integer division
   - `+ Ngp (%1.2f,%1.2f,0)`
   - an empty line, then `Nsub x=%1.2f y=%1.2f z=0`
   - `.Equiv Nsub Ngp`

2. **Per net.**
   - An empty line, then `* List of nodes in network`.
   - One line per node: `N<name or id> x=%1.2f y=%1.2f z=%1.2f`. Unnamed nodes get ids from a counter starting at 1 whose scope depends on the `magic-compat` setting (Q-NETLIST-40, resolution: compat): with it ON (the default) it restarts for each net, as in Magic; with it OFF it runs across the whole `.fh` file, so every node name is unique. z is the layer height times o, or 0.1 × the plane order when the height is 0. For a node that touches only contact resistors Magic uses an unassigned height (Q-NETLIST-27).
   - Multi-via contacts add per-via nodes `N<n>_<i>_<j>` and an `.equiv` line.
   - An empty line, `* List of externally-connected ports`, and then `.external` followed by the first two named nodes. Further named nodes go on a line `* Warning! external nodes not recorded:`. Then two newlines.
   - `* Segments connecting nodes in network`, then `E<k> N<a> N<b> w=%1.2f h=%1.2f` per resistor. Contact resistors use the CIF via width/100; others use rr_width × o and thickness × o, with thickness 0.05 when the thickness is 0. Multi-via contacts produce one segment per via. k is a counter starting at 1, per net with `magic-compat` ON and per file with it OFF (Q-NETLIST-40). The geometry output uses the same counters.

3. **Footer.**
   - an empty line, then `.freq fmin=%2.1g fmax=%2.1g`
   - an empty line, then `* Order of arguments to SPICE subcircuit call:`
   - `* <index> <text>` per port label
   - an empty line, then `.end`

Port labels that match the first two external nodes SHALL be renumbered sequentially, which modifies the cell.
Compatibility: exact
(Magic ref: resis/ResPrint.c:409-697, resis/ResPrint.c:699-755, resis/ResRex.c:1077, resis/ResRex.c:1395-1440)

#### Scenario: Reference plane segments
- **GIVEN** a cell with bounding box (0,0)–(40,200)
- **WHEN** the reference plane is written
- **THEN** the segment line is `+ thick=0.1 seg1=4 seg2=10`

#### Scenario: Unnamed node ids across nets
- **GIVEN** `extresist fasthenry` on a cell with two processed nets, each with two unnamed nodes and one segment
- **WHEN** the `.fh` file is written
- **THEN** with `magic-compat` ON both nets use nodes `N1`, `N2` and segment `E1`
- **AND** with `magic-compat` OFF the second net uses `N3`, `N4` and `E2`

### Requirement: Geometry centerlines
With `geometry`, no file SHALL be written. For each non-contact resistor of each processed net, a line element SHALL be added to the layout window from the first node's location to the second node's location. It is named `N<n1>_<n2>`, where unnamed nodes take sequential ids, and drawn in the first yellow style. The node coordinates are first aligned to the resistor centerline: y for east-west resistors, x for north-south resistors, skipping diagonal ones. Without a layout window, nothing SHALL be drawn.
Compatibility: behavioral
(Magic ref: resis/ResPrint.c:757-814, resis/ResRex.c:2034-2091)

#### Scenario: Centerline element
- **GIVEN** a net with one east-west resistor between unnamed nodes at (0,2) and (100,2)
- **WHEN** `extresist geometry` runs
- **THEN** one line element named `N1_2` from (0,2) to (100,2) is added
