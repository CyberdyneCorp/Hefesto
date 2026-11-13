# Schematic Netlisting

## Purpose
Defines how Hefesto turns a schematic hierarchy into a SPICE netlist that is identical to the one xschem produces (decision P-12). The netlist feeds embedded simulation (capabilities `simulation-*`) and LVS (capabilities `lvs-*`). This covers:
- connectivity: wires, pins, labels, pass-through and shorted symbols, bus taps and global nets
- net naming, including unnamed nets and unconnected pins
- bus and vector notation
- `format` string substitution (`@name`, `@pinlist`, `@@pin`, `@#n`, `@key`, `%key`, `@spiceprefix`, `tcleval`, `expr(...)`)
- the post-processing that xschem performs with its `spice.awk` and `break.awk` helper scripts: vector expansion, line joining and line breaking
- the file structure of hierarchical, top-level-only and flat netlists, `.subckt` generation and ordering
- netlist options and the LVS netlist mode used by PDK LVS flows

Only the SPICE netlister is in scope. xschem's Verilog, VHDL, Spectre and tEDAx netlisters are out of scope; their records and `*_format` attributes are kept through round trips, and Verilog is revisited with P-17 (Q-SCHEM-4, `Resolution: other`).

**Compatibility adaptation.** This capability is about xschem, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **xschem** (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`, version `3.4.8RC`):
- `exact`: byte-identical netlist text
- `behavioral`: same circuit; text MAY differ as stated
- `informative`: xschem detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-12)`

xschem quirks whose open-question entry records `Resolution: compat` are governed by the `xschem-compat` setting (capability `hefesto-magic-compat-mode`, default ON). The requirements that cite them state both modes; `exact` refers to the output with `xschem-compat` ON, which is the default. Warnings that an entry gives in both modes are reported whatever the setting.

Traces use `(xschem ref: path/file:line)`, relative to the xschem `src/` directory; `.awk` traces point at xschem's post-processing scripts. They exist for traceability only. Implementers SHALL work from this spec and SHALL NOT consult the GPL sources (capability `hefesto-licensing`). Hefesto SHALL produce the final text directly; it SHALL NOT run awk or any external program.

Other capabilities own related parts:
- File grammar, property strings and boolean values: capability `schematic-file-format`.
- Symbol properties, types, pins, templates, `tcleval` and library resolution: capability `schematic-symbols-and-libraries`.
- How the netlist is turned into a simulation deck: capabilities `simulation-netlist-preparation` and `schematic-simulation-integration`.
- How LVS reads the netlist: capability `lvs-netlist-input`.

Open questions are in `docs/decisions/open-questions/SCHEM.md` (Q-SCHEM-n).

## Requirements

### Requirement: Netlist options
The SPICE netlister SHALL take these options. Each SHALL have the default shown. Each SHALL be settable in Hefesto settings (capability `hefesto-settings`) and per run (capability `schematic-command`); a `netlist_options` instance in the top schematic SHALL override them for that run.

| Option | Default | Meaning |
|---|---|---|
| `hierarchy` | `hier` | `hier`: hierarchical netlist; `top`: top level only, no subcircuit definitions; `flat`: hierarchical netlist flattened (Requirement "Flat netlist") |
| `lvs_netlist` | false | LVS netlist mode (Requirement "LVS netlist mode") |
| `top_is_subckt` | false | Wrap the top level in an active `.subckt`/`.ends` |
| `lvs_ignore` | false | Honour `lvs_ignore` attributes |
| `spiceprefix` | true | Honour `@spiceprefix` |
| `bus_replacement_char` | empty | Two characters that replace `[` and `]` in expanded net and instance names |
| `uppercase_subckt` | false | Write `.SUBCKT`/`.ENDS` instead of `.subckt`/`.ends` |
| `hiersep` | `.` | Hierarchy separator used for simulation node paths (capability `schematic-simulation-integration`) |

A `netlist_options` instance SHALL apply each of these keys that it defines: `bus_replacement_char` (only if exactly 2 characters), `lvs_ignore`, `lvs_netlist`, `top_is_subckt`, `spiceprefix` (false only for a false value; any other value is true) and `hiersep`. They take effect when that instance is reached, in instance order, before the top-level `.subckt` line is written.

Compatibility: exact

(xschem ref: netlist.c:598-641, spice_netlist.c:268-310, xschem.tcl:11926-11947)

#### Scenario: Options instance enables LVS mode
- **GIVEN** a top schematic containing a `netlist_options.sym` instance with `lvs_netlist=true`
- **WHEN** it is netlisted with default options
- **THEN** the netlist SHALL be produced in LVS netlist mode

### Requirement: Ignored and shorted objects
For the SPICE netlist, an instance SHALL be ignored (not netlisted and not connected) if its instance property or its symbol property has `spice_ignore` true or `open`. It SHALL be shorted if either has `spice_ignore=short`: all its pins become one net (Requirement "Pass-through and shorted symbols") and it is not netlisted. When option `lvs_ignore` is on, `lvs_ignore` true or `open` SHALL also ignore an instance and `lvs_ignore=short` SHALL also short it. A wire SHALL be ignored when its `spice_ignore` is true or `open`, or, with option `lvs_ignore` on, when its `lvs_ignore` is true or `open`. Ignored wires carry no connectivity. Instances whose symbol was not found SHALL be ignored.

The instance value SHALL be evaluated through `tcleval` when it has that form (capability `schematic-symbols-and-libraries`), so a PDK can write `lvs_ignore=tcleval($lvs_ignore)`.

Compatibility: exact

(xschem ref: actions.c:825-930, netlist.c:1149-1240)

#### Scenario: Shorted ammeter
- **GIVEN** an `ammeter.sym` instance with `spice_ignore=short` between nets `A` and `B`
- **WHEN** the schematic is netlisted
- **THEN** no line SHALL be written for it, and every pin connected to `A` or `B` SHALL be on one net

### Requirement: Connection geometry
Connectivity SHALL be computed per schematic from exact binary64 coordinates, with no tolerance:
- A point P **touches** a segment from (x1,y1) to (x2,y2), whose endpoints are ordered as in capability `schematic-file-format`, when (x2−x1)(Py−y1) = (y2−y1)(Px−x1) and either x1 < x2 and x1 ≤ Px ≤ x2, or x1 = x2 and y1 ≤ Py ≤ y2. A zero-length segment touches only its own point.
- Two wires connect when an endpoint of either touches the other. Two wires that cross at interior points do not connect.
- An instance pin (the transformed centre of its pin box, capability `schematic-file-format`, Requirement "Orientation transform") connects to every wire it touches, including at a wire's interior.
- Two instance pins connect when their points are equal.
- A label or port instance has exactly one pin; it connects in the same way.

Compatibility: exact

(xschem ref: clip.c:245-256, netlist.c:1019-1147, netlist.c:777-801)

#### Scenario: T junction connects
- **GIVEN** wires (0,0)-(100,0) and (50,0)-(50,50)
- **WHEN** connectivity is computed
- **THEN** both wires SHALL be on one net

#### Scenario: Crossing does not connect
- **GIVEN** wires (0,0)-(100,0) and (50,-50)-(50,50)
- **WHEN** connectivity is computed
- **THEN** they SHALL be on two different nets

#### Scenario: Pin on wire interior
- **GIVEN** a wire (0,0)-(100,0) and a resistor pin at (40,0)
- **WHEN** connectivity is computed
- **THEN** the pin SHALL connect to the wire's net

### Requirement: Net naming order
Hefesto SHALL name the nets of one schematic in this order, and the first name given to a net SHALL be kept:
1. **Labels and ports.** For each non-ignored instance in instance order whose type is `label`, `ipin`, `opin` or `iopin`: its pin gets the name in its `lab` attribute (after `@`/`%` substitution in the parent context, Requirement "Hierarchical parameters", and `expr` evaluation). The name then spreads to every wire touching the pin and every instance pin at the same point, and from there recursively through connected wires (depth-first) and to every instance pin touching those wires, and through pass-through, shorted and bus-tap symbols. A label or port without `lab` does not name anything.
2. **Unnamed wires.** For each non-ignored wire in wire order that is still unnamed: it gets the next automatic name (Requirement "Unnamed nets"), which spreads as in step 1.
3. **Unnamed pins.** For each non-ignored instance in instance order, for each pin in pin order that is still unnamed: it gets the next automatic name, which spreads to other instance pins at the same point.

When the spreading reaches a wire or pin that already has a different name, the existing name SHALL be kept and the netlist SHALL still be written. This is a short, reported as `Error: <tag> shorted: <a> - <b>` with tag `Net to net` (wire to wire), `net to named instance pin`, `Net`, `Instance pin`, `Pass_through symbol` or `Bus tap` (Requirement "Errors and warnings"). Every short SHALL be reported, including shorts at the top level during its first connectivity pass and in runs that do not netlist the hierarchy, which xschem does not always report; the netlist text is the same either way (Q-SCHEM-16, `Resolution: fix`).

After naming, every wire's `lab` attribute SHALL be set to its net name (unnamed nets including their `#`), and every label or port without `lab` SHALL get `lab` set to its net name without `#`. These updates SHALL use the property update rules of capability `schematic-file-format`; they change the saved file like xschem does, but SHALL NOT mark the document as modified (Q-SCHEM-29, `Resolution: replicate`, whatever the `xschem-compat` setting).

Compatibility: exact

(xschem ref: netlist.c:1404-1615, netlist.c:908-1015, netlist.c:1714-1755)

#### Scenario: First label wins
- **GIVEN** one connected net with label instance `l1` (`lab=A`) before label instance `l2` (`lab=B`) in instance order
- **WHEN** the schematic is netlisted
- **THEN** every device pin on that net SHALL be on net `A`, and the netlist SHALL be written

#### Scenario: Top-level short always reported
- **GIVEN** a top schematic with no subcircuits where labels `A` and `B` are on one wire
- **WHEN** it is netlisted, in a normal run and in a run that does not netlist the hierarchy
- **THEN** each run SHALL report the short between `A` and `B` and return a failure status, and the netlist text SHALL be the same as xschem's

### Requirement: Unnamed nets
Automatic names SHALL be `#net<N>` internally, with N counting 1, 2, 3, ... per schematic, skipping any N for which `net<N>` is already a net name in that schematic (for example from a label). In the netlist the `#` SHALL be removed, giving `net<N>`. The multiplicity of an unnamed net SHALL be the smallest product (pin bus width × instance vector width) among the pins attached to it. An unnamed net with multiplicity m > 1 SHALL be written as the expansion of `net<N>_[m−1..0]`, that is `net<N>_<m−1>,...,net<N>_0`.

A pin that has no net at all (for example on an instance whose node data is missing) SHALL be written `__UNCONNECTED_PIN__<k>`, or `__UNCONNECTED_PIN__<k>_[w−1..0]` expanded for a pin of width w > 1, with k counting from 0 over the whole netlist run, and SHALL report the warning `Warning: unconnected pin,  Inst idx: <i>, Pin idx: <j>  Inst:<name>`.

Compatibility: exact

(xschem ref: netlist.c:803-837, netlist.c:1543-1615, token.c:4126-4224)

#### Scenario: Two floating resistors
- **GIVEN** a schematic with one wire (0,0)-(0,50), unlabeled, joining resistor `R1` pin `M` and resistor `R2` pin `P`, while `R1` pin `P` and `R2` pin `M` touch nothing
- **WHEN** it is netlisted
- **THEN** the wire SHALL be `net1`, `R1` pin `P` SHALL be `net2` and `R2` pin `M` SHALL be `net3`, assuming `R1` precedes `R2` and pin `P` precedes pin `M`

#### Scenario: User net named net1
- **GIVEN** a label `lab=net1` and two unlabeled wires
- **WHEN** the schematic is netlisted
- **THEN** the unlabeled wires SHALL be named `net2` and `net3`

### Requirement: Pass-through and shorted symbols
When a pin of an instance gets a net name, and the symbol has another pin with the same `name`, or the instance is shorted (Requirement "Ignored and shorted objects"), every other pin with that name (for a shorted instance: every pin) SHALL get the same net name and spread it.

Compatibility: exact

(xschem ref: netlist.c:1242-1402)

#### Scenario: Shorted resistor joins nets
- **GIVEN** a resistor instance with `spice_ignore=short` whose pins touch labels `A` and nothing else
- **WHEN** the schematic is netlisted
- **THEN** both pins SHALL be on net `A`

### Requirement: Bus taps
For an instance of type `bus_tap` with at least two pins, pin 1 connects to a bus and pin 0 is the tap. When pin 1 gets a net name `bus`, pin 0 SHALL be renamed, unless it already has a non-automatic name:
- if the instance's `lab` starts with `[` or is all digits, the tap name SHALL be the bus base name (the part of `bus` before `[`, taken after the last `,` and its following spaces if the bus is a list) followed by `lab`
- otherwise the tap name SHALL be `lab`

The tap name then spreads from pin 0.

Compatibility: exact

(xschem ref: netlist.c:1330-1372)

#### Scenario: Bit tap
- **GIVEN** a bus tap whose pin 1 is on net `DATA[15:0]` and whose `lab` is `[3]`
- **WHEN** the schematic is netlisted
- **THEN** its pin 0 net SHALL be `DATA[3]`

### Requirement: Bus and vector notation
Net names, pin names and instance names SHALL be expanded into comma-separated lists of single names by this grammar. A string that contains none of `*`, `,`, `.` and `:`, or that starts with `$`, SHALL be returned unchanged with width 1.

```ebnf
list     = item | list , item-suffix | list , "*" , int     (* each element repeated n times: a,a,b,b *)
         | int , "*" , list                                  (* whole list repeated n times: a,b,a,b *)
         | list , "," , list | "(" , list , ")" ;
item     = name
         | name , "[" , index , "]" , [ trailer ]           (* name[i] for each index *)
         | name , "[" , dindex , "]" , [ trailer ]          (* name<i> for each index, no brackets *)
         | name , "[" , name , "]" ;                         (* kept literally, width unknown *)
index    = range , { "," , range } ;
range    = i | i , ":" , j | i , ":" , j , ":" , step | i , ":" , j , ":" , ext , ":" , rep ;
dindex   = drange , { "," , drange } ;
drange   = i | i , ".." , j | i , ".." , j , ".." , step | i , ".." , j , ".." , ext , ".." , rep ;
```

Index semantics, with s = sign(j − i), or +1 when i = j:
- `i:j` gives i, i+s, ..., j
- `i:j:step` gives i, i+s·step, ... while not past j (stops before exceeding j)
- `i:j:ext:rep` repeats the sequence i..j rep times, adding ext to every element of each successive repetition
- a `trailer` (characters after `]`) is appended to every element
- the width is the number of elements

When option `bus_replacement_char` is set, every `[` and `]` in an expanded name SHALL be replaced by its first and second character. A syntax error SHALL report `syntax error in <string>` and SHALL use the string unchanged with unknown width.

Compatibility: exact

(xschem ref: expandlabel.y:283-470, parselabel.l:97-150)

#### Scenario: Descending bus
- **WHEN** `A[3:0]` is expanded
- **THEN** the result SHALL be `A[3],A[2],A[1],A[0]` with width 4

#### Scenario: Stepped and bracketless forms
- **WHEN** `D[7:0:2]` and `Q[2..0]` are expanded
- **THEN** the results SHALL be `D[7],D[5],D[3],D[1]` and `Q2,Q1,Q0`

#### Scenario: Repetition
- **WHEN** `2*(a,b)` and `(a,b)*2` are expanded
- **THEN** the results SHALL be `a,b,a,b` and `a,a,b,b`

#### Scenario: Bus replacement characters
- **GIVEN** option `bus_replacement_char` = `<>`
- **WHEN** `A[1:0]` is written to the netlist
- **THEN** it SHALL appear as `A<1> A<0>` where nets are space-separated

### Requirement: Global nets
A label whose instance property, or else symbol property, has `global` true or `global=ground` SHALL declare its net name (each bit of a bus separately) as global. The name `0` SHALL never be declared. Global names SHALL be collected over the whole netlist run in first-seen order, without duplicates, and SHALL be written once as `.GLOBAL <name>` lines (Requirement "Netlist file structure"). Declaring a net global does not by itself connect nets in different subcircuits; the simulator or LVS tool does that from the `.GLOBAL` line.

Compatibility: exact

(xschem ref: netlist.c:703-776, netlist.c:1470-1500)

#### Scenario: IHP ground
- **GIVEN** a testbench using `devices/gnd.sym` instances with `lab=GND`
- **WHEN** it is netlisted
- **THEN** the netlist SHALL contain exactly one line `.GLOBAL GND`

### Requirement: Format string substitution
Each netlisted instance SHALL produce the text of its format string with tokens replaced. The format string SHALL be, in order of preference: the instance's `lvs_format` (LVS mode only), the symbol's `lvs_format` (LVS mode only), the instance's `format`, the symbol's `format`. It SHALL be read decoded but without Tcl evaluation (capability `schematic-file-format`, Requirement "Property strings"). An instance with no format SHALL produce nothing. The instance name SHALL be its `name`, or the template's `name` if it has none.

Parsing:
- A backslash escapes the next character, and the backslash is dropped. An escaped LF is dropped entirely.
- A token starts at an unescaped `@` or `%`. It ends before the first character, at position 2 or later of the token, that is an unescaped separator (space, tab, LF, `;`, end of string), an unescaped `@` or `%`, or an escaped character other than a separator, `@` or `%`. This is xschem's rule, kept whatever the `xschem-compat` setting because PDK formats rely on it (`@spice_get_voltage(net)`). When a token's lookup fails but a shorter prefix of the token made of identifier characters (letters, digits, `_`) names a defined key, Hefesto SHALL report a warning naming the token and that prefix (Q-SCHEM-26, `Resolution: replicate`).
- The ending character is copied to the output unless it is `@` or `%` (which starts the next token) or the end of the string. Characters outside tokens are copied.
- A LF is appended at the end.

Token values:

| Token | Output |
|---|---|
| `@pinlist` | For each pin in pin order that does not have `spice_ignore` true, only the first pin of each distinct name: `?<w> <nets> `, where w is the pin's width (bus expansion of its name) and `<nets>` is the comma-separated expansion of the attached net |
| `@@<pin>` | The same `?<w> <nets> ` for the first pin named `<pin>`, if it does not have `spice_ignore` true |
| `@#<n>` or `@#<pin>` | The same for pin number n (0-based) or the named pin |
| `@#<n>:<attr>` | The pin attribute value (capability `schematic-symbols-and-libraries`, Requirement "Text substitution tokens"); `--UNDEF--` if not found; for `pinnumber` with an instance name ending in `:<k>` and a value containing `:`, the k-th `:`-separated item |
| `@symname`, `@symname_ext`, `@symref`, `@schname`, `@schname_ext`, `@topschname`, `@index` | As in capability `schematic-symbols-and-libraries`; `@symname` sanitised like a generator name |
| `@savecurrent` | If the instance's (else template's) `savecurrent` is true: LF `.save I( ?1 <instance name> )`; else nothing |
| `@spiceprefix` | With option `spiceprefix` off: nothing. Otherwise its value v (Requirement "Hierarchical parameters"); if v is non-empty, the text `**** spice_prefix <v>` LF, which marks the line for prefixing (Requirement "Element line post-processing") |
| `@tcleval(...)` | Tcl evaluation (capability `schematic-symbols-and-libraries`, Requirement "tcleval values") |
| `@<key>` | The value of `key` (Requirement "Hierarchical parameters"), then `expr` evaluation. If unresolved, nothing. `@name` and `@lab` values are bus-expanded. |
| `%<key>` | As `@key`, but if `key` is defined nowhere, the text `<key>` |

