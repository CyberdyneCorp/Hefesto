# Schematic Netlisting

## Purpose
Defines how Hefesto turns a schematic hierarchy into a SPICE netlist that is identical to the one xschem produces (decision P-12). The netlist feeds embedded simulation (capabilities `simulation-*`) and LVS (capabilities `lvs-*`). This covers:
- connectivity: wires, pins, labels, pass-through and shorted symbols, bus taps and global nets
- net naming, including unnamed nets and unconnected pins
- bus and vector notation
- `format` string substitution (`@name`, `@pinlist`, `@@pin`, `@#n`, `@key`, `%key`, `@spiceprefix`, `tcleval`, `expr(...)`)
- the rules that turn substituted element text into netlist lines: continuation lines, vector expansion, renames and line breaking
- the file structure of hierarchical, top-level-only and flat netlists, `.subckt` generation and ordering
- netlist options and the LVS netlist mode used by PDK LVS flows

Only the SPICE netlister is in scope. xschem's Verilog, VHDL, Spectre and tEDAx netlisters are out of scope; their records and `*_format` attributes are kept through round trips, and Verilog is revisited with P-17 (Q-SCHEM-4, `Resolution: other`).

**Compatibility adaptation.** This capability is about xschem, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **xschem** (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`, version `3.4.8RC`):
- `exact`: byte-identical netlist text
- `behavioral`: same circuit; text MAY differ as stated
- `informative`: xschem detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-12)`

xschem quirks whose open-question entry records `Resolution: compat` are governed by the `xschem-compat` setting (capability `hefesto-magic-compat-mode`, default ON). The requirements that cite them state both modes; `exact` refers to the output with `xschem-compat` ON, which is the default. Warnings that an entry gives in both modes are reported whatever the setting.

Traces use `(xschem ref: path/file:line)`, relative to the xschem `src/` directory. They exist for traceability only. Implementers SHALL work from this spec and SHALL NOT consult the GPL sources (capability `hefesto-licensing`). Hefesto SHALL produce the final text directly; it SHALL NOT run awk or any external program.

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

A `netlist_options` instance SHALL apply each of these keys that it defines: `bus_replacement_char` (only if exactly 2 characters), `lvs_ignore`, `lvs_netlist`, `top_is_subckt`, `spiceprefix` (false only for a false value; any other value is true) and `hiersep`. They take effect when that instance is reached, in instance order, before the top-level `.subckt` line is written. One exception: `top_is_subckt` set by an instance SHALL select the `*.PININFO` port lines of Requirement "Subcircuit body" but SHALL NOT change the top line, the closing line or the final `.end`, which stay `**.subckt`, `**.ends` and `.end` (golden case SCHEM-NL-17). Set as a run option, `top_is_subckt` changes them as Requirement "LVS netlist mode" says.

Compatibility: exact

(xschem ref: netlist.c:598-641, spice_netlist.c:268-310, xschem.tcl:11926-11947)

#### Scenario: Options instance enables LVS mode
- **GIVEN** a top schematic containing a `netlist_options.sym` instance with `lvs_netlist=true`
- **WHEN** it is netlisted with default options
- **THEN** the netlist SHALL be produced in LVS netlist mode

#### Scenario: Options instance with top_is_subckt
- **GIVEN** top schematic `nl17` with a `netlist_options.sym` instance `bus_replacement_char=<> top_is_subckt=true`, an `ipin` `IN[1:0]`, an `opin` `OUT[1:0]` and a resistor `R1[1:0]` with `value=1k` between them
- **WHEN** it is netlisted with default options
- **THEN** the netlist SHALL be `** sch_path: <path>`, `**.subckt nl17 IN<1>,IN<0> OUT<1>,OUT<0>`, `*.PININFO IN[1:0]:I OUT[1:0]:O`, `R1<1> IN<1> OUT<1> 1k`, `R1<0> IN<0> OUT<0> 1k`, `**.ends`, `.end`; with the run option `uppercase_subckt` as well, the top and closing lines SHALL be `**.SUBCKT nl17 IN<1>,IN<0> OUT<1>,OUT<0>` and `**.ENDS` (golden case SCHEM-NL-17)

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

When the spreading reaches a wire or pin that already has a different name, the existing name SHALL be kept and the netlist SHALL still be written. This is a short, reported as `Error: <tag> shorted: <a> - <b>` (Requirement "Errors and warnings"), where `<a>` is the name being spread and `<b>` the name already there, and the tag says where the two met:

| Tag | When |
|---|---|
| `Net` | a label or port pin (step 1) names a wire it touches that already has a different name |
| `Net to net` | a name spreading through wires reaches a connected wire that already has a different name |
| `net to named instance pin` | a name spreading through wires reaches an instance pin that touches the wire and already has a different name |
| `Instance pin` | a name given to an instance pin (by a label or port, a wire, or another pin) reaches another instance's pin at the same point that already has a different name |
| `Pass_through symbol` | two pins of one instance share a `name` and both already have nets, with different names (Requirement "Pass-through and shorted symbols") |
| `Bus tap` | the tap pin of a bus tap already has a name (Requirement "Bus taps") |

A short is reported only when the two names differ. Every short SHALL be reported, including shorts at the top level during its first connectivity pass and in runs that do not netlist the hierarchy, which xschem does not always report; the netlist text is the same either way (Q-SCHEM-16, `Resolution: fix`).

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

A pin that has no net at all (for example on an instance whose node data is missing) SHALL be written `__UNCONNECTED_PIN__<k>`, or `__UNCONNECTED_PIN__<k>_[w−1..0]` expanded for a pin of width w > 1, with k counting from 0 over the whole netlist run (it starts at 0 for each run and is never reset inside one). k SHALL advance by one each time such a pin's net is written, once per token occurrence and not once per bit or per pin: a pin written by both `@pinlist` and `@#0` takes two numbers, and a bus pin takes one, and SHALL report the warning `Warning: unconnected pin,  Inst idx: <i>, Pin idx: <j>  Inst:<name>`.

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
For an instance of type `bus_tap` with at least two pins, pin 1 connects to a bus and pin 0 is the tap. When pin 1 gets a net name `bus`, pin 0 SHALL be renamed, unless it already has a non-automatic name. An **automatic** name is one that starts with `#` (Requirement "Unnamed nets"); a name derived from an automatic name without the `#` (for example `net3[2]` built from a bus `#net3`) is not automatic. The tap name is:
- if the instance's `lab` starts with `[` or is all digits, the bus base name (the part of `bus` before `[`, taken after the last `,` and its following spaces if the bus is a list) followed by `lab`
- otherwise `lab`

An empty or missing `lab` gives no tap name: pin 0 stays unnamed and later gets an automatic name (step 3 of Requirement "Net naming order"). A tap name spreads from pin 0.

When pin 0 already has a non-automatic name, that name SHALL be kept and pin 0 SHALL NOT be renamed. The `Bus tap` short SHALL be reported only when the tap name computed from `lab` differs from the name pin 0 already has, as `Error: Bus tap shorted: <tap name> - <pin 0 name>`, naming those two, in both modes (Q-SCHEM-37, `Resolution: fix`, approved by the user 2026-11-25). An empty or missing `lab` gives no tap name and so no `Bus tap` short. Like every short, it fails the netlist run (Q-SCHEM-16). The netlist text does not depend on this rule; only the diagnostic and the exit status do.

Informative: xschem reports `Error: Bus tap shorted: <bus> - <pin 0 name>` whenever the bus net and the name pin 0 already has differ, which is almost always (a bus name is not a bit name), so it reports correct schematics as shorted. Hefesto does not replicate this in either mode.

Compatibility: exact

(xschem ref: netlist.c:1330-1372)

#### Scenario: Bit tap
- **GIVEN** a bus tap whose pin 1 is on net `DATA[15:0]` and whose `lab` is `[3]`
- **WHEN** the schematic is netlisted
- **THEN** its pin 0 net SHALL be `DATA[3]`

#### Scenario: Tap with an empty lab
- **GIVEN** a bus tap whose pin 1 is on net `DATA[3:0]`, whose `lab` is empty, and whose pin 0 touches only a resistor pin
- **WHEN** the schematic is netlisted
- **THEN** pin 0 SHALL get an automatic name (`net<N>`) and no short SHALL be reported

#### Scenario: Tap side already named
- **GIVEN** a bus tap with `lab=[3]` whose pin 1 is on net `DATA[3:0]` and whose pin 0 wire carries a label `DATA[3]` that comes before the bus label in instance order, and a second such tap with `lab=[2]` whose pin 0 wire carries a label `X`
- **WHEN** the schematic is netlisted
- **THEN** the pin 0 nets SHALL be `DATA[3]` and `X`, and exactly one short SHALL be reported, `Error: Bus tap shorted: DATA[2] - X`, with `xschem-compat` ON or OFF (Q-SCHEM-37)

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

Lexical rules:
- **Names.** A `name` is a run of letters, digits and the characters `_ % $ ~ " + # ! / \ < > ? . :` and `-`; a `-` starts a name only when more name characters follow it. Blanks (space, tab, LF) between name characters belong to the name (`a b` is one name); blanks between other tokens are ignored (`a , b` is `a,b`).
- **Integers** (`int`, `i`, `j`, `step`, `ext`, `rep`) are an optional `-` followed by decimal digits, so indices MAY be negative: `A[1:-1]` is `A[1],A[0],A[-1]` and `Q[0..-1]` is `Q0,Q-1`.
- **Trailer** (`item-suffix`): characters right after `]` that are not `*`, `,` or `)` start a trailer, a run of name characters, blanks, `(`, `)` and `*`; it is appended to every element (`A[1:0]_n` is `A[1]_n,A[0]_n`).
- **Precedence.** `*` binds tighter than `,`, and both group from the left: `2*a,b` is `a,a,b` and `a,b*2` is `a,b,b`; parentheses group explicitly.

Values that xschem does not handle (it loops forever, reads undefined memory or gives an empty list) SHALL be syntax errors, in both modes (Q-SCHEM-35, `Resolution: fix`, approved by the user 2026-11-25): a repetition count of 0 or below (`a*0`, `0*a`, `a*-1`; xschem gives an empty list of width 0 for 0), a `step` of 0 or below, and a `rep` of 0 or below. A stepped range `i:j:step` or `i..j..step` with i = j is the single element i, in both modes (xschem loops forever; Q-SCHEM-35).

**Input limits** (approved by the user on 2026-11-21, Q-SCHEM-32; both modes): one expansion SHALL give at most 1,048,576 (2^20) names (`kMaxBusBits`) and at most 67,108,864 bytes (64 MiB, `kMaxBusBytes`) of expanded text, commas included; parentheses SHALL nest at most 256 deep; index and count arithmetic SHALL NOT overflow. Input past a limit SHALL be used unchanged with unknown width, like a syntax error, and SHALL report the warning `bus expansion of <string> exceeds a limit (1048576 names, 67108864 bytes or 256 nested parentheses); used unchanged` (`SCHEM-NET-006`), where `<string>` is cut to its first 80 bytes followed by `...` when longer.

**Total expansion** (approved by the user on 2026-11-22, Q-SCHEM-41, decision M8-D3; both modes): the expanded text of all bus expansions of one netlist run, or of one editing session, SHALL total at most 4,294,967,296 bytes (4 GiB, 64 × `kMaxBusBytes`), commas included. A name whose expansion would take the total past that limit SHALL be used unchanged with unknown width and SHALL report the same `SCHEM-NET-006` warning as a single name past `kMaxBusBytes`; names expanded before it keep their expansion. The total starts at 0 for each netlist run and for each editing session.

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

#### Scenario: Precedence, blanks and negative indices
- **WHEN** `2*a,b`, `a , b*2`, `A[1:-1]` and `D[1:0]_n` are expanded
- **THEN** the results SHALL be `a,a,b`, `a,b,b`, `A[1],A[0],A[-1]` and `D[1]_n,D[0]_n`

#### Scenario: Zero and negative counts are syntax errors
- **WHEN** `a*0`, `0*a`, `D[7:0:0]`, `D[7:0:-2]` and `D[3:0:1:0]` are expanded, with `xschem-compat` ON or OFF
- **THEN** each SHALL report `syntax error in <string>` and SHALL be used unchanged with unknown width (Q-SCHEM-35)

#### Scenario: Stepped range with equal ends
- **WHEN** `D[3:3:2]` and `Q[3..3..2]` are expanded, with `xschem-compat` ON or OFF
- **THEN** the results SHALL be `D[3]` and `Q3`, each with width 1 (Q-SCHEM-35)

#### Scenario: Expansion past the limits
- **WHEN** `A[1048576:0]`, a 128-byte name followed by `[1048575:0]` (more than 64 MiB of text) and a name inside 300 nested parentheses are expanded, with `xschem-compat` ON or OFF
- **THEN** each SHALL be used unchanged with unknown width and SHALL report the `SCHEM-NET-006` warning, with work and memory bounded by the limits

#### Scenario: Total bus expansion limit
- **GIVEN** a netlist run in which 64 names each expand to 67,108,864 bytes or slightly less, so that their total is just under 4,294,967,296 bytes, followed by a name whose expansion would take the total past it, with `xschem-compat` ON or OFF
- **WHEN** the names are expanded
- **THEN** the first 64 names SHALL be expanded, the last name SHALL be used unchanged with unknown width and SHALL report the `SCHEM-NET-006` warning, and the run SHALL continue (Q-SCHEM-41)

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
- A token starts at any unescaped `@` or `%`, wherever it stands (also inside a word, as in `x@a`). Its first character is the `@` or `%`, and its second character always belongs to it, whatever it is: an ending is looked for only from the token's second character on, that is, at the third character of the token or later counting the `@` as the first. So in `a @ b c` the token is `@ b`, and `@@` starts the token `@@...`. It ends before the first such character that is an unescaped separator (space, tab, LF, `;`, end of string), an unescaped `@` or `%`, or an escaped character other than a separator, `@` or `%`. A lone `@` or `%` at the end of the string produces nothing. This is xschem's rule, kept whatever the `xschem-compat` setting because PDK formats rely on it (`@spice_get_voltage(net)`). When a token's lookup fails but a shorter prefix of the token made of identifier characters (letters, digits, `_`) names a defined key, Hefesto SHALL report a warning naming the token and that prefix (Q-SCHEM-26, `Resolution: replicate`).
- The ending character is copied to the output unless it is `@` or `%` (which starts the next token) or the end of the string. Characters outside tokens are copied.
- A LF is appended at the end.

Token values:

| Token | Output |
|---|---|
| `@pinlist` | For each pin in pin order that does not have `spice_ignore` true, only the first pin of each distinct name: `?<w> <nets> `, where w is the pin's width (bus expansion of its name) and `<nets>` is the comma-separated expansion of the attached net |
| `@@<pin>` | The same `?<w> <nets> ` for the first pin named `<pin>`, if it does not have `spice_ignore` true |
| `@#<n>` or `@#<pin>` | The same for pin number n (0-based) or the named pin. A number below 0 or not below the pin count, or a name that is no pin, produces nothing (no text, no warning) |
| `@#<n>:<attr>` | The pin attribute value (capability `schematic-symbols-and-libraries`, Requirement "Text substitution tokens"); `--UNDEF--` if the pin exists but has no such attribute; nothing if the pin does not exist (out-of-range number or unknown name); for `pinnumber` with an instance name ending in `:<k>` and a value containing `:`, the k-th `:`-separated item |
| `@symname`, `@symname_ext`, `@symref`, `@schname`, `@schname_ext`, `@topschname`, `@index` | As in capability `schematic-symbols-and-libraries`; `@symname` sanitised like a generator name. `@topschname` is the file name of the top schematic of the netlist run, without its folder and without its extension (the text after its last `.`), for example `inv_tb` for `/w/inv_tb.sch`; when the name contains `(`, every `.` becomes `_` and no extension is removed |
| `@savecurrent` | If the instance's (else template's) `savecurrent` is true: LF `.save I( ?1 <instance name> )`; else nothing |
| `@spiceprefix` | Nothing is written. With option `spiceprefix` on, its value v (Requirement "Hierarchical parameters"), if non-empty, becomes the element's spice prefix, which Requirement "Element line post-processing" puts before each element name |
| `@tcleval(...)` | Tcl evaluation (capability `schematic-symbols-and-libraries`, Requirement "tcleval values") |
| `@<key>` | The value of `key` (Requirement "Hierarchical parameters"), then `expr` evaluation. If unresolved, nothing. `@name` and `@lab` values are bus-expanded. |
| `%<key>` | As `@key`, but if `key` is defined nowhere, the text `<key>` |

