# ext-flattening-read

## Purpose

This capability defines how the netlisting front end reads a tree of `.ext` files (and their optional `.res.ext` overlays) into an in-memory hierarchical circuit model of cell definitions, nodes, devices, connections, couplings and resistors. The model is the input to flattening (capability `ext-flattening-hierarchy`), to name handling (capability `ext-flattening-names`), and to every netlister (`netlist-spice-*`, `netlist-sim`, `netlist-extcheck`). This capability owns the read-side semantics of each `.ext` keyword. It does not own the writer-side grammar of `.ext`, which belongs to capability `extract-file-format`; the EBNF here is only the subset the reader depends on.

Units used throughout: lengths are in `.ext` internal units, capacitance in attofarads (aF), and resistance in milliohms (mΩ), unless stated otherwise.

## Requirements

### Requirement: Locating and recursively reading .ext files
The reader SHALL open `<name>.ext` for the root cell. It SHALL try the following locations in order and use the first file found:
1. The colon-separated search path given with `-p`, if one was given.
2. The directory of the file the layout cell was loaded from, if the cell is loaded in the editor.
3. The editor's cell search path, but only if no `-p` path was given.

A trailing `.ext` on the root name given by the user SHALL be removed before the search. After reading a file completely, including its `.res.ext` overlay when that applies, the reader SHALL read every subcell definition named by a `use` record that has not been read yet, using the same three locations. Subcells SHALL be read in the iteration order of the parent's use table (see Requirement "Container iteration order" in capability `ext-flattening-hierarchy`). Each definition SHALL be read at most once per run. If a file cannot be opened, the read SHALL fail with the message `Cannot read extract file <name>.ext` (informative). Errors in subcells SHALL make the overall result a failure, but reading SHALL continue.
Compatibility: behavioral
(Magic ref: extflat/EFread.c:120-200, extflat/EFread.c:704-760, extflat/EFargs.c:330-345)

#### Scenario: Root name given with extension
- **GIVEN** the user runs the SPICE netlister on `inv.ext`
- **WHEN** the reader resolves the root
- **THEN** it searches for `inv` with suffix `.ext`, not for `inv.ext.ext`

#### Scenario: Shared subcell read once
- **GIVEN** `top.ext` contains `use inv u1 ...` and `use inv u2 ...`
- **WHEN** the tree is read
- **THEN** `inv.ext` is opened exactly once

### Requirement: Line tokenization
The reader SHALL split each physical line into tokens as follows:
- A line whose first character is `#` is a comment and is skipped.
- A line that ends with a backslash continues on the next physical line. The backslash is removed and the next line is appended directly.
- Blank lines and lines that contain only whitespace are skipped.
- Tokens are separated by whitespace. A double quote starts a quoted section, which may contain whitespace, and the next double quote ends it. The quote characters themselves are removed from the token.
- Any other character preceded by a backslash is taken literally.

Lines have no length limit. At most 128 tokens per line are supported.
Compatibility: exact
(Magic ref: extflat/EFread.c:770-885)

#### Scenario: Quoted node name
- **GIVEN** the line `equiv "a b" c`
- **WHEN** it is tokenized
- **THEN** the tokens are `equiv`, `a b`, `c`