After the whole line is built: if it begins with `tcleval(`, it SHALL be Tcl-evaluated; then `expr` evaluation SHALL be applied to the whole line. If the instance produced output, the marker line `**** end_element` SHALL follow it.

Compatibility: exact

(xschem ref: token.c:2426-2816)

#### Scenario: Resistor line before post-processing
- **GIVEN** `res.sym` (`format="@name @pinlist @value m=@m"`) with instance `name=R1 value=1k m=1`, pin P on net `A`, pin M on net `B`
- **WHEN** the element text is built
- **THEN** it SHALL be `R1 ?1 A ?1 B  1k m=1` (two spaces before `1k`, because `@pinlist` ends with a space and the space after the token is copied) LF, followed by the line `**** end_element`, and after post-processing the line SHALL be `R1 A B 1k`

#### Scenario: Token ends at whitespace only
- **GIVEN** a format containing `@W/@L` and an instance with `W=2 L=1`
- **WHEN** it is substituted
- **THEN** the token `@W/` SHALL be unresolved and give nothing, the output SHALL be `1`, and a warning SHALL say that `@W/` is undefined while `W` is defined

### Requirement: Hierarchical parameters
The value of `@key` for an instance SHALL be found as follows, where "lookup in S1, S2, ..." means: the first source that defines the key gives the value v; each later source that defines a key equal to v replaces v with that key's value:
1. v = the result of substituting the token in the instance property string (unresolved tokens are kept as text).
2. If v still contains `@` or `%`: substitute v by lookup in the instance properties, then the properties of the parent instance (the instance through which the netlister descended into the current schematic; none at top level), then the symbol template.
3. If v still contains `@` or `%`: substitute v in the instance properties without predefined tokens; then, if still needed, by lookup in the instance properties and then the parent symbol's template, keeping backslash escapes.
4. If v still starts with `@`, the value is empty.