After the whole line is built: if it begins with `tcleval(`, it SHALL be Tcl-evaluated, and the line's terminating LF SHALL be appended after the result (so a format `tcleval( @value )` gives the result, blanks included, then LF; golden cases SCHEM-NL-07 and NL-08, clarified 2026-11-24 from the capture); then `expr` evaluation SHALL be applied to the whole line. The text one instance produces (one line, or several with continuation lines) is one **element**; its spice prefix applies to that element only.

Compatibility: exact

(xschem ref: token.c:2426-2816)

#### Scenario: Resistor element text
- **GIVEN** `res.sym` (`format="@name @pinlist @value m=@m"`) with instance `name=R1 value=1k m=1`, pin P on net `A`, pin M on net `B`
- **WHEN** the element text is built
- **THEN** it SHALL be `R1 ?1 A ?1 B  1k m=1` (two spaces before `1k`, because `@pinlist` ends with a space and the space after the token is copied) followed by LF, and the netlist line SHALL be `R1 A B 1k`

#### Scenario: Token start and lone markers
- **GIVEN** a format `a @ b c@` and an instance with no key `b`
- **WHEN** it is substituted
- **THEN** the token SHALL be `@ b`, it SHALL give nothing, the space after it SHALL be copied, the final lone `@` SHALL give nothing, and the output SHALL be `a  c` followed by LF