#### Scenario: Continuation line
- **GIVEN** the two physical lines `node n1 0 10 \` and `0 0 metal1 0 0`
- **WHEN** they are tokenized
- **THEN** a single logical line with 9 tokens results

### Requirement: Keyword recognition
The first token of a line SHALL be matched case-insensitively against this keyword table. A unique prefix is accepted, and an exact match always wins:

| keyword | min tokens | | keyword | min tokens |
|---|---|---|---|---|
| abstract | 0 | | port | 8 |
| adjust | 4 | | primitive | 0 |
| attr | 8 | | resist | 4 |
| cap | 4 | | resistclasses | 1 |
| connect | 7 | | rnode | 5 |
| device | 11 | | scale | 4 |
| distance | 4 | | subcap | 3 |
| equiv | 3 | | substrate | 3 |
| fet | 12 | | tech | 2 |
| killnode | 2 | | timestamp | 2 |
| merge | 3 | | use | 9 |
| node | 7 | | version | 2 |
| parameters | 3 | | style | 2 |

An unknown keyword SHALL be reported as `Unrecognized token "<tok>" (ignored)` and the line skipped. A line with fewer tokens than the minimum SHALL be reported as `Not enough tokens for <kw> line` and skipped. Diagnostics are prefixed with the file name and line number (informative). The `adjust` and `timestamp` lines SHALL have no effect.
Compatibility: exact
(Magic ref: extflat/extparse.h:31-62, utils/lookup.c:58-230, extflat/EFread.c:222-240)

#### Scenario: Prefix keyword
- **GIVEN** a line starting with `kill`
- **WHEN** it is matched
- **THEN** it is treated as `killnode`

#### Scenario: Ambiguous prefix
- **GIVEN** a line starting with `s`, which matches `scale`, `subcap`, `substrate` and `style`
- **WHEN** it is matched
- **THEN** the line is reported as unrecognized and ignored

### Requirement: Scale record
A `scale <rscale> <cscale> <lscale>` record SHALL set the per-file multipliers. Every later `resist` value is multiplied by `rscale`, and every capacitance (`node`, `substrate`, `cap`, `merge`, `subcap`, `rnode`) by `cscale`. If `rscale` or `cscale` parses to 0 it SHALL be reset to 1. If `lscale` differs from the current extraction style's units-per-lambda `U`, the coordinate rescale factor `locScale = lscale / U` SHALL apply to later coordinates, as stated per record. `lscale` SHALL then be taken as `U`, or as 1 if it is 0. Unless coordinates are to be kept in database units, the definition's scale SHALL become `lscale`. The global output scale `S_out`, used by the netlisters for output lengths (`S_out / 100` µm per internal unit), SHALL work as follows:
- If it is still 0, it becomes `lscale`.
- If a different nonzero value is seen in another file, it becomes 1.0 and the "scale changed" flag is set.

At the end of a successful read, `S_out = 0` SHALL be replaced by 1.0.
Compatibility: exact
(Magic ref: extflat/EFread.c:244-276, extflat/EFread.c:110-118)

#### Scenario: Matching scales
- **GIVEN** two files that both have `scale 1000 1000 5` and an extraction style with units-per-lambda 5
- **WHEN** both are read
- **THEN** `S_out = 5` and the scale-changed flag stays clear

#### Scenario: Zero resistance scale
- **GIVEN** `scale 0 1000 5`
- **WHEN** it is read
- **THEN** the warning `Bad resistance scaling = 0; reset to 1.` is issued and `rscale = 1`

### Requirement: Node and substrate records
`node <name> <R> <C> <x> <y> <layer> {<area> <perim>} [<attrs>]` and `substrate ...` with the same fields SHALL create or extend a node. `R` is ignored by the reader. The capacitance is `cscale × atof(C)`, stored as single-precision float. `area`/`perim` pairs are integers assigned to resistance classes 0, 1, … in order. If the number of tokens after `<layer>` is odd, the last one is an attribute string and is not an area/perimeter value. Fewer than 2×(number of resistance classes) values SHALL produce the warning `Too few area/perim values: assuming remainder are zero`, and the missing values are 0.

If the name already exists in the definition, its capacitance and area/perimeter SHALL be added to the existing node; no new node is created. Otherwise a new node SHALL be created. Its location is `(int)(0.5 + x × locScale)` for each coordinate, and it is inserted at the head of the definition's node list. This makes node-list order the reverse of creation order. A `substrate` record SHALL mark the node as the global substrate and SHALL switch off backward-compatibility mode. In compatibility mode the global substrate is recognized by the name `GND!`; see capability `ext-flattening-hierarchy`. A `node` or `substrate` record read from the root file SHALL be recorded as a top-level node.

When a name is defined again right after a `killnode` of a node that aliases it, the alias SHALL first be removed from the killed node. A new separate node is then created. This supports `.res.ext` overlays.
Compatibility: exact
(Magic ref: extflat/EFread.c:417-434, extflat/EFbuild.c:143-334)

#### Scenario: Duplicate node record accumulates
- **GIVEN** `scale 1000 10 5`, `node a 0 7 0 0 m1 4 8` and `node a 0 3 10 0 m1 1 2` with one resistance class
- **WHEN** they are read
- **THEN** node `a` has capacitance 100 aF, area 5 and perimeter 10, and only one node exists

#### Scenario: Node order
- **GIVEN** records `node a ...` and then `node b ...`
- **WHEN** the definition's node list is walked from its head
- **THEN** `b` is visited before `a`

### Requirement: Device records
`device <class> <type> <llx> <lly> <urx> <ury> ...` and the legacy `fet <type> <llx> <lly> <urx> <ury> <area> <perim> <substrate> {<term> <len> <attrs>}` SHALL create devices. The class keyword SHALL be one of `fet mosfet asymmetric bjt devres devcap devcaprev vsource diode pdiode ndiode subckt rsubckt msubckt csubckt dsubckt veriloga`. An unknown class SHALL give `Unknown device class` and the line SHALL be skipped. The coordinates SHALL be scaled with `(int)(0.5 + v × locScale)`. After the coordinates the fields are, by class:

```ebnf
mosfet_like = len , wid , substrate , params , terms ;       (* mosfet, asymmetric, bjt *)
rc_device   = ( value | len , wid ) , [ substrate ] , params , terms ;  (* devres, devcap, devcaprev *)
diode_like  = params , [ substrate ] , terms ;               (* diode, pdiode, ndiode *)
subckt_like = params , [ substrate ] , terms ;               (* subckt rsubckt msubckt csubckt dsubckt veriloga *)
params      = { name , "=" , value } ;
terms       = { node_name , length , attrs } ;               (* attrs "0" means none *)
```

A resistor or capacitor whose type is `None` SHALL take one numeric value: resistance in Ω, or capacitance in fF. Otherwise it SHALL take integer L and W. A resistor or capacitor whose type is `None` and that has non-numeric arguments SHALL report an error and use 0.

An optional substrate is present when the remaining token count after the parameters is ≡ 1 (mod 3). A substrate token starting with `None` SHALL mean no substrate. Every parameter token `k=v` before the terminals SHALL be matched case-insensitively on its first two characters against the device type's declared parameters (see Requirement "Parameters records"):
- `a`/`a0` sets device area to `(int)(0.5 + v × locScale²)`.
- `p`/`p0` sets perimeter to `(int)(0.5 + v × locScale)`.
- `l`/`l0` and `w`/`w0` set length and width the same way.
- `c` sets capacitance and `r` sets resistance as floats.
- `l<n>`/`w<n>` with n ≠ 0, and any undeclared parameter, SHALL be kept verbatim in a per-device list. That list is in reverse order of appearance.

If the terminal tokens are not a multiple of 3, the record SHALL fail with `Incomplete terminal description for device`. Terminal and substrate node names that do not exist SHALL be created. A substrate node created this way, and any node marked as a device substrate whose name is not global, SHALL be flagged as an implicit substrate port. A substrate node created this way SHALL also be flagged as a device-terminal-only node.

A device whose type is `Short` SHALL NOT be stored. It SHALL behave as `equiv` between its first two terminal names. When resistance-extraction overlays (`extresist on`) are being read, it SHALL do so only while reading the `.res.ext` file, or always when overlays are disabled.

Devices SHALL be keyed by the string `"<llx>x<lly>_<t>"` (decimal integers). `t` is the technology tile-type index of the device type. If the type is unknown to the technology, `t` is the smallest index ≥ the number of tile types for which the key is still unused. A second record with an existing key SHALL be merged into the first if class, type (or tile type) and terminal count match. Its terminal nodes then replace the original ones. Otherwise it SHALL be rejected with an overlap error.
Compatibility: exact
(Magic ref: extflat/EFread.c:285-372, extflat/EFbuild.c:875-1330, extflat/EFbuild.c:1425-1470)

#### Scenario: MOSFET record
- **GIVEN** `device mosfet nfet 10 20 11 21 2 8 GND! a 4 0 b 8 0 c 8 0` and no declared parameters
- **WHEN** it is read
- **THEN** a device of class mosfet with L=2, W=8, substrate `GND!`, gate `a`, source `b` and drain `c` exists under key `10x20_<t>`

#### Scenario: Unmodeled resistor value
- **GIVEN** `device devres None 0 0 1 1 25.5 n1 0 0 n2 0 0`
- **WHEN** it is read
- **THEN** the device has resistance 25.5 and terminals `n1`, `n2`, and no substrate

#### Scenario: Short device
- **GIVEN** `device subckt Short 0 0 1 1 a 0 0 b 0 0` with overlays disabled
- **WHEN** it is read
- **THEN** no device is created and nodes `a` and `b` become one node

### Requirement: Parameters records
`parameters <type> {<key>=<expr>}` SHALL declare, once per device type name, how parameters are output. Later declarations for the same type are ignored. For each token:
- `parm_type` is the key's first character and, if the key is longer than one character, its second character.
- `parm_name` is the text after `=`.
- If that text contains `*`, the part after `*` is a float scale factor and the name ends at `*`. The offset is then left undefined; see Q-NETLIST-8.
- Otherwise, a `+N` or `-N` suffix is an integer offset and the name ends at the sign.
- Tokens without `=` SHALL be reported as `Bad parameter assignment` and skipped.

The declared list SHALL be stored in reverse order of the tokens. Output therefore emits parameters last-declared-first; see Q-NETLIST-9. A type name starting with `:` declares instance parameters of a cell. For those, the whole token is kept verbatim as the name, and `parm_type` becomes the two-digit token index `"00"`, `"01"`, ….
Compatibility: exact
(Magic ref: extflat/EFbuild.c:753-870)

#### Scenario: Declaration order reversed
- **GIVEN** `parameters nfet l=l w=w a1=as p1=ps`
- **WHEN** the parameter list for `nfet` is walked
- **THEN** the order is `p1→ps`, `a1→as`, `w→w`, `l→l`

#### Scenario: Scale suffix
- **GIVEN** `parameters rpoly l=len*0.5`
- **WHEN** it is read
- **THEN** the `l` parameter has name `len` and scale 0.5

### Requirement: Equivalence, merge and kill records
`equiv <a> <b>` SHALL make the two names the same node:
- If neither name exists, `a` is created, then `b` is added as a non-preferred alias.
- If only one name exists, the other is added as an alias. When `a` is new, the alias is added non-preferred. Otherwise preference follows capability `ext-flattening-names`, except that a top-level port name is never displaced.
- If both names exist on different nodes, the nodes are merged (see capability `ext-flattening-hierarchy`).

When both names are ports with different port indices, the following rules SHALL apply in order:
1. When netlisting SPICE and the names are equal ignoring case, merge silently.
2. If the names are equal after cutting each name at its first `_uq`, merge silently.
3. Otherwise, under `short resistor` (or `short voltage`), do not merge. Instead create a resistor (or voltage source) device of type `None` with value 0 between the two names, plus a dangling first terminal named `dummy` (Q-NETLIST-31).
4. Otherwise, when not reading resistance-extraction overlays, warn `Ports "<a>" and "<b>" are electrically shorted.` and merge.
5. When reading with resistance-extraction overlays, keep them separate.

Every `equiv` read while an overlay is pending SHALL also be remembered. After the `.res.ext` file has been read, it SHALL be applied as a plain merge, but only if neither name appeared on an `rnode` or `resist` record of the overlay.

`merge <a> <b> [<C> {<area> <perim>}]` SHALL record a hierarchical connection that is resolved during flattening. Its delta capacitance is `C × cscale`, and its area/perimeter deltas are `(int)(0.5 + v × locScale²)` and `(int)(0.5 + v × locScale)`. The same port-short handling as for `equiv` SHALL apply when both names are local ports with different indices and shorts are not `none`.

`killnode <name>` SHALL record a kill that is applied at flattening time.
Compatibility: exact
(Magic ref: extflat/EFbuild.c:541-750, extflat/EFbuild.c:1762-1880, extflat/EFbuild.c:509-530, extflat/EFread.c:374-416, extflat/EFread.c:725-745)

#### Scenario: Shorted ports with resistor policy
- **GIVEN** `ext2spice short resistor`, ports `A` (index 0) and `B` (index 1), and the line `equiv A B`
- **WHEN** it is read
- **THEN** `A` and `B` remain distinct nodes and a 0-valued `None` resistor device connects them

#### Scenario: Unique-suffix ports merge
- **GIVEN** ports `net` (index 0) and `net_uq1` (index 1) and the line `equiv net net_uq1`
- **WHEN** it is read
- **THEN** the nodes are merged and no warning is printed

### Requirement: Use, port, cap, resist, attr and connect records
The reader SHALL handle these records as follows:
- **`use`**: `use <def> <id> <a> <b> <c> <d> <e> <f>` SHALL create an instance with transform `(a b c / d e f)`. If `<id>` contains `[` followed by `%d:%d:%d][%d:%d:%d`, it SHALL be an array with x range `xlo:xhi:xsep` and y range `ylo:yhi:ysep`. The id is cut at the `[`. Any other bracket text SHALL be kept in the id verbatim, with no array. A repeated id SHALL warn and replace the earlier use.
- **`port`**: `port <name> <idx> <llx> <lly> <urx> <ury> <layer>` SHALL mark the node as a port with index `idx`. The node is created if needed. A port in the root file is also a top-level port. Port records SHALL mark the definition as a subcircuit, but only when subcircuit processing was requested for this read. Subcircuit processing is disabled for the children of a definition that has itself been marked as a subcircuit, unless that definition is the root.
- **`cap`**: `cap <a> <b> <C>` SHALL record a coupling of `C × cscale` aF.
- **`resist`**: `resist <a> <b> <R>` SHALL record a resistor of value `(float)(0.5 + rscale × atof(R))`, with no truncation; see Q-NETLIST-12.
- **Array ranges in names**: in `cap`, `resist` and `merge`, names may contain array ranges `[lo:hi]` or `[lo:hi,lo:hi]`, with at most 2 ranges. Both names SHALL have the same number of ranges with equal spans. Otherwise the record is rejected with `Number of subscripts doesn't match` or `Subscript <n> range mismatch`. A range with lo > hi is rejected.
- **`subcap`**: `subcap <name> <C>` SHALL add `C × cscale` to an existing node. An unknown node is silently ignored unless warnings are on.
- **`attr`**: `attr <node> <llx> <lly> <urx> <ury> <layer> <text>` SHALL prepend the attribute text to the node, with scaled coordinates. An unknown node is reported as `Attribute for nonexistent node <n> ignored`.
- **`connect`**: `connect` records SHALL only be kept as connection points for resistance extraction.
- **`abstract`/`primitive`**: these SHALL flag the definition as abstract or primitive.
Compatibility: exact
(Magic ref: extflat/EFbuild.c:1533-1612, extflat/EFbuild.c:1331-1376, extflat/EFbuild.c:1881-2110, extflat/EFbuild.c:336-430, extflat/EFread.c:438-470, extflat/EFread.c:627-648)

#### Scenario: Array use
- **GIVEN** `use inv u1[0:3:10][0:0:0] 1 0 0 0 1 0`
- **WHEN** it is read
- **THEN** use `u1` exists with x range 0..3, separation 10, and a single y element

#### Scenario: Resist rounding
- **GIVEN** `scale 1 1 1` and `resist a b 1234`
- **WHEN** it is read
- **THEN** the stored resistance is 1234.5 mΩ

### Requirement: Technology, style, version and resistance classes
The reader SHALL handle these records as follows:
- **`tech <name>`**: SHALL set the technology name. In the embedded editor, if the name differs from the loaded technology and layout is loaded, the read SHALL fail. If no layout is loaded, the named technology SHALL be loaded. A `-T` argument SHALL override the final technology name.
- **`style <name>`**: an unknown style SHALL clear the active style and warn `.ext file style <s> is not known in this technology!`. The netlisters then output zero area/perimeter and parasitic values.
- **`version <v>`**: a version different from the editor's own SHALL warn `Cell was extracted using version <v> of the extractor.` (informative).
- **`resistclasses <r0> <r1> …`**: the first occurrence SHALL define the class count and their sheet resistances in mΩ/square. A later file with a different count or different values SHALL report a mismatch and set an error flag, but reading SHALL continue.
Compatibility: behavioral
(Magic ref: extflat/EFread.c:470-626)

#### Scenario: Resistance class mismatch
- **GIVEN** a root with `resistclasses 100 200` and a child with `resistclasses 100 300`
- **WHEN** both are read
- **THEN** `Resistance class values don't match:` is reported and reading continues

### Requirement: Resistance-extraction overlay
When overlays are requested (the `extresist on` option of the netlisters), the reader SHALL do the following after each `<cell>.ext`. It SHALL search `<cell>.res.ext` in the same three locations, and if found, read it with the same keyword semantics into the same definition. The overlay's own `scale` record applies to it. Every node name that appears in an `rnode` or `resist` record of the overlay SHALL be recorded so that deferred `equiv` statements can be filtered (see Requirement "Equivalence, merge and kill records"). The overlay grammar is defined in capability `extresist-output`.
Compatibility: exact
(Magic ref: extflat/EFread.c:665-700, extflat/EFread.c:404-412, extflat/EFread.c:610-624)

#### Scenario: Port equiv preserved for resistor network
- **GIVEN** `extresist on`, `cell.ext` containing `equiv P1 P2` between two ports, and `cell.res.ext` containing `resist "P1" "x.n0" 12.3`
- **WHEN** the files are read
- **THEN** `P1` and `P2` remain distinct nodes connected only through the resistor network