Pin names, `lab` values and instance names that contain `@` or `%` SHALL be substituted in the context of the parent instances, from the nearest parent outwards, before bus expansion and `expr` evaluation.

Compatibility: exact

(xschem ref: token.c:2690-2760, token.c:5709-5800, token.c:5223-5297, netlist.c:971-1015)

#### Scenario: Model passed down from the parent template
- **GIVEN** `passgate.sym` with template `... modeln=nfet_01v8`, and inside `passgate.sch` an nmos instance with `model=@modeln` and format token `@model`
- **WHEN** the hierarchical netlist is generated
- **THEN** the nmos line inside `.subckt passgate` SHALL use `nfet_01v8`

### Requirement: expr evaluation
Text SHALL be scanned for the markers `expr(`, `expr_eng(` and `expr_eng4(`. Text outside markers SHALL be copied. After a marker, an arithmetic expression SHALL be parsed up to the matching `)`, optionally enclosed in single quotes (`expr('...')`):
- **numbers**: a run of the characters `.0-9a-zA-Z_-` starting with a digit or `.`, converted by a C `%lf` scan, then multiplied by the SI suffix that starts the rest of the run: `T` 1e12, `G` 1e9, `x`/`X` or `meg` (case-insensitive) or `M` 1e6, `m` 1e-3, `k` 1e3, `u` 1e-6, `n` 1e-9, `p` 1e-12, `f` 1e-15, `a` 1e-18. With `xschem-compat` ON, `-` belongs to the run anywhere after its first character, as in xschem, so `2-1` is read as the number 2. With `xschem-compat` OFF, a `-` belongs to the run only directly after an `e` or `E` (an exponent sign), so `2-1` is the number 2 minus the number 1. In both modes, a run as read with `xschem-compat` ON that contains a `-` not directly after `e` or `E` SHALL be reported as a warning, because the two modes read it differently (Q-SCHEM-12, `Resolution: compat`).
- **constants**: `pi`, `e`, `k` (1.380649e-23), `h` (6.62607e-34), `echarge` (1.60217646e-19), `abszero` (273.15), `c` (2.99792458e8)
- **functions** of one argument: `int` (truncate toward zero), `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `log` (base 10), `ln`, `exp`, `sqrt`, `round6`, `round1` (round to 6 or 1 significant digits), `round`
- **operators**, lowest to highest precedence: `?:` (right-assoc), `==` and `!=`, `<` `>` `<=` `>=`, `+` and `-`, `*` `/` `%` (integer remainder of truncated operands), unary minus, `^` (power, right-assoc), parentheses
- **result**: `expr` prints with C `%.15g`; `expr_eng` prints in engineering notation with the `ev_precision` setting (default 4) significant digits; `expr_eng4` with 4. Engineering notation divides by 1e12, 1e9, 1e6 or 1e3 for |x| above 0.999999 times that power (suffix `T`, `G`, `MEG`, `k`), keeps the value for |x| > 0.0999999, else multiplies by 1e3 ... 1e18 (suffix `m`, `u`, `n`, `p`, `f`, `a`); |x| < 0.999999e-23 prints `0`.
- **errors**: on a syntax error or an unknown identifier, the rest of the text from the failing expression on SHALL be copied with every `expr(`, `expr_eng(` or `expr_eng4(` marker and its matching `)` removed.

Compatibility: exact

(xschem ref: eval_expr.y:1-460, editprop.c:536-621, token.c:964-973)

#### Scenario: sky130 drain area
- **GIVEN** the sky130 nfet template `ad="expr('int((@nf + 1)/2) * @W / @nf * 0.29')"` with `nf=1` and `W=1`
- **WHEN** `@ad` is substituted
- **THEN** it SHALL give `0.29`

#### Scenario: Parameter left symbolic
- **GIVEN** the same template with `W=W_N`
- **WHEN** `@ad` is substituted
- **THEN** it SHALL give `int((1 + 1)/2) * W_N / 1 * 0.29`

#### Scenario: Minus inside a number, xschem-compat ON
- **GIVEN** `xschem-compat` ON
- **WHEN** `expr(2-1)` and `expr(1e-3-1)` are evaluated
- **THEN** they SHALL give `2` and `0.001`, and a warning SHALL be reported for each

#### Scenario: Minus inside a number, xschem-compat OFF
- **GIVEN** `xschem-compat` OFF
- **WHEN** `expr(2-1)`, `expr(1e-3-1)` and `expr(2 - 1)` are evaluated
- **THEN** they SHALL give `1`, `-0.999` and `1`, and a warning SHALL be reported for the first two only

### Requirement: Element line post-processing
After all element texts are built, the netlist text SHALL be transformed line by line, as xschem does with its `spice.awk` script:
1. **Join.** Outside user-code blocks, a line starting with `+` SHALL be appended to the previous line as one space followed by the rest of the line after the `+`. A user-code block runs from a line starting with `**** begin user architecture code` or `**** begin user header code` to the matching `**** end user ... code` line.
2. **Trailing m=1.** A trailing match of the regular expression ` [mM] *= *1 *$` SHALL be removed. With `xschem-compat` ON it is removed from every line, including user code, comments, `.param` and `.control` text, as xschem does. With `xschem-compat` OFF it is removed only from device lines: lines outside user-code blocks whose first field does not start with `*` or `.` (Q-SCHEM-22, `Resolution: compat`).
3. **Parametric vector instance names.** If any line starts with `.param` (case-insensitive), its `name=value` pairs (spaces around `=` removed) SHALL be recorded; an element whose first field matches `^[a-zA-Z][^\[\]:]+\[[a-zA-Z_][a-zA-Z0-9_]*:[a-zA-Z_][a-zA-Z0-9_]*\]$` SHALL have its first field replaced by the expansion `name[first],...,name[last]` with recorded parameter values.
4. **Markers.** `**** end_element` lines SHALL be dropped and reset the prefix; `**** spice_prefix <p>` lines SHALL be dropped and set the prefix to the third field. User-code header markers SHALL be dropped; architecture markers SHALL be kept. Lines inside user code SHALL be kept unchanged.
5. **Comments.** A line whose first field starts with `*` SHALL be kept unchanged.
6. **Field prefixes.** A field starting with `##` SHALL lose the `##`. A field of the form `#<pfx>#<rest>` SHALL become `<pfx><rest>` with `<pfx>` also inserted after every comma, and `?1 ` inserted before it if the previous field is not a `?` marker. A field `#<pfx>#?<w>` SHALL lose its prefix and the prefix SHALL be put before each comma-separated item of the next field.
7. **Renames.** Every `PARAM:` SHALL be removed. On lines starting with `g` or `G`, the first ` value=` (case-insensitive) becomes ` cur=`; `e`/`E`: ` vol=`; `r`/`R`: ` r=`; `c`/`C`: ` c=`. On lines starting with `D`, the first `PERI` followed by optional spaces and `=` becomes `PJ=`.
8. **Save lines.** A line whose first field is `.save` or `.print` (case-insensitive) and that contains `?<n>` SHALL be lowercased, and for each remaining field `<t>(<names>)`, after removing `?n` markers and spaces next to parentheses, one line `<.save or .print> <t>(<name>)` SHALL be written per comma-separated name.
9. **Subcircuit lines.** A line whose first field contains `.subckt` (case-insensitive) SHALL have its first ` m=<digits>` replaced by one space and every comma replaced by a space, and SHALL otherwise keep its spacing.
10. **Element expansion.** Any other line: let the names be the comma-separated items of field 1 (for the devices `ymemristor`, `ylin`, `ydelay`, `ytransline`, `ypgbr`, `ypowergridbranch`, `yacc`, `.model` and `.subckt`, field 1 repeated as many times as field 3 has items). A field matching `^(.*=)?\?-?[0-9]+$` is a width marker for the next field. For the i-th name (i from 1), write the prefix and the name, then each other field: a non-marker field as is; for a marker with width w (w = −1 means the item count of the next field) and next field with items a₁..a_k, write `<key=>` if the marker had one, then items a[((l + w·(i−1)) mod k) + 1] for l = 0..w−1. Every written field is followed by one space. A line with no names SHALL be written as an empty line.

Case-insensitive matches in steps 3 and 7 follow gawk on every platform, whatever the `xschem-compat` setting (Q-SCHEM-18, `Resolution: replicate`).

Compatibility: exact

(xschem ref: spice.awk:25-360)

#### Scenario: Vector instance on a bus
- **GIVEN** the element text `R1[1],R1[0] ?1 A[1],A[0] ?1 B 1k`
- **WHEN** it is post-processed
- **THEN** the lines SHALL be `R1[1] A[1] B 1k` and `R1[0] A[0] B 1k`

#### Scenario: sky130 transistor with prefix
- **GIVEN** the element text `**** spice_prefix X`, `M1 ?1 D ?1 G ?1 S ?1 B sky130_fd_pr__nfet_01v8 L=0.15 W=1`, `+ nf=1 ad=0.29`, `**** end_element`
- **WHEN** it is post-processed
- **THEN** the single output line SHALL be `XM1 D G S B sky130_fd_pr__nfet_01v8 L=0.15 W=1 nf=1 ad=0.29`

#### Scenario: Current save
- **GIVEN** a `vsource.sym` instance `V1` with `value=3 savecurrent=true` between nets `A` and `0`
- **WHEN** it is netlisted
- **THEN** the output SHALL be the line `V1 A 0 3` followed by `.save i(v1)`

#### Scenario: Trailing m=1 in user code
- **GIVEN** a `code_shown.sym` instance whose value is the line `.param m=1`, and a resistor line `R1 A B 1k m=1`
- **WHEN** the schematic is netlisted with `xschem-compat` ON, then OFF
- **THEN** with ON the lines SHALL be `.param` and `R1 A B 1k`; with OFF they SHALL be `.param m=1` and `R1 A B 1k`

### Requirement: Line breaking
Finally, every line SHALL be processed as xschem does with its `break.awk` script:
- trailing spaces and tabs SHALL be removed; an empty line stays empty
- lines inside user-code blocks, lines whose first field (lowercased) matches the unanchored regular expression `\.inc(lude)?|\.lib|\.title|\.save|\.write`, and lines matching `^\*\* ..._path:` SHALL NOT be broken
- other lines SHALL be broken so that no segment exceeds 130 characters before a break opportunity: a running column counter p starts at 0; for each field, p increases by the field's length plus the length of the whitespace before it; when p > 130, the text LF followed by `+` (or `*+` if the line starts with `*`) SHALL be emitted before that whitespace and p SHALL be reset to 0 (the field just placed is not counted)
- if a line contains `{`, `}` or `'`, or the quote state carried over from an earlier line is set, it SHALL instead be scanned character by character: each of `{`, `}` and `'` toggles a quote state that persists across lines; p counts characters; a break (LF then `+` or `*+`) SHALL be inserted before a space or tab when p > 130 and the quote state is off, and p SHALL be reset to 0. Both rules are xschem's and are kept whatever the `xschem-compat` setting, because they change only where long lines wrap (Q-SCHEM-20, `Resolution: replicate`)

Compatibility: exact

(xschem ref: break.awk:24-78)

#### Scenario: Long device line
- **GIVEN** a device line of 200 characters made of fields of 10 characters separated by single spaces, with no braces or quotes
- **WHEN** it is broken
- **THEN** the first output line SHALL end after the eleventh field (the counter reaches 131 at the twelfth field), the next line SHALL start with `+ ` followed by the twelfth field, and the following break SHALL come before the twenty-fourth field

### Requirement: Netlist file structure
The hierarchical SPICE netlist of top schematic `T` (path `/abs/T.sch`) SHALL consist of, in order:
1. `** sch_path: /abs/T.sch`, followed, only when `xschem-compat` is OFF, by the line `** xschem-compat=off` (capability `hefesto-magic-compat-mode`, Requirement "xschem-compat setting for xschem quirks")
2. Header code: the `netlist_commands` instances whose `place` (symbol property first, then instance property) is `header`, in instance order, each element text written as is (no markers in the final text).
3. Netlist options are applied (Requirement "Netlist options").
4. The top line: `**.subckt T` (or `.subckt T` when the top is a subcircuit, Requirement "LVS netlist mode"; `.SUBCKT` with `uppercase_subckt`), followed by the port list (Requirement "Top-level port order"), then LF.
5. The body of `T` (Requirement "Subcircuit body") at top level.
6. If any `netlist_commands` instance has no `place` of `header` or `end`, or the `S` property is non-empty: `**** begin user architecture code`, those instances' texts in instance order, then the `S` property followed by LF, then `**** end user architecture code`.
7. `**.ends` (`.ends` when the top is a subcircuit; `.ENDS` variants).
8. In `hier` mode, the subcircuit definitions (Requirement "Subcircuit definitions").
9. One `.GLOBAL <name>` line per global net.
10. If any `netlist_commands` instance has `place=end`, or any `device_model` was collected: `**** begin user architecture code`, the `place=end` texts in instance order, the `device_model` texts (Requirement "Device models"), `**** end user architecture code`.
11. `.end`, unless the top is a subcircuit.

All of this text then goes through Requirements "Element line post-processing" and "Line breaking". The `sch_path` and `sym_path` comment lines carry absolute paths on the netlisting host, as in xschem. A per-project setting "portable netlist paths" (capability `hefesto-settings`, default off) SHALL instead write each such path as `$PDK_ROOT/<path>` when the file is inside the active PDK root, and otherwise relative to the folder of the top schematic. The setting changes only these comment lines. Comparisons with xschem SHALL normalise these lines (Q-SCHEM-13, `Resolution: platform`).

Compatibility: exact

(xschem ref: spice_netlist.c:268-644, tests/SC_Test.spice)

#### Scenario: Flat testbench
- **GIVEN** a top schematic `SC_Test` whose only user code is one `code_shown.sym` instance and which has no ports
- **WHEN** it is netlisted
- **THEN** the netlist SHALL start with `** sch_path: <path>`, then `**.subckt SC_Test`, the device lines, `**** begin user architecture code`, the code text, `**** end user architecture code`, `**.ends`, `.end`

#### Scenario: Portable netlist paths
- **GIVEN** the setting "portable netlist paths" on, a top schematic `/home/u/proj/tb.sch`, and a subcircuit symbol `/pdks/sky130A/libs.tech/xschem/sky130_tests/not.sym` with the active PDK root at `/pdks`
- **WHEN** it is netlisted
- **THEN** the first line SHALL be `** sch_path: tb.sch`, the symbol's line SHALL be `** sym_path: $PDK_ROOT/sky130A/libs.tech/xschem/sky130_tests/not.sym`, and every other line SHALL be as with the setting off

### Requirement: Subcircuit body
The body of one schematic SHALL be written as:
1. For each non-ignored port instance (`ipin`, `opin`, `iopin`) in instance order: its element text (for the system symbols, `*.ipin <lab>`, `*.opin <lab>`, `*.iopin <lab>`). In LVS netlist mode or with `top_is_subckt`, instead one line `*.PININFO ` followed by `<lab>:<d> ` per port, with d = `I`, `O` or `B` for `ipin`, `opin`, `iopin`.
2. For each other non-ignored, non-shorted instance in instance order, skipping labels:
   - at top level, `netlist_commands` instances are skipped here (they are written per Requirement "Netlist file structure")
   - below top level, instances with `only_toplevel` true are skipped
   - below top level, a `netlist_commands` instance is written between `**** begin user architecture code` and `**** end user architecture code`
   - every other instance writes its element text; if it has a `device_model` (instance, else symbol), that text is collected (Requirement "Device models")

Compatibility: exact

(xschem ref: spice_netlist.c:183-266)

#### Scenario: Port comments first
- **GIVEN** a schematic whose instances in order are resistor `R1`, port `ipin A`, port `opin Y`
- **WHEN** it is netlisted
- **THEN** the body SHALL list `*.ipin A`, `*.opin Y`, then the `R1` line

### Requirement: Top-level port order
The top-level port list SHALL be taken from the symbol `<top base name>.sym` if it exists on the search path and has at least one pin: its format string SHALL be scanned like a subcircuit header (Requirement "Subcircuit definitions") and written after one space. Otherwise it SHALL be the `lab` of each port instance in instance order, sorted by `sim_pinnumber` when every port has one (unknown order on ties in xschem; Hefesto uses a stable sort that keeps instance order on ties and reports a warning for each duplicated value, whatever the `xschem-compat` setting; Q-SCHEM-5, `Resolution: fix`), each bus-expanded and written as ` <names>`. A mismatch between the symbol pins and the schematic ports SHALL be reported (Requirement "Errors and warnings").

Compatibility: exact

(xschem ref: spice_netlist.c:368-403, save.c:3878-3922)

#### Scenario: Port order from symbol
- **GIVEN** `inv.sch` with ports in instance order `Y`, `A`, and `inv.sym` with pins in order `A`, `Y` and format `@name @pinlist @symname`
- **WHEN** `inv.sch` is netlisted with `top_is_subckt`
- **THEN** the top line SHALL be `.subckt inv A Y`

### Requirement: Subcircuit definitions
In `hier` mode, after the top level, Hefesto SHALL write one definition per distinct subcircuit cell, in this order: walk the list of loaded symbols from the first; the list starts with the symbols used at top level in order of first use (then the variants created by instance `schematic` attributes), and each time a definition is written, the symbols first used inside it are appended to the end of the list. For each symbol S in the list:
- skip S if its symbol property has `spice_ignore` true, `open` or `short`, or, with option `lvs_ignore`, `lvs_ignore` set; or if it has no `type`; or if its type is not `subcircuit`
- if a definition with the same cell name (the symbol reference without folder and extension) was already written from a different symbol file: with `xschem-compat` ON, skip S, as xschem does; with `xschem-compat` OFF, write S under the unique name `<cell>_<k>`, with k the smallest integer from 1 for which no definition of that name was written, and write every instance of S with that name in place of the cell name. Both modes SHALL report a warning naming both symbol files (Q-SCHEM-14, `Resolution: compat`). A symbol whose definition was already written from the same file is skipped. A symbol with `default_schematic=ignore` is skipped and not recorded
- skip S if `spice_primitive` is true, or in LVS mode if `lvs_primitive` is true
- skip S if its format (as in Requirement "Format string substitution") is empty

Otherwise write:
1. an empty line, then `* expanding   symbol:  <reference> # of pins=<n>` (three spaces after `expanding`, two after `symbol:`)
2. `** sym_path: <absolute symbol path>`
3. if `spice_sym_def` is set: its value with `@` tokens substituted from the template (plus `symname`) and a LF, and nothing else
4. otherwise `** sch_path: <absolute schematic path>` (Requirement "Schematic resolution"), then the header line `.subckt <name> ` where `<name>` is the template's `model` if set, else the cell name, followed by:
   - the port names, from scanning the symbol format: `@spiceprefix` and `@name` are ignored; `@pinlist` writes each pin name (not `spice_ignore`, first of each distinct name, substituted in the parent context, `expr`-evaluated, bus-expanded) followed by a space; `@@pin` and `@#n` write that pin's expanded name and a space; any other `@token` that is followed by a separator writes the token name and a space; `@symname` or `@model` ends the scan; if the scan reaches the end of the format instead, a LF is written. A format starting with `tcleval(` is Tcl-evaluated first.
   - the template with the keys `name`, `spiceprefix` and `model` removed, values decoded, and all other characters (including separators) kept. Keys named in `extra` are also removed: with `xschem-compat` ON, every key that occurs as a substring of the symbol's `extra` value, as xschem does; with `xschem-compat` OFF, only keys that are whole words of `extra` (delimited by separators). Both modes SHALL report a warning for each template key that occurs in `extra` as a substring but not as a whole word (Q-SCHEM-19, `Resolution: compat`).
   - LFs of the template: with `xschem-compat` ON they are kept, as xschem does, which puts parameters on lines without `+`; with `xschem-compat` OFF each LF SHALL be written as one space, so the header is valid SPICE. Both modes SHALL report a warning naming the symbol whose template contains an LF (Q-SCHEM-21, `Resolution: compat`)
   - a LF
5. the body of the schematic (Requirement "Subcircuit body"), unless `spice_stop` is true
6. if the schematic's `S` property is non-empty: `**** begin user architecture code`, the property, `**** end user architecture code`
7. `.ends` and an empty line

Compatibility: exact

(xschem ref: spice_netlist.c:470-540, spice_netlist.c:646-780, token.c:2080-2245, token.c:576-668)

#### Scenario: Parametric subcircuit header
- **GIVEN** symbol `comp_ngspice.sym` with format `@name @pinlist @symname OFFSET=@OFFSET` and template `name=x1 OFFSET=0 AMPLITUDE=5`, pins PLUS, OUT, MINUS
- **WHEN** its definition is written
- **THEN** the header line SHALL be `.subckt comp_ngspice PLUS OUT MINUS  OFFSET=0 AMPLITUDE=5`, with two spaces before `OFFSET`

#### Scenario: Same cell name from two libraries
- **GIVEN** a top schematic using `libA/inv.sym` and then `libB/inv.sym`, both `type=subcircuit`
- **WHEN** it is netlisted
- **THEN** with `xschem-compat` ON only the definition from `libA/inv.sch` SHALL be written, and a warning SHALL name both symbols

#### Scenario: Same cell name from two libraries, xschem-compat OFF
- **GIVEN** the same top schematic and `xschem-compat` OFF
- **WHEN** it is netlisted
- **THEN** `.subckt inv` SHALL be written from `libA/inv.sch` and `.subckt inv_1` from `libB/inv.sch`, the instances of `libB/inv.sym` SHALL use `inv_1`, and a warning SHALL name both symbols

#### Scenario: extra key matched as a substring
- **GIVEN** a subcircuit symbol with `extra="VPWR VGND"` and template `name=x1 VPW=1 L=1`
- **WHEN** its definition is written
- **THEN** with `xschem-compat` ON `VPW=1` SHALL be dropped from the header; with `xschem-compat` OFF it SHALL be kept; in both modes a warning SHALL name `VPW`

#### Scenario: Multi-line template, xschem-compat OFF
- **GIVEN** a subcircuit symbol whose template is `name=x1` LF `W=1`, and `xschem-compat` OFF
- **WHEN** its definition is written
- **THEN** the header line SHALL end with ` W=1` on the same line, and a warning SHALL name the symbol

### Requirement: Schematic resolution
The schematic of a subcircuit instance or symbol SHALL be:
1. the instance's `schematic` attribute, with `@` tokens substituted from the instance, if non-empty; else
2. the symbol's `schematic` attribute, if non-empty;
3. in both cases with `@symname` replaced by the symbol cell name, Tcl-evaluated if in `tcleval(...)` form, and resolved like a symbol reference (capability `schematic-symbols-and-libraries`) unless it is a generator;
4. otherwise the symbol's reference with its extension replaced by `.sch`, in the same folder as the resolved symbol file.

An instance whose `schematic` attribute differs from its symbol's SHALL produce a separate subcircuit definition, one per distinct `schematic` value, named after that schematic's cell name; the exact naming is fixed by golden case SCHEM-NL-15 (Q-SCHEM-28, `Resolution: replicate`). A missing schematic SHALL produce a definition with its header lines and an empty body, and SHALL report a warning; the exact text is fixed by golden case SCHEM-NL-14 (Q-SCHEM-27, `Resolution: replicate`). Both rules apply whatever the `xschem-compat` setting.

Compatibility: exact

(xschem ref: actions.c:2474-2580)

#### Scenario: Instance-based implementation
- **GIVEN** two instances of `amp.sym`, one with `schematic=amp_fast.sch`
- **WHEN** the hierarchical netlist is generated
- **THEN** both `.subckt amp` and `.subckt amp_fast` SHALL be written (verify against xschem)

### Requirement: Device models
The value of `device_model` of each netlisted instance (instance property first, else symbol property; `@` tokens substituted, else Tcl-evaluated) SHALL be collected once per key. The key SHALL be computed from the lowercased text: if it contains `.subckt` or `.model`, the two words after it concatenated; otherwise the first two words concatenated; or the whole lowercased text if fewer than two words exist. A later value with the same key SHALL replace the earlier one. Values SHALL be written in this order: compute h = 5381 and, for each byte c of the key, h = (h × 33 + c) mod 2³²; bucket = h mod 367; write buckets 0 to 366 in order, and within a bucket in order of first insertion.

Compatibility: exact

(xschem ref: spice_netlist.c:155-181, spice_netlist.c:237-258, spice_netlist.c:600-612, token.c:28-37)

#### Scenario: Duplicate model printed once
- **GIVEN** two instances with `device_model=".model D1N4148 D(Is=2.52n)"`
- **WHEN** the schematic is netlisted
- **THEN** that `.model` line SHALL appear once, inside the final user-code block

### Requirement: LVS netlist mode
With option `lvs_netlist`, the netlister SHALL:
- use `lvs_format` instead of `format` wherever it exists (Requirement "Format string substitution"), including for subcircuit port scanning
- treat the top level as a subcircuit: write `.subckt T ...` and `.ends` without the `**` prefix, and write no `.end`
- write `*.PININFO` lines instead of port comment lines (Requirement "Subcircuit body") at every level
- not expand symbols with `lvs_primitive` true

Option `top_is_subckt` SHALL have the same effect on the top level and `*.PININFO` lines, without switching to `lvs_format`. PDK flows typically combine LVS mode with option `lvs_ignore` to drop simulation-only instances. The output is a netlist that capability `lvs-netlist-input` reads as the schematic side of LVS.

Compatibility: exact

(xschem ref: spice_netlist.c:183-230, spice_netlist.c:290-300, spice_netlist.c:356-366, spice_netlist.c:620-625)

#### Scenario: LVS inverter
- **GIVEN** `inv.sch` with ports `A` (ipin) and `Y` (opin), labels `VDD`, `VSS`, and sky130 `nfet_01v8`/`pfet_01v8` instances, in LVS mode with no `inv.sym`
- **WHEN** it is netlisted
- **THEN** the netlist SHALL contain `.subckt inv A Y`, then `*.PININFO A:I Y:O`, then device lines using `lvs_format` (no `ad=` or `nrd=` parameters), then `.ends`, and SHALL NOT contain `.end`

### Requirement: Flat netlist
In `flat` mode, Hefesto SHALL produce a netlist with no subcircuit hierarchy for the subcircuits defined in the schematic: every instance of a defined subcircuit is replaced by its body; port nets map to the parent nets; global nets and `0` are not renamed; device lines of undefined subcircuits (for example PDK model subcircuits) stay `X` lines. Instance names SHALL be suffixed `_<path>` and local net names prefixed `<path>_`, where the path joins the instance names from the top with `_`.

The flat netlist SHALL be electrically equivalent to the hierarchical one and to xschem's flat netlist: the same devices, the same connections, and the same model names and parameter values. It is not byte-compared: the golden comparison SHALL parse both netlists into a canonical form (devices with their resolved nodes, models and parameter values, in a canonical order, with names compared case-insensitively as SPICE reads them) and compare those. Any other property of xschem's flat text, such as its letter case, is informative and is not reproduced (Q-SCHEM-15, `Resolution: replicate`, behavioural).

Compatibility: behavioral

(xschem ref: flatten.awk:36-312, xschem.tcl:1006-1013)

#### Scenario: Flattened inverter chain
- **GIVEN** a top schematic with two instances `x1`, `x2` of subcircuit `inv`
- **WHEN** a flat netlist is generated
- **THEN** it SHALL contain no `.subckt` line for `inv`, and the transistors of both instances SHALL appear with distinct names

#### Scenario: Flat netlist compared behaviourally
- **GIVEN** golden case SCHEM-NL-21 (sky130 `top.sch` netlisted flat by xschem and by Hefesto)
- **WHEN** the outputs are compared
- **THEN** the comparison SHALL pass when the canonical parsed forms are equal, even if the texts differ in letter case or line order

### Requirement: Errors and warnings
Netlisting SHALL report, without stopping:
- shorts between differently named nets (Requirement "Net naming order")
- instances with no `name` whose template has no `name` either, except for types `package`, `port_attributes`, `architecture`, `arch_declarations`, `attributes`, `netlist_options` and `use`: `Warning: instance: <name> (<symbol>): no name attribute set`
- symbols with no `type`: `Warning: Symbol: <symbol>: no type attribute set`
- unconnected pins (Requirement "Unnamed nets")
- instances with all pins unconnected, overlapping identical instances, and pin mismatches between a symbol and its schematic
- bus expansion syntax errors
- the warnings that the rules above give in both modes of `xschem-compat`: duplicate `sim_pinnumber` values (Q-SCHEM-5), two subcircuits with one cell name (Q-SCHEM-14), template keys found in `extra` only as substrings (Q-SCHEM-19), templates with LFs (Q-SCHEM-21), numbers whose reading depends on `xschem-compat` (Q-SCHEM-12), and undefined tokens with a defined identifier prefix (Q-SCHEM-26)

A run that found a short, a pin mismatch or another error SHALL return a failure status (xschem exit code 10), and the offending objects SHALL be highlighted in the editor (capability `schematic-editing`). Warnings alone SHALL NOT fail the run. The message wording is informative.

Compatibility: behavioral

(xschem ref: netlist.c:1430-1460, netlist.c:1791-1990, spice_netlist.c:636-641)

#### Scenario: Netlist with a short
- **GIVEN** a schematic where labels `A` and `B` are on one wire
- **WHEN** it is netlisted from a script
- **THEN** the netlist SHALL be produced and the command SHALL return a failure status

### Requirement: In-process netlisting
Hefesto SHALL generate the netlist in memory, on every platform, without temporary files or external programs. It SHALL write the netlist to a file only when asked: by default `<netlist folder>/<top cell>.spice`, where the netlist folder is a Hefesto setting (default: a `simulation` folder beside the schematic, inside the document's granted folder on iPadOS). The output SHALL be identical on every platform for the same inputs and the same resolved absolute paths. Netlisting SHALL NOT change the document's undo history.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: iPad netlist in memory
- **GIVEN** a testbench open on iPadOS
- **WHEN** a simulation starts from it
- **THEN** the netlist SHALL be passed to the simulation engine in memory and no `.spice` file SHALL be created