#### Scenario: Out-of-range pin number
- **GIVEN** a two-pin symbol with format `@name X@#5 Y@#5:name Z@#1:foo` and an instance `R1`
- **WHEN** the element text is built
- **THEN** it SHALL be `R1 X Y Z--UNDEF--` followed by LF, with no warning for `@#5`

#### Scenario: Top schematic name
- **GIVEN** a netlist run of `/w/inv_tb.sch` whose subcircuit schematic `inv.sch` holds an instance of a symbol whose format is `* top=@topschname`
- **WHEN** the netlist is written
- **THEN** that instance's text inside `.subckt inv` SHALL be `* top=inv_tb`

#### Scenario: Token ends at whitespace only
- **GIVEN** a format containing `@W/@L` and an instance with `W=2 L=1`
- **WHEN** it is substituted
- **THEN** the token `@W/` SHALL be unresolved and give nothing, the output SHALL be `1`, and a warning SHALL say that `@W/` is undefined while `W` is defined

### Requirement: Hierarchical parameters
The value of `@key` for an instance SHALL be found as follows, where "lookup in S1, S2, ..." means: the first source that defines the key gives the value v; each later source that defines a key equal to v replaces v with that key's value:
1. v = the result of substituting the token in the instance property string only (unresolved tokens are kept as text). The symbol template is NOT consulted in this step: it comes in step 2, after the parent instance.
2. If v still contains `@` or `%`: substitute v by lookup in the instance properties, then the properties of the parent instance (the instance through which the netlister descended into the current schematic; none at top level), then the symbol template.
3. If v still contains `@` or `%`: substitute v in the instance properties without predefined tokens; then, if still needed, by lookup in the instance properties and then the parent symbol's template, keeping backslash escapes.
4. If v still starts with `@`, the value is empty.

Pin names, `lab` values and instance names that contain `@` or `%` SHALL be substituted in the context of the parent instances, from the nearest parent outwards, before bus expansion and `expr` evaluation.

Compatibility: exact

(xschem ref: token.c:2690-2760, token.c:5709-5800, token.c:5223-5297, netlist.c:971-1015)

#### Scenario: Template default found in step 2
- **GIVEN** a top-level instance `R1` of a symbol whose template is `name=R1 value=1k` and whose format is `@name @pinlist @value`, the instance property string `name=R1`, and no parent instance
- **WHEN** the element text is built
- **THEN** step 1 SHALL leave `@value` unresolved, step 2 SHALL find `value=1k` in the template, and the element SHALL end in `1k`

#### Scenario: Model passed down from the parent template
- **GIVEN** `passgate.sym` with template `... modeln=nfet_01v8`, and inside `passgate.sch` an nmos instance with `model=@modeln` and format token `@model`
- **WHEN** the hierarchical netlist is generated
- **THEN** the nmos line inside `.subckt passgate` SHALL use `nfet_01v8`

### Requirement: expr evaluation
Text SHALL be scanned for the markers `expr(`, `expr_eng(` and `expr_eng4(`. Text outside markers SHALL be copied. After a marker, an arithmetic expression SHALL be parsed up to the matching `)`, optionally enclosed in single quotes (`expr('...')`):
- **numbers**: a run of the characters `.0-9a-zA-Z_-` starting with a digit or `.`, converted by a C `%lf` scan, then multiplied by the SI suffix that starts the rest of the run: `T` 1e12, `G` 1e9, `x`/`X` or `meg` (case-insensitive) or `M` 1e6, `m` 1e-3, `k` 1e3, `u` 1e-6, `n` 1e-9, `p` 1e-12, `f` 1e-15, `a` 1e-18. With `xschem-compat` ON, `-` belongs to the run anywhere after its first character, as in xschem, so `2-1` is read as the number 2. With `xschem-compat` OFF, a `-` belongs to the run only directly after an `e` or `E` (an exponent sign), so `2-1` is the number 2 minus the number 1. In both modes, a run as read with `xschem-compat` ON that contains a `-` not directly after `e` or `E` SHALL be reported as a warning, because the two modes read it differently (Q-SCHEM-12, `Resolution: compat`).
- **constants**: `pi`, `e`, `k` (1.380649e-23), `h` (6.62607e-34), `echarge` (1.60217646e-19), `abszero` (273.15), `c` (2.99792458e8)
- **functions** of one argument: `int` (truncate toward zero, in floating point: no integer conversion, so any finite value works), `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `log` (base 10), `ln`, `exp`, `sqrt`, `round6`, `round1` (round to 6 or 1 significant digits), `round`
- **rounding**: `round(x)` rounds half away from zero, computed as floor(x + 0.5) for x > 0 and ceil(x − 0.5) for x < 0 (so 2.5 gives 3, −2.5 gives −3, and 0.49999999999999994 gives 1, as the addition rounds up); 0, −0, NaN and infinities are returned unchanged. `round6(x)` and `round1(x)` (n = 6 or 1) return x unchanged when x is 0; otherwise they take s = 10^(ceil(log10(|x|)) − n) and return round(x / s) · s, so −123 with n = 2 would give −120 and 9.96 gives 10. NaN gives NaN, and an infinite x gives NaN
- **operators**, lowest to highest precedence: `?:` (right-assoc), `==` and `!=`, `<` `>` `<=` `>=`, `+` and `-`, `*` `/` `%`, unary minus, `^` (power by C `pow`, right-assoc), parentheses
- **`%`** truncates both operands toward zero and gives the remainder with the sign of the left operand (`7 % 3` is 1, `-7.9 % 2` is −1, `7 % 0.5` uses 0 as the right operand). xschem converts the operands to C `int`, so a right operand that truncates to 0, or an operand outside the 32-bit range, is undefined (a crash on most hosts). Hefesto SHALL compute fmod(trunc(a), trunc(b)) in binary64, which equals C's `%` for every pair xschem handles, gives NaN when trunc(b) is 0, and is exact for operands up to 2^53, in both modes (Q-SCHEM-36, `Resolution: fix` option (a), approved by the user 2026-11-25)
- **nesting**: parentheses, function calls and unary operators SHALL nest at most **200** levels (`kMaxExprDepth`), in both modes (input limit approved by the user on 2026-11-21, Q-SCHEM-32). Deeper input SHALL fail like a syntax error (the errors rule below) and SHALL report the warning `expr nested deeper than 200 levels; text copied without evaluation` (`SCHEM-NET-006`)
- **NaN and infinity**: an infinite result prints `inf` or `-inf` with `expr`; a NaN result prints `nan` whatever its sign bit, on every host. xschem prints the C library's text, which on the GNU C library is `-nan` when the sign bit is set, and whether an invalid operation such as 0/0 sets it depends on the processor, so no single xschem text exists; Hefesto SHALL always print `nan`, in both modes, so netlists are identical on every platform (Q-SCHEM-36 option (a), D20). With `expr_eng` and `expr_eng4`, xschem's engineering formatter sends infinities to the `T` branch and NaN to the `a` branch: Hefesto SHALL print `infT`, `-infT` and `nana`
- **result**: `expr` prints with C `%.15g`; `expr_eng` prints in engineering notation with the `ev_precision` setting (default 4) significant digits; `expr_eng4` with 4. Engineering notation divides by 1e12, 1e9, 1e6 or 1e3 for |x| above 0.999999 times that power (suffix `T`, `G`, `MEG`, `k`), keeps the value for |x| > 0.0999999, else multiplies by 1e3 ... 1e18 (suffix `m`, `u`, `n`, `p`, `f`, `a`); |x| < 0.999999e-23 prints `0`.
- **errors**: on a syntax error or an unknown identifier, the rest of the text from the failing expression on SHALL be copied with every `expr(`, `expr_eng(` or `expr_eng4(` marker and its matching `)` removed; the single quotes of `expr('...')` are kept.

Compatibility: exact

(xschem ref: eval_expr.y:1-460, editprop.c:536-621, token.c:964-973)

#### Scenario: sky130 drain area
- **GIVEN** the sky130 nfet template `ad="expr('int((@nf + 1)/2) * @W / @nf * 0.29')"` with `nf=1` and `W=1`
- **WHEN** `@ad` is substituted
- **THEN** it SHALL give `0.29`

#### Scenario: Parameter left symbolic
- **GIVEN** the same template with `W=W_N`
- **WHEN** `@ad` is substituted
- **THEN** it SHALL give `'int((1 + 1)/2) * W_N / 1 * 0.29'`: the marker and its `)` are removed and the single quotes are kept (errors rule above; golden case SCHEM-NL-03, corrected 2026-11-24 from the capture)

#### Scenario: Minus inside a number, xschem-compat ON
- **GIVEN** `xschem-compat` ON
- **WHEN** `expr(2-1)` and `expr(1e-3-1)` are evaluated
- **THEN** they SHALL give `2` and `0.001`, and a warning SHALL be reported for each

#### Scenario: Minus inside a number, xschem-compat OFF
- **GIVEN** `xschem-compat` OFF
- **WHEN** `expr(2-1)`, `expr(1e-3-1)` and `expr(2 - 1)` are evaluated
- **THEN** they SHALL give `1`, `-0.999` and `1`, and a warning SHALL be reported for the first two only

#### Scenario: Rounding functions
- **WHEN** `expr(round(2.5))`, `expr(round(-2.5))`, `expr(round6(1.23456789))`, `expr(round1(-123))` and `expr(round1(9.96))` are evaluated
- **THEN** they SHALL give `3`, `-3`, `1.23457`, `-100` and `10`

#### Scenario: Remainder by zero and NaN text
- **GIVEN** `xschem-compat` ON or OFF
- **WHEN** `expr(7%3)`, `expr(-7.9%2)`, `expr(7%0.5)`, `expr(0/0)` and `expr_eng(0/0)` are evaluated
- **THEN** they SHALL give `1`, `-1`, `nan` (no crash), `nan` and `nana`, on every platform (Q-SCHEM-36)

#### Scenario: Nesting past 200 levels
- **GIVEN** `xschem-compat` ON or OFF
- **WHEN** `expr(` followed by 201 `(`, `1`, 201 `)` and `)` is evaluated
- **THEN** the evaluation SHALL fail like a syntax error, the text SHALL be copied with the `expr(` marker and its `)` removed, and the warning `expr nested deeper than 200 levels; text copied without evaluation` SHALL be reported

#### Scenario: Engineering notation and division by zero
- **GIVEN** golden case SCHEM-NL-13
- **WHEN** `expr_eng(1.5e-7)`, `expr_eng4(123456)`, `expr(W_N*2)` and `expr(1/0)` are evaluated with the default `ev_precision`
- **THEN** they SHALL give `150n` (trailing zeros are not printed), `123.5k`, `W_N*2` (unknown identifier, the text is copied without the marker) and `inf` (the `%.15g` text of an infinite result)

### Requirement: Element line post-processing
The netlist text (the element texts of Requirement "Format string substitution", the user code and the lines of Requirement "Netlist file structure") SHALL become the final netlist lines by the rules below. They state the result; where one rule works on the result of another, the rule says so. None of these rules depends on the `xschem-compat` setting except "Trailing m=1".

**User-code blocks.** The user-code blocks are the header code (item 2 of Requirement "Netlist file structure") and the architecture code blocks, each of which runs from a line starting with `**** begin user architecture code` to the next line starting with `**** end user architecture code`; both of these lines SHALL be written. Lines inside a block SHALL be written unchanged, except for "Trailing m=1" and "Parametric vector names".

**Continuation lines.** Outside user-code blocks, a line starting with `+` continues the line before it: they SHALL form one line, the earlier line followed by one space and the rest of the continuation line after the `+`. The other rules apply to the joined line.

**Trailing m=1.** A trailing match of the regular expression ` [mM] *= *1 *$` SHALL be removed from the joined line; for a vector element it is therefore missing from every line written for it. With `xschem-compat` ON it is removed from every line, including user code, comments, `.param` and `.control` text, as xschem does. With `xschem-compat` OFF it is removed only from device lines: lines outside user-code blocks whose first field does not start with `*` or `.` (Q-SCHEM-22, `Resolution: compat`).

**Comment lines.** A line whose first field starts with `*` SHALL be written unchanged.

**Notations in element text.** Element text MAY contain these notations, from the tokens of Requirement "Format string substitution" or written literally in a format string. They are part of the format-string syntax: a user MAY write any of them in a `format` or `lvs_format` attribute, and they have the meaning below whether a token produced them or the user wrote them. None of them appears in the netlist:
- **width marker**: a field matching `^(.*=)?\?-?[0-9]+$`, that is an optional `<key>=` and `?<w>`, applies to the field after it (see "Vector elements")
- `##` at the start of a field SHALL be removed
- `#<p>#<rest>` SHALL become `<p><rest>` with `<p>` also inserted after every comma of `<rest>`, and the field SHALL be treated as preceded by the width marker `?1` when the field before it is not a width marker
- `#<p>#?<w>` is a width marker `?<w>` whose prefix `<p>` SHALL be put before each comma-separated item of the field after it
- every `PARAM:` SHALL be removed

**Vector elements.** Every other line (outside user-code blocks, not a comment line, not a subcircuit line and not a save line with a width marker, as below; `.param` and other dot lines included) is an element line. Its first field is the list of element names, separated by commas: for example `R1[1],R1[0]` for a vector instance. For the devices `ymemristor`, `ylin`, `ydelay`, `ytransline`, `ypgbr`, `ypowergridbranch` and `yacc`, and for `.model` lines, the list is the first field repeated once per comma-separated item of the third field. One line SHALL be written per name, in list order. The line for the i-th name (i from 1) SHALL be the element's spice prefix, the name, and then the other fields in order, each followed by one space:
- a width marker itself is not written
- the field after a width marker with width w (w = −1 means the item count of that field) and items a₁..a_k SHALL contribute `<key>=` when the marker has one, then the items a[((l + w·(i−1)) mod k) + 1] for l = 0..w−1
- any other field SHALL be written as it is

An element line whose name list is empty SHALL be written as an empty line.

**Parametric vector names.** The `name=value` pairs of every line of the netlist text that starts with `.param` (matched case-insensitively, spaces around `=` ignored, lines in user-code blocks included, wherever the line is in the netlist relative to the element, and as written before "Trailing m=1" removes anything; a later pair for a name replaces an earlier one) SHALL be used to expand the first field of every line, user-code lines included, when that field matches `^[a-zA-Z][^\[\]:]+\[[a-zA-Z_][a-zA-Z0-9_]*:[a-zA-Z_][a-zA-Z0-9_]*\]$`: `<base>[<a>:<b>]` becomes the bus expansion (Requirement "Bus and vector notation") of `<base>[<value of a>:<value of b>]`.

**Renames.** On an element line starting with `g` or `G`, the first ` value=` (case-insensitive) SHALL become ` cur=`; with `e` or `E` it becomes ` vol=`; with `r` or `R`, ` r=`; with `c` or `C`, ` c=`. On a line starting with `D`, the first `PERI` followed by optional spaces and `=` SHALL become `PJ=`.

**Save lines.** A line whose first field is `.save` or `.print` (case-insensitive) and that contains a width marker (a field that is one, or a field `<t>(?<w>` whose width marker follows its `(`, as `spice_probe_vdiff.sym` writes: golden case SCHEM-NL-05, clarified 2026-11-24) SHALL be written in lower case as one line `<.save or .print> <t>(<name>)` for each comma-separated name of each of its other fields `<t>(<names>)`, after the width markers and the spaces inside the parentheses (after a `(`, before a `)`) are removed; a space after a `)` still separates two fields.

**Subcircuit lines.** A line whose first field contains `.subckt` (case-insensitive) and does not start with `*` SHALL have its first ` m=<digits>` replaced by one space and every comma replaced by a space, and SHALL otherwise keep its spacing. A commented top line such as `**.subckt T A[1],A[0]` is a comment line and keeps its commas (golden case SCHEM-NL-17).

Case-insensitive matches in "Parametric vector names" and "Renames" follow gawk on every platform, whatever the `xschem-compat` setting (Q-SCHEM-18, `Resolution: replicate`).

Compatibility: exact

(xschem ref: spice.awk:25-360)

#### Scenario: Vector instance on a bus
- **GIVEN** the element text `R1[1],R1[0] ?1 A[1],A[0] ?1 B 1k`
- **WHEN** it is written to the netlist
- **THEN** the lines SHALL be `R1[1] A[1] B 1k` and `R1[0] A[0] B 1k`

#### Scenario: sky130 transistor with prefix
- **GIVEN** an element with spice prefix `X` whose text is `M1 ?1 D ?1 G ?1 S ?1 B sky130_fd_pr__nfet_01v8 L=0.15 W=1` LF `+ nf=1 ad=0.29`
- **WHEN** it is written to the netlist
- **THEN** the single output line SHALL be `XM1 D G S B sky130_fd_pr__nfet_01v8 L=0.15 W=1 nf=1 ad=0.29`

#### Scenario: Current save
- **GIVEN** a `vsource.sym` instance `V1` with `value=3 savecurrent=true` between nets `A` and `0`
- **WHEN** it is netlisted
- **THEN** the output SHALL be the line `V1 A 0 3` followed by `.save i(v1)`

#### Scenario: Trailing m=1 in user code
- **GIVEN** a `code_shown.sym` instance whose value is the line `.param m=1`, and a resistor line `R1 A B 1k m=1`
- **WHEN** the schematic is netlisted with `xschem-compat` ON, then OFF
- **THEN** with ON the lines SHALL be `.param` and `R1 A B 1k`; with OFF they SHALL be `.param m=1` and `R1 A B 1k`

#### Scenario: Trailing m=1 in a testbench, xschem-compat ON
- **GIVEN** golden case SCHEM-NL-01: top schematic `SC_Test` with no ports, instances `vsource.sym` `V1` (`value=5`, nets `VIN`, `0`), `res.sym` `R1` (`value=1k m=1`, nets `VIN`, `VOUT`), `capa.sym` `C1` (`m=1 value=1n`, nets `VOUT`, `0`) and one `code_shown.sym` instance whose value is the three lines `.param m=1`, `.tran 1n 1u`, `.save all`
- **WHEN** it is netlisted with `xschem-compat` ON
- **THEN** the netlist SHALL be, line by line: `** sch_path: <path>`, `**.subckt SC_Test`, `V1 VIN 0 5`, `R1 VIN VOUT 1k`, `C1 VOUT 0 1n`, `**** begin user architecture code`, an empty line, `.param`, `.tran 1n 1u`, `.save all`, an empty line, `**** end user architecture code`, `**.ends`, `.end`

### Requirement: Line breaking
Finally, long lines SHALL be wrapped with SPICE continuation lines. This is xschem's layout of long lines, kept whatever the `xschem-compat` setting because it changes only where lines wrap (Q-SCHEM-20, `Resolution: replicate`):
- Trailing spaces and tabs SHALL be removed from every line; an empty line stays empty.
- These lines SHALL NOT be wrapped: the lines of architecture code blocks (Requirement "Element line post-processing", "User-code blocks"), including their begin and end lines, lines whose first field in lower case contains a match of `\.inc(lude)?|\.lib|\.title|\.save|\.write` (unanchored), and lines matching `^\*\* ..._path:`. Header code lines are wrapped like other lines.
- A wrap SHALL insert LF and `+` (or `*+` when the line starts with `*`) directly before the whitespace that precedes a field, so the continuation line starts with `+` followed by that whitespace.
- **Field rule.** A line with no `{`, `}` or `'`, read while no quote is open (below), SHALL be wrapped before a field when the length of the current segment, counted up to the end of that field, exceeds 130 characters. A segment's length counts each field with the whitespace before it, except the field that starts a continuation segment (the field written after the `+`), which counts as zero.
- **Quote rule.** A line that contains `{`, `}` or `'`, or that starts while a quote is open, SHALL instead be read character by character. Each `{`, `}` and `'` opens a quote when none is open and closes it otherwise (there is no nesting, and the three characters are interchangeable); a quote left open at the end of a line stays open for the following lines. Lines that are not wrapped are read for quotes too; empty lines are not. The line SHALL be wrapped before a space or tab when no quote is open and the segment, counted in characters from its start to that space or tab, is 130 characters or longer (golden case SCHEM-NL-05, corrected 2026-11-24 from the capture: a blank right after 130 characters wraps); the next segment is counted from the character after that space or tab, so the blank written after the `+` does not count.

Compatibility: exact

(xschem ref: break.awk:24-78)

#### Scenario: Long device line
- **GIVEN** a device line of 200 characters made of fields of 10 characters separated by single spaces, with no braces or quotes
- **WHEN** it is broken
- **THEN** the first output line SHALL end after the eleventh field (the segment reaches 131 characters at the twelfth field), the next line SHALL start with `+ ` followed by the twelfth field, and the following break SHALL come before the twenty-fourth field

### Requirement: Netlist file structure
The hierarchical SPICE netlist of top schematic `T` (path `/abs/T.sch`) SHALL consist of, in order:
1. `** sch_path: /abs/T.sch`, followed, only when `xschem-compat` is OFF, by the line `** xschem-compat=off` (capability `hefesto-magic-compat-mode`, Requirement "xschem-compat setting for xschem quirks")
2. Header code: the `netlist_commands` instances whose `place` (symbol property first, then instance property) is `header`, in instance order, each element text written as is. This header code is a user-code block (Requirement "Element line post-processing").
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
1. For each non-ignored port instance (`ipin`, `opin`, `iopin`) in instance order: its element text (for the system symbols, `*.ipin <lab>`, `*.opin <lab>`, `*.iopin <lab>`). In LVS netlist mode or with `top_is_subckt`, instead one line `*.PININFO ` followed by `<lab>:<d> ` per port, with d = `I`, `O` or `B` for `ipin`, `opin`, `iopin`; with no port, that line is an empty line (golden case SCHEM-NL-10, clarified 2026-11-24 from the capture).
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
   - LFs of the template: with `xschem-compat` ON they are kept, as xschem does, which puts parameters on lines without `+`; with `xschem-compat` OFF an LF followed by optional blanks (spaces and tabs) and `+` SHALL be kept, so a SPICE continuation line stays one, and every other LF SHALL be written as one space, so the header is valid SPICE. Both modes SHALL report a warning naming the symbol whose template contains an LF (Q-SCHEM-21, `Resolution: compat`, amended 2026-11-18). The text after a kept LF is a line of its own and goes through Requirement "Element line post-processing" like any other line (scenarios "Multi-line template, xschem-compat ON" and "Template continuation lines, xschem-compat OFF")
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
- **AND** for golden case SCHEM-NL-16 (instances `x1` of `libA/inv.sym` and `x2` of `libB/inv.sym`, both on nets `A` and `B`; pins `A`, `Y`; each `inv.sch` has `iopin` ports `A`, `Y` and one resistor, `1k` in `libA`, `2k` in `libB`) the netlist with ON SHALL be, line by line: `** sch_path: <path>/nl16.sch`, `**.subckt nl16`, `x1 A B inv`, `x2 A B inv`, `**.ends`, an empty line, `* expanding   symbol:  libA/inv.sym # of pins=2`, `** sym_path: <path>/libA/inv.sym`, `** sch_path: <path>/libA/inv.sch`, `.subckt inv A Y`, `*.iopin A`, `*.iopin Y`, `R1 A Y 1k`, `.ends`, an empty line, `.end`

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

#### Scenario: Template continuation lines, xschem-compat OFF
- **GIVEN** a subcircuit symbol `amp.sym` with pins `IN`, `OUT`, format `@name @pinlist @symname` and template `name=x1 W=1` LF `L=2` LF `+ M=4`, and `xschem-compat` OFF
- **WHEN** its definition is written
- **THEN** the header text handed to Requirement "Element line post-processing" SHALL be `.subckt amp IN OUT  W=1 L=2` LF `+ M=4`: the LF before `L=2` becomes a space and the LF before `+` is kept, so no bare `+` word is put inside the header line (post-processing then treats `+ M=4` as a continuation line); a warning SHALL name the symbol
- **AND** with the last template line written `  + M=4` (blanks before the `+`) the LF before it SHALL also be kept
- **AND** with `xschem-compat` ON both LFs SHALL be kept, as in scenario "Multi-line template, xschem-compat ON"

#### Scenario: Multi-line template, xschem-compat ON
- **GIVEN** sky130 `sky130_tests/passgate.sym` (pins `Z`, `A`, `GP`, `GN`; format `@name @pinlist @VCCBPIN @VSSBPIN @symname W_N=@W_N L_N=@L_N W_P=@W_P L_P=@L_P m=@m`; template `name=x1 W_N=1 L_N=0.2 W_P=1 L_P=0.2 VCCBPIN=VCC VSSBPIN=VSS ` LF `modeln=nfet_01v8 modelp=pfet_01v8 m=1`; `extra="VCCBPIN VSSBPIN modeln modelp"`), used by `gain_stage.sch`, and `xschem-compat` ON
- **WHEN** `gain_stage.sch` is netlisted
- **THEN** the definition SHALL start with the lines `* expanding   symbol:  sky130_tests/passgate.sym # of pins=4`, `** sym_path: <path>/sky130_tests/passgate.sym`, `** sch_path: <path>/sky130_tests/passgate.sch`, `.subckt passgate Z A GP GN VCCBPIN VSSBPIN  W_N=1 L_N=0.2 W_P=1 L_P=0.2` (two spaces before `W_N`) and an empty line (the second template line after the `extra` keys are removed, the trailing ` m=1` removed and the trailing spaces removed), followed by the body (golden case SCHEM-NL-03), and a warning SHALL name the symbol

### Requirement: Schematic resolution
The schematic of a subcircuit instance or symbol SHALL be:
1. the instance's `schematic` attribute, with `@` tokens substituted from the instance, if non-empty; else
2. the symbol's `schematic` attribute, if non-empty;
3. in both cases with `@symname` replaced by the symbol cell name, Tcl-evaluated if in `tcleval(...)` form, and resolved like a symbol reference (capability `schematic-symbols-and-libraries`) unless it is a generator;
4. otherwise the symbol's reference with its extension replaced by `.sch`, in the same folder as the resolved symbol file.

An instance whose `schematic` attribute differs from its symbol's SHALL produce a separate subcircuit definition, one per distinct `schematic` value, named after that schematic's cell name `<v>` (the value without folder and extension) (Q-SCHEM-28, `Resolution: replicate`). This variant is an entry of the list of loaded symbols of Requirement "Subcircuit definitions", after the symbols used at top level; it is written as any other entry, with these texts (golden cases SCHEM-NL-15 and SCHEM-NL-03):
- the instance lines of the instances with that value write `<v>` in place of the cell name (`@symname`)
- the definition starts with `* expanding   symbol:  <v>.sym # of pins=<n>`, then `** sym_path: <absolute path of the original symbol>`, then `** sch_path: <absolute path of the variant's schematic>`, then the header line `.subckt <v> ...` built from the original symbol's format and template
- when the value names no existing file (SCHEM-NL-03: `schematic=passgate_1` with no `passgate_1` file), the variant's schematic is the one of rule 4, the original symbol's own schematic; the variant is still a separate definition named `<v>`
- the body of the variant takes the instance's values for hierarchical parameters (Requirement "Hierarchical parameters"), so it can differ from the original symbol's definition (SCHEM-NL-03: `modelp=pfet_01v8_lvt` on the instance gives `sky130_fd_pr__pfet_01v8_lvt` in `.subckt passgate_1`, while `.subckt passgate` takes `pfet_01v8` from the template)
- the original symbol's own definition is written too, also when every instance of it in the netlist uses a variant (SCHEM-NL-03)

A missing schematic SHALL produce a definition with its header lines and an empty body, and SHALL report a warning, and the run SHALL NOT fail (Q-SCHEM-27, `Resolution: replicate`; golden case SCHEM-NL-14): `* expanding   symbol:  <reference> # of pins=<n>`, `** sym_path: <absolute symbol path>`, `** sch_path: <absolute path of the missing schematic>`, the header line, then directly `.ends` and an empty line. The warning wording is informative (xschem: `load_schematic(): unable to open file: <path>, ffname=<path>`). Both rules apply whatever the `xschem-compat` setting.

Compatibility: exact

(xschem ref: actions.c:2474-2580)

#### Scenario: Instance-based implementation
- **GIVEN** golden case SCHEM-NL-15: top schematic `nl15` with instances `x1` of `amp.sym` and `x2` of `amp.sym` with `schematic=amp_fast.sch`, both on nets `A` and `B`; `amp.sym` has pins `IN`, `OUT` and format `@name @pinlist @symname`; `amp.sch` and `amp_fast.sch` each have `iopin` ports `IN`, `OUT` and one resistor (`1k` and `100`)
- **WHEN** the hierarchical netlist is generated
- **THEN** it SHALL be, line by line: `** sch_path: <path>/nl15.sch`, `**.subckt nl15`, `x1 A B amp`, `x2 A B amp_fast`, `**.ends`, an empty line, `* expanding   symbol:  amp.sym # of pins=2`, `** sym_path: <path>/amp.sym`, `** sch_path: <path>/amp.sch`, `.subckt amp IN OUT`, `*.iopin IN`, `*.iopin OUT`, `R1 IN OUT 1k`, `.ends`, two empty lines, `* expanding   symbol:  amp_fast.sym # of pins=2`, `** sym_path: <path>/amp.sym`, `** sch_path: <path>/amp_fast.sch`, `.subckt amp_fast IN OUT`, `*.iopin IN`, `*.iopin OUT`, `R1 IN OUT 100`, `.ends`, an empty line, `.end`

#### Scenario: Missing subcircuit schematic
- **GIVEN** golden case SCHEM-NL-14: instance `x1` of subcircuit symbol `missing.sym` (pins `A`, `B`; format `@name @pinlist @symname`) on nets `N1`, `N2`, and no `missing.sch`
- **WHEN** the hierarchical netlist is generated
- **THEN** the instance line SHALL be `x1 N1 N2 missing`, the definition SHALL be the lines `* expanding   symbol:  missing.sym # of pins=2`, `** sym_path: <path>/missing.sym`, `** sch_path: <path>/missing.sch`, `.subckt missing A B`, `.ends` and an empty line, a warning SHALL name `missing.sch`, and the run SHALL succeed

#### Scenario: spice_sym_def, spice_stop and default_schematic=ignore
- **GIVEN** golden case SCHEM-NL-14: subcircuit symbols `withdef.sym` (`spice_sym_def=".include x.spice"`), `stopped.sym` (`spice_stop=true`, with a schematic) and `ignored.sym` (`default_schematic=ignore`, with a schematic), each used by one instance after `x1`
- **WHEN** the hierarchical netlist is generated
- **THEN** after the `missing` definition the netlist SHALL continue with an empty line, `* expanding   symbol:  withdef.sym # of pins=2`, `** sym_path: <path>/withdef.sym`, `.include x.spice`, an empty line, `* expanding   symbol:  stopped.sym # of pins=2`, `** sym_path: <path>/stopped.sym`, `** sch_path: <path>/stopped.sch`, `.subckt stopped A B`, `.ends`, an empty line, `.end`; `ignored.sym` SHALL have no definition while its instance line `x4 N1 N2 ignored` is written

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
In `flat` mode, Hefesto SHALL produce a netlist with no subcircuit hierarchy for the subcircuits defined in the schematic: every instance of a defined subcircuit is replaced by its body; port nets map to the parent nets; global nets and `0` are not renamed; device lines of undefined subcircuits (for example PDK model subcircuits) stay `X` lines. Instance names SHALL be suffixed `_<path>` and local net names prefixed `<path>_`, where the path joins the instance names from the top with `_` (SCHEM-NL-21: `XM1` inside instance `x6` becomes `XM1_X6`). A parameter of a substituted device line whose whole value is the name of a parameter of the enclosing subcircuit SHALL take the value that the instance passes, else the subcircuit header's default; a parameter name inside a longer value, such as a quoted expression, SHALL be kept as written (SCHEM-NL-21: inside `x6` with `W_N=0.42 L_N=0.35`, `L=L_N` becomes `L=0.35` and `W=W_N` becomes `W=0.42`, while `ad='int((1 + 1)/2) * W_N / 1 * 0.29'` keeps `W_N`).

The flat netlist SHALL be electrically equivalent to the hierarchical one and to xschem's flat netlist: the same devices, the same connections, and the same model names and parameter values. It is not byte-compared: the golden comparison SHALL parse both netlists into a canonical form (devices with their resolved nodes, models and parameter values, in a canonical order, with names compared case-insensitively as SPICE reads them) and compare those. Any other property of xschem's flat text is informative and is not reproduced (Q-SCHEM-15, `Resolution: replicate`, behavioural), in particular: its letter case (xschem writes the flat text in upper case except `.end`), the whitespace inside quoted expressions (xschem removes it: `'int((1 + 1)/2) * WN / 1 * 0.29'` becomes `'INT((1+1)/2)*WN/1*0.29'`, which SPICE reads the same), and its `*--------BEGIN_<path>->...`/`*--------END___<path>->...` comment lines. The canonical form SHALL therefore compare quoted expressions without their whitespace.

**Limits** (approved by the user on 2026-11-22, Q-SCHEM-42, decision M8-D1; both modes of `xschem-compat`; xschem bounds neither):
- **Depth.** Substitution SHALL descend at most 64 levels (`kMaxFlatDepth`), the top schematic's instances being level 1. An instance of a defined subcircuit that would open level 65 SHALL NOT be substituted: it SHALL be written as its `X` line, and the warning `flat netlist stopped: depth limit of 64 levels` (`SCHEM-NET-017`) SHALL be reported once per run. A subcircuit body that places itself therefore stops after 64 levels.
- **Size.** The flat output is counted line by line before line breaking (clarified by the user on 2026-11-23, decision M8-D8): each kept line counts exactly the bytes of its text, plus 1 for its line feed, plus `kFlatLineOverhead` = 40 bytes for its in-memory record; a substituted instance, which is replaced by its body and not kept, counts its instance line the same way (its text as written in the enclosing body, plus 1, plus 40). A line of `n` bytes of text therefore counts `n + 41`, and the limit bounds memory as well as text. Lines written after the stop are not counted. When the next line would pass 268,435,456 bytes (256 MiB), substitution SHALL stop: no further instance SHALL be substituted, the instances not yet substituted SHALL be written as their `X` lines, every other remaining line (the rest of the bodies already being substituted, `**.ends`, `.GLOBAL`, `.end`) SHALL still be written, so the output stays a complete netlist, and the warning `flat netlist stopped: output passed 268435456 bytes` (`SCHEM-NET-017`) SHALL be reported once per run.
- In both cases the netlist run SHALL NOT fail; the warning is the only sign that the flat netlist is incomplete.

Compatibility: behavioral

(xschem ref: flatten.awk:36-312, xschem.tcl:1006-1013)

#### Scenario: Flattened inverter chain
- **GIVEN** a top schematic with two instances `x1`, `x2` of subcircuit `inv`
- **WHEN** a flat netlist is generated
- **THEN** it SHALL contain no `.subckt` line for `inv`, and the transistors of both instances SHALL appear with distinct names

#### Scenario: Flat netlist compared behaviourally
- **GIVEN** golden case SCHEM-NL-21 (sky130 `top.sch` and `gain_stage.sch` netlisted flat by xschem and by Hefesto; xschem's flat `top.sch` has only comment lines and `.end`, no device line, while flat `gain_stage.sch` has 16 device lines, two of them from the `passgate_1` variant of instance `x6`)
- **WHEN** the outputs are compared
- **THEN** the comparison SHALL pass when the canonical parsed forms are equal, even if the texts differ in letter case, whitespace inside quoted expressions or line order

#### Scenario: Flat netlist deeper than 64 levels
- **GIVEN** a subcircuit `loop` whose body holds one resistor and one instance of `loop`, and a top schematic that places `loop` once, with `xschem-compat` ON or OFF
- **WHEN** a flat netlist is generated
- **THEN** it SHALL hold 64 resistor lines (levels 1 to 64) and one `X` line for the instance of `loop` at level 65, the warning `flat netlist stopped: depth limit of 64 levels` (`SCHEM-NET-017`) SHALL be reported once, and the run SHALL complete without failing (Q-SCHEM-42)

#### Scenario: Flat netlist byte count
- **GIVEN** a flat netlist run whose byte limit is lowered to 4,096 bytes (a test of the count, not a user setting), a top level that places 1,000 instances of a subcircuit `c` whose body holds 100 comment lines `*`, with `xschem-compat` ON or OFF
- **WHEN** a flat netlist is generated
- **THEN** each kept `*` line SHALL count 1 + 1 + 40 = 42 bytes, so at most 4,096 / 42 such lines, plus the rest of the one body open at the stop, SHALL be kept; the instances after the stop SHALL be written as their `X` lines, the netlist SHALL end with `.end`, and `SCHEM-NET-017` SHALL be reported once (Q-SCHEM-42, M8-D8)

#### Scenario: Flat netlist larger than 256 MiB
- **GIVEN** a hierarchy whose fully flattened netlist would be larger than 268,435,456 bytes (for example 24 levels that each place the next subcircuit twice, the last holding a resistor: 2^24 resistor lines), with `xschem-compat` ON or OFF
- **WHEN** a flat netlist is generated
- **THEN** the output SHALL hold no more than 268,435,456 bytes plus the remaining lines written after the stop (the `X` lines of the instances not yet substituted and the other lines of the bodies already open and of the top level), SHALL end with `.end`, the warning `flat netlist stopped: output passed 268435456 bytes` (`SCHEM-NET-017`) SHALL be reported once, the run SHALL complete without failing, and work and memory SHALL stay bounded by the limit (Q-SCHEM-42)

### Requirement: Errors and warnings
Netlisting SHALL report, without stopping:
- shorts between differently named nets (Requirement "Net naming order")
- instances with no `name` whose template has no `name` either, except for types `package`, `port_attributes`, `architecture`, `arch_declarations`, `attributes`, `netlist_options` and `use`: `Warning: instance: <name> (<symbol>): no name attribute set`
- symbols with no `type`: `Warning: Symbol: <symbol>: no type attribute set`
- unconnected pins (Requirement "Unnamed nets")
- instances with all pins unconnected, overlapping identical instances, and pin mismatches between a symbol and its schematic
- bus expansion syntax errors
- subcircuit schematics that do not exist (Requirement "Schematic resolution"); this is a warning
- the warnings that the rules above give in both modes of `xschem-compat`: duplicate `sim_pinnumber` values (Q-SCHEM-5), two subcircuits with one cell name (Q-SCHEM-14), template keys found in `extra` only as substrings (Q-SCHEM-19), templates with LFs (Q-SCHEM-21), numbers whose reading depends on `xschem-compat` (Q-SCHEM-12), and undefined tokens with a defined identifier prefix (Q-SCHEM-26)

A run that found a short, a pin mismatch or another error SHALL return a failure status (xschem exit code 10), and the offending objects SHALL be highlighted in the editor (capability `schematic-editing`). Warnings alone SHALL NOT fail the run. The message wording is informative.

Compatibility: behavioral

(xschem ref: netlist.c:1430-1460, netlist.c:1791-1990, spice_netlist.c:636-641)

#### Scenario: Netlist with a short
- **GIVEN** a schematic where labels `A` and `B` are on one wire
- **WHEN** it is netlisted from a script
- **THEN** the netlist SHALL be produced and the command SHALL return a failure status

### Requirement: In-process netlisting
Hefesto SHALL generate the netlist in memory, on every platform, without temporary files or external programs. It SHALL write the netlist to a file only when asked: by default `<netlist folder>/<top cell>.spice`, where the netlist folder is a Hefesto setting (default: a `simulation` folder beside the schematic, inside the document's granted folder on iPadOS). The output SHALL be identical on every platform for the same inputs and the same resolved absolute paths. Netlisting SHALL NOT change the document's undo history. A run SHALL be interruptible: the caller's interrupt check SHALL be polled at least once per schematic level placed, and an interrupted run SHALL return no netlist text and no computed labels and SHALL fail with error SCHEM-NET-018 (in both xschem-compat modes).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: iPad netlist in memory
- **GIVEN** a testbench open on iPadOS
- **WHEN** a simulation starts from it
- **THEN** the netlist SHALL be passed to the simulation engine in memory and no `.spice` file SHALL be created

#### Scenario: Interrupted netlist run
- **GIVEN** a hierarchical testbench whose run is interrupted after its top level is placed
- **WHEN** the netlister polls the interrupt check before placing a subcircuit's schematic
- **THEN** the run SHALL stop, return no netlist text and no computed labels, and fail with SCHEM-NET-018
