# Schematic File Format

## Purpose
Defines the on-disk format of schematic (`.sch`) and symbol (`.sym`) files used by Hefesto's built-in schematic editor (decision P-12). Hefesto SHALL read and write these files so that xschem symbol libraries and schematics, including the sky130, gf180mcu and IHP sg13g2 PDK libraries, work unchanged. This covers:
- the record grammar of both file kinds (they share one grammar)
- the brace-string encoding and its escapes
- numeric field parsing and printing
- the version record, header text and file-format versions 1.0 to 1.3
- global property records (`K`, `G`, `V`, `S`, `F`, `E`)
- graphic records (`L`, `B`, `A`, `P`, `T`), wires (`N`), component instances (`C`) and embedded symbols (`[` ... `]`)
- the property-string (attribute) grammar shared by every record
- orientation transforms
- canonical writing and round-trip guarantees

**Compatibility adaptation.** This capability is about xschem, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **xschem** (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`, version `3.4.8RC`, file format `1.3`):
- `exact`: byte-identical to what xschem writes, and accepts everything xschem accepts
- `behavioral`: same data semantics; the interaction surface may differ
- `informative`: xschem detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-12)`

Traces use `(xschem ref: path/file.c:line)`, relative to the xschem `src/` directory. They exist for traceability only. xschem is GPL: implementers SHALL work from this spec and SHALL NOT consult the xschem sources (capability `hefesto-licensing`).

Other capabilities own related parts:
- Symbol semantics, library search and PDK conventions: capability `schematic-symbols-and-libraries`.
- Connectivity and SPICE output: capability `schematic-netlisting`.
- Editing operations: capability `schematic-editing`.

Open questions are in `docs/decisions/open-questions/SCHEM.md` (Q-SCHEM-n). Golden cases are in `tests/golden/SCHEM/CASES.md`.

## Requirements

### Requirement: File kinds and character handling
Hefesto SHALL treat `.sch` and `.sym` files as one grammar. A schematic normally holds wires and component instances; a symbol normally holds only graphics, pins and texts, but either kind MAY hold any record. Files SHALL be read as bytes:
- a carriage return (CR, 0x0D) SHALL be discarded wherever it occurs inside a brace string or while skipping a record, so CR-LF files read like LF files
- all other bytes, including non-ASCII UTF-8 sequences, SHALL be kept unchanged inside brace strings
- files SHALL be written with LF line ends only

Compatibility: exact

(xschem ref: save.c:2826, save.c:2861, save.c:3736)

#### Scenario: CR-LF file
- **GIVEN** a `.sym` file whose lines end in CR-LF, containing `K {type=nmos\r\nformat="@name"}\r\n`
- **WHEN** it is read and written back
- **THEN** the symbol property SHALL be `type=nmos` LF `format="@name"`, and the written file SHALL contain no CR byte

### Requirement: Record grammar
A file SHALL be a sequence of records. Before each record, any whitespace (space, tab, CR, LF) SHALL be skipped. The first non-whitespace character is the record tag. After a record is parsed, the remaining characters up to (not including) the next LF SHALL be discarded.

```ebnf
file        = { ws , record } , [ ws ] ;
record      = version | global | line | box | arc | poly | text | wire
            | instance , [ ws , embedded ] | comment | unknown ;
version     = "v" , ws1 , bstring ;
global      = ( "K" | "G" | "V" | "S" | "F" | "E" ) , ws1 , bstring ;
line        = "L" , ws1 , layer , ws1 , num , ws1 , num , ws1 , num , ws1 , num , ws1 , bstring ;
box         = "B" , ws1 , layer , ws1 , num , ws1 , num , ws1 , num , ws1 , num , ws1 , bstring ;
arc         = "A" , ws1 , layer , ws1 , num , ws1 , num , ws1 , num , ws1 , num , ws1 , num , ws1 , bstring ;
                (* centre x, centre y, radius, start angle, sweep angle *)
poly        = "P" , ws1 , layer , ws1 , count , { ws1 , num , ws1 , num } , ws1 , bstring ;
                (* exactly count coordinate pairs *)
text        = "T" , ws1 , bstring , ws1 , num , ws1 , num , ws1 , rot , ws1 , flip ,
              ws1 , num , ws1 , num , ws1 , bstring ;
                (* text, x, y, rotation, flip, x scale, y scale, attributes *)
wire        = "N" , ws1 , num , ws1 , num , ws1 , num , ws1 , num , ws1 , bstring ;
instance    = "C" , ws1 , bstring , ws1 , num , ws1 , num , ws1 , rot , ws1 , flip , ws1 , bstring ;
                (* symbol reference, x, y, rotation, flip, attributes *)
embedded    = "[" , rest-of-line , LF , { ws , record } , ws , "]" ;
comment     = "#" , rest-of-line ;
unknown     = other-tag , { any-char-or-bstring } ;   (* skipped up to the next LF outside braces *)
layer       = integer ;                 (* 0 <= layer < 22 *)
count       = integer ;
rot         = integer ;                 (* 0..3 *)
flip        = integer ;                 (* 0..1 *)
num         = decimal-floating-point ;
ws          = { " " | TAB | CR | LF } ;
ws1         = ws ;                       (* at least the separator the writer emits *)
```

Rules:
- Records MAY appear in any order and any record kind MAY repeat. For a repeated `v`, `K`, `G`, `V`, `S`, `F` or `E` record the last one read SHALL win.
- A `bstring` MAY span lines (Requirement "Brace strings"). The "discard to end of line" step applies after the closing brace of the last field.
- An unknown tag SHALL be skipped up to the next LF that is not inside a brace string; braces in it SHALL be read as brace strings so that an LF inside them does not end the record.
- A `#` record SHALL be skipped to the end of its line.

Compatibility: exact

(xschem ref: save.c:3585, save.c:2826)

#### Scenario: Unknown record skipped
- **GIVEN** a schematic containing the line `Z 1 2 {a` LF `b} 3` followed by `N 0 0 10 0 {}`
- **WHEN** the file is read
- **THEN** the `Z` record, including its two-line brace string, SHALL be skipped, and exactly one wire from (0,0) to (10,0) SHALL be loaded

### Requirement: Brace strings
Every text field and property field SHALL be a brace string. Reading SHALL work as follows:
1. Characters up to and including the first `{` are skipped.
2. After that, a backslash that is not itself escaped starts an escape: the backslash is dropped and the next character is taken literally.
3. An unescaped `}` ends the string.
4. CR characters are dropped. All other characters, including LF, are kept.
5. An empty result SHALL be stored as "no value" (absent property), which is equivalent to the empty string for every lookup.
6. End of file inside a brace string SHALL make that field absent and SHALL report `EOF reached, malformed {...} string input, missing close brace`.

Writing SHALL emit `{`, then each character of the value with a backslash inserted before every `\`, `{` and `}`, then `}`. An absent or empty value SHALL be written as `{}`. No other character is escaped.

Compatibility: exact

(xschem ref: save.c:3736, save.c:2978, editprop.c:236)

#### Scenario: Escaped braces and backslashes round-trip
- **GIVEN** a symbol property held in memory as the characters `drc="f {@name\}"`
- **WHEN** the value is written
- **THEN** the output SHALL be `{drc="f \{@name\\\}"}`, and reading that output SHALL give back the in-memory value exactly

#### Scenario: Double-escaped quote in a PDK template
- **GIVEN** the file text `ad=\\"expr('0.29')\\"` inside a `K` brace string
- **WHEN** it is read
- **THEN** the in-memory property text SHALL be `ad=\"expr('0.29')\"` (one backslash before each quote)

### Requirement: Numeric fields
Coordinates, radii, angles and text scales SHALL be IEEE-754 binary64 values. Reading SHALL accept any decimal floating-point syntax accepted by C `strtod` in the "C" locale (optional sign, digits, optional fraction, optional exponent; `inf` and `nan` are accepted but have no defined drawing). Layer, point count, rotation and flip SHALL be read as decimal integers.

Writing SHALL format every floating-point field with the C format `%.16g` and every integer field with `%d`. Consequences that SHALL hold:
- `10.0` is written `10`
- `0.1` is written `0.1`
- `1e-5` is written `1e-05`
- negative zero is written `-0`
- `0.30000000000000004` is written `0.3`, so a value whose shortest round-trip form needs 17 digits is not preserved bit-exactly

Compatibility: exact

(xschem ref: save.c:3121, save.c:3136, save.c:3309)

#### Scenario: Re-formatting a hand-edited coordinate
- **GIVEN** the record `N 10.50 -20.0 30 -20 {}`
- **WHEN** the file is read and written
- **THEN** the written record SHALL be `N 10.5 -20 30 -20 {}`

### Requirement: Version record and header text
The `v` record SHALL hold a property string. On reading:
- `file_version` SHALL be the value of the `file_version` key (property grammar, Requirement "Property strings"). If the file has no `v` record or the key is absent, the file version SHALL be `1.0`.
- The header text SHALL be the `v` string with the keys `xschem`, `version` and `file_version` removed (the key `xschem` only if the substring `xschem` starts within the first 50 characters), then leading spaces and tabs removed, then one leading LF removed.

On writing, the `v` record SHALL be the first line of the file:
- with no header text: `v {xschem version=<V> file_version=1.3}`
- with header text: `v {xschem version=<V> file_version=1.3` LF `<header text>}` (brace-escaped)

where `<V>` is the xschem compatibility version (Requirement "Writer version string"). Hefesto SHALL always write file format `1.3` regardless of the version read.

File version `1.0` SHALL change the meaning of symbol references: a reference is a symbol name without extension, and `.sym` SHALL be appended (replacing any extension) when the instance is loaded, so the reference is written back with `.sym`. From `1.1` on, the reference is used as written. When the file has no `v` record, the first record depends on the `xschem-compat` setting (capability `hefesto-magic-compat-mode`; Q-SCHEM-3, `Resolution: compat`):
- with `xschem-compat` ON, the file version is unset (not `1.0`) until the end of the first record, as in xschem, so an instance record that comes before any other record is not given the extension; later records see `1.0`
- with `xschem-compat` OFF, a file with no `v` record is version `1.0` from its first record, so every instance reference is given the extension

Compatibility: exact

(xschem ref: save.c:3592-3612, save.c:3223-3240, save.c:3335-3350, xschem.h:26-27)

#### Scenario: License header preserved
- **GIVEN** a symbol whose first record is `v {xschem version=3.4.7RC file_version=1.2` LF `* Copyright 2021 X` LF `}`
- **WHEN** it is read and saved
- **THEN** the header text SHALL be `* Copyright 2021 X` LF, and the first record written SHALL be `v {xschem version=3.4.8RC file_version=1.3` LF `* Copyright 2021 X` LF `}`

#### Scenario: Missing version means 1.0
- **GIVEN** a schematic with no `v` record whose first record is `K {}`, followed by the instance `C {res} 0 0 0 0 {name=R1}`
- **WHEN** it is read
- **THEN** the file version SHALL be `1.0` and the instance SHALL reference symbol `res.sym`

#### Scenario: Leading instance record, xschem-compat ON
- **GIVEN** `xschem-compat` ON and a schematic with no `v` record whose first record is `C {res} 0 0 0 0 {name=R1}`, followed by `C {capa} 0 0 0 0 {name=C1}`
- **WHEN** it is read and saved
- **THEN** `R1` SHALL reference `res` and `C1` SHALL reference `capa.sym`

#### Scenario: Leading instance record, xschem-compat OFF
- **GIVEN** `xschem-compat` OFF and the same schematic
- **WHEN** it is read and saved
- **THEN** `R1` SHALL reference `res.sym` and `C1` SHALL reference `capa.sym`

### Requirement: Writer version string
Hefesto SHALL write the version key as `xschem version=<V>`, where `<V>` is a single pinned xschem compatibility version, initially `3.4.8RC`. It SHALL NOT write a Hefesto version there, because xschem and other tools only check `file_version`. The pinned value SHALL change only together with a re-validation of this capability against a newer xschem, and SHALL NOT depend on the `xschem-compat` setting (Q-SCHEM-1, `Resolution: replicate`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Pinned version
- **WHEN** Hefesto saves any schematic
- **THEN** the first line SHALL begin with `v {xschem version=3.4.8RC file_version=1.3`

### Requirement: Global property records
The six global records SHALL each hold one property string for the file:

| Tag | Meaning |
|---|---|
| `K` | Symbol property: the netlisting rules (`type`, `format`, `template`, ...) of a `.sym`, or of a `.sch` used directly as a symbol. File version 1.2 and later. |
| `G` | VHDL global text of a schematic. In files before 1.2 it also served as the symbol property. |
| `V` | Verilog global text, copied verbatim into Verilog netlists |
| `S` | SPICE global text, copied verbatim into SPICE netlists (capability `schematic-netlisting`) |
| `F` | Spectre global text. File version 1.3 and later. |
| `E` | tEDAx global text |

On reading a file whose name ends in `.sym`, if no `K` record was read and the `G` value has a non-empty `type` key, the `G` value SHALL be used as the symbol property and the VHDL text SHALL be empty.

On writing, the global records SHALL appear in this order, each on its own line: `G`, `K`, `V`, `S`, `F`, `E`. As an exception, when the file name ends in `.sym`, the symbol property is absent and the VHDL text has a non-empty `type` key, Hefesto SHALL write `G {}` and then `K <VHDL text>`.

Compatibility: exact

(xschem ref: save.c:3613-3635, save.c:3696-3705, save.c:3242-3270)

#### Scenario: Pre-1.2 symbol upgraded
- **GIVEN** `old.sym` containing `v {xschem version=2.9.5 file_version=1.1}` and `G {type=subcircuit format="@name @pinlist @symname"}` and no `K` record
- **WHEN** it is read and saved
- **THEN** the written file SHALL contain `G {}` followed by `K {type=subcircuit format="@name @pinlist @symname"}`, then `V {}`, `S {}`, `F {}`, `E {}`

### Requirement: Layers
Graphic records SHALL carry a layer number `c` with `0 <= c < 22` (xschem's default layer count). A record with a missing layer field or `c` outside that range SHALL be skipped with a warning naming the record kind, and the rest of its line SHALL be discarded. Layers with fixed meaning:

| Layer | Meaning |
|---|---|
| 0 | background |
| 1 | wires (nets) |
| 2 | grid / selection; also graph boxes (capability `schematic-simulation-integration`) |
| 3 | text |
| 4 | symbol drawing |
| 5 | pins |
| 6–21 | general purpose |

A line drawn on layer 1 SHALL NOT carry connectivity; only `N` records do. A box on layer 5 in a symbol SHALL be a pin (capability `schematic-symbols-and-libraries`).

Compatibility: exact

(xschem ref: xschem.h:149-157, save.c:3487-3500, xschem.tcl:12127)

#### Scenario: Out-of-range layer
- **GIVEN** the record `L 30 0 0 10 10 {}`
- **WHEN** the file is read
- **THEN** the record SHALL be skipped with the warning `wrong or missing layer number for LINE object`, and the next record SHALL be read normally

### Requirement: Line, box, arc and polygon records
Hefesto SHALL store graphic records with these semantics:
- **Line `L`**: endpoints (x1,y1) and (x2,y2). On reading, the endpoints SHALL be ordered: if x2 < x1 the two points are swapped; else if x1 = x2 and y2 < y1 they are swapped.
- **Box `B`**: corners (x1,y1) and (x2,y2). On reading, x1 and x2 SHALL be sorted ascending, and y1 and y2 SHALL be sorted ascending, independently.
- **Arc `A`**: centre (x,y), radius r, start angle a and sweep angle b in degrees. Angles are measured counter-clockwise from the +x direction in a y-up frame; because the file's y axis points down (Requirement "Coordinate system"), a positive sweep appears clockwise on screen. A sweep of 360 is a full circle. Values are stored as read, without normalisation.
- **Polygon `P`**: `count` points in order. If the last point equals the first, the polygon is closed; otherwise it is an open polyline. If fewer than `count` pairs can be read, the record SHALL be skipped.

Recognised keys in their property strings (other keys SHALL be preserved verbatim):

| Key | Applies to | Values and default |
|---|---|---|
| `fill` | `B` | `full` solid; `false` (boolean, Requirement "Boolean values") none; anything else, including absent, stippled |
| `fill` | `A`, `P` | `full` solid; `true` stippled; anything else, including absent, none |
| `dash` | `L`, `B`, `A`, `P` | integer dash length; negative values are treated as 0; absent = 0 (solid) |
| `bus` | `L`, `B`, `A`, `P`, `N` | numeric line width; absent = normal width |
| `ellipse` | `B` | `a,b` or `a b`: draw an elliptical arc inscribed in the box from angle a sweeping b degrees; an unparsable value means `0,360` |
| `bezier` | `P` | `true`: draw the points as a Bezier curve |
| `symbol_ignore` | all, in symbols | `true`: the object is not part of the symbol when the symbol is instantiated |

Compatibility: exact

(xschem ref: save.c:3384-3584, xschem.h:360-370)

#### Scenario: Line endpoints normalised
- **GIVEN** the records `L 4 20 0 10 5 {}` and `L 4 0 10 0 -10 {}`
- **WHEN** the file is read and written
- **THEN** they SHALL be written as `L 4 10 5 20 0 {}` and `L 4 0 -10 0 10 {}`

#### Scenario: Box corners normalised
- **GIVEN** the record `B 5 2.5 2.5 -2.5 -2.5 {name=A dir=in}`
- **WHEN** the file is read and written
- **THEN** it SHALL be written as `B 5 -2.5 -2.5 2.5 2.5 {name=A dir=in}`

### Requirement: Text records
A `T` record SHALL hold the displayed text (which MAY contain LF and `@` substitution tokens, see capability `schematic-symbols-and-libraries`), the anchor (x,y), rotation 0..3, flip 0..1, horizontal scale and vertical scale, and a property string. A text scale of 1.0 is the base font size. Recognised text keys (others preserved):

| Key | Meaning |
|---|---|
| `font` | font family name |
| `layer` | integer layer used for the text colour |
| `hcenter`, `vcenter` | `true`: centre horizontally / vertically on the anchor |
| `weight` | `bold` |
| `slant` | `italic` or `oblique` |
| `hide` | `true`: hidden unless "show hidden texts" is on; `instance`: hidden in placed instances of the symbol, visible when editing the symbol |
| `name` or `floater` | present: the text is a floater bound to the instance named by `name` (capability `schematic-symbols-and-libraries`) |

If any of the six numeric fields is missing, the record SHALL be skipped and the rest of the line discarded.

Compatibility: exact

(xschem ref: save.c:3283-3307, save.c:3136-3151)

#### Scenario: Multi-line text
- **GIVEN** the record `T {line one` LF `line two} 100 -50 0 0 0.4 0.4 {layer=4}`
- **WHEN** the file is read and written
- **THEN** the text SHALL have two lines and SHALL be written back byte-identically

### Requirement: Wire records
An `N` record SHALL be a wire segment from (x1,y1) to (x2,y2) with a property string. On reading, its endpoints SHALL be ordered as for lines (Requirement "Line, box, arc and polygon records"). Recognised keys: `lab` (the net name last computed for the wire; capability `schematic-netlisting` rewrites it), `bus` (draw thick; set automatically for bus nets), and the `*_ignore` keys of capability `schematic-netlisting`. Wires SHALL be the only records that carry connectivity.

Compatibility: exact

(xschem ref: save.c:3309-3333, save.c:3121-3134)

#### Scenario: Wire with stored net name
- **GIVEN** `N 890 -130 890 -110 {lab=ANALOG_GND}`
- **WHEN** the file is read
- **THEN** one vertical wire from (890,-130) to (890,-110) SHALL exist with property `lab=ANALOG_GND`

### Requirement: Component instance records
A `C` record SHALL place one instance of a symbol:
- **symbol reference**: a brace string naming the symbol (capability `schematic-symbols-and-libraries` resolves it). On a Unix host, a reference that starts with `/` SHALL be converted on reading to the shortest library-relative form (Requirement "Symbol references" of that capability).
- **origin** (x0,y0)
- **rotation** 0..3 and **flip** 0..1 (Requirement "Orientation transform")
- **property string**: the instance attributes. `name` is the instance name and SHALL be present for netlisted instances.

If any of the four numeric fields is missing, the record SHALL be skipped with the warning `missing fields for INSTANCE object`. An instance whose symbol cannot be found SHALL still be kept and written back unchanged; it SHALL be drawn with the placeholder symbol of capability `schematic-symbols-and-libraries`.

Compatibility: exact

(xschem ref: save.c:3335-3382, save.c:3080-3119)

#### Scenario: Instance read and written
- **GIVEN** `C {capa.sym} 890 -160 0 0 {name=C4 m=1 value=10u device="tantalium capacitor"}`
- **WHEN** the file is read and written
- **THEN** the record SHALL be written back byte-identically

### Requirement: Embedded symbols
A `[` record SHALL directly follow a `C` record and SHALL hold a complete symbol definition (records up to a line starting with `]`). On reading:
- the preceding instance SHALL get `embed=true` set in its property string (property update rules of Requirement "Property strings")
- if a symbol with the same reference is already loaded, the block SHALL be skipped; otherwise its records SHALL define that symbol (as a `.sym` would), and the symbol SHALL be marked embedded
- a `[` with no preceding instance SHALL be skipped with a warning
- every later instance that references the same symbol SHALL use the embedded definition

On writing, for each symbol, the block SHALL be emitted exactly once, right after the first instance (in instance order) that references that symbol and has `embed=true`:

```
[
v {xschem version=<V> file_version=1.3}
K <symbol property>
G {}
V {}
S {}
E {}
<L records by layer 0..21>
<B records by layer 0..21>
<A records by layer 0..21>
<T records>
<P records by layer 0..21>
]
```

The embedded block SHALL NOT contain `F`, `N` or `C` records and SHALL write polygons after texts. Wires of a schematic-used-as-symbol appear there as `L 1` records. These are xschem quirks that Hefesto SHALL reproduce whatever the `xschem-compat` setting, because they change only the layout of the block, which reads back the same either way (Q-SCHEM-2, `Resolution: replicate`).

Compatibility: exact

(xschem ref: save.c:3010-3078, save.c:3640-3690)

#### Scenario: One embedded copy
- **GIVEN** three instances of `res.sym` of which the second and third have `embed=true`
- **WHEN** the schematic is saved
- **THEN** exactly one `[` ... `]` block SHALL follow the second instance, and none SHALL follow the first or third

### Requirement: Property strings
Every property string SHALL be parsed as a list of attributes with this grammar. Separators are space, tab, LF and `;`.

```ebnf
props     = { sep } , [ attr , { sep , { sep } , attr } ] , { sep } ;
attr      = key , [ { sp } , "=" , { sp } , value ] ;
key       = keychar , { keychar } ;      (* ends at an unquoted, unescaped separator or "=" *)
value     = valchar , { valchar } ;      (* ends at an unquoted, unescaped separator *)
sep       = " " | TAB | LF | ";" ;
sp        = sep ;
```

Quoting and escaping inside keys and values:
- A backslash that is not itself escaped escapes the next character.
- A `"` that is not escaped toggles quoted mode. Separators inside quoted mode belong to the value.

Lookup of key `k` SHALL return the first attribute whose key equals `k` exactly (case-sensitive):
- An attribute written without `=` exists and has the empty value.
- **Decoded value** (default): the characters of the value with every unescaped `"` removed, every unescaped `\` removed, and each escaped character kept literally.
- **Raw value**: the characters exactly as stored. Netlist `format` strings are read decoded but without Tcl evaluation (capability `schematic-netlisting`).
- If a decoded value begins with `tcleval(`, it SHALL be passed to Tcl evaluation (capability `schematic-symbols-and-libraries`, Requirement "tcleval values") unless the caller asks for the unevaluated value.

Setting key `k` to value `v` SHALL follow these rules:
1. If `v` contains `;`, LF, space or tab, is not already enclosed in `"`, and `k` is not `name`, `v` SHALL be written as `"v"`.
2. If `k` exists, the value of its first occurrence SHALL be replaced. An attribute without `=` SHALL become `k=v` followed by one space.
3. If `k` does not exist and the string is empty, the result SHALL be `k=v`.
4. If `k` does not exist and the string ends in a space or LF, `k=v` SHALL be appended. Otherwise LF followed by `k=v` SHALL be appended.
5. Setting an empty value SHALL remove the first occurrence of `k`, together with its `=`, its value and one following space or LF.

Compatibility: exact

(xschem ref: token.c:24, token.c:479-570, token.c:1288-1466)

#### Scenario: Quoted value with spaces
- **GIVEN** the property string `name=C4 m=1 device="tantalium capacitor"`
- **WHEN** key `device` is looked up
- **THEN** the decoded value SHALL be `tantalium capacitor`

#### Scenario: Spaces around equals and semicolon separator
- **GIVEN** the property string `a = 1;b=2`
- **WHEN** keys `a` and `b` are looked up
- **THEN** they SHALL be `1` and `2`

#### Scenario: Setting a new key
- **GIVEN** the property string `name=p1` with no trailing whitespace
- **WHEN** key `lab` is set to `VDD`
- **THEN** the string SHALL become `name=p1` LF `lab=VDD`

#### Scenario: Setting a value with spaces
- **GIVEN** the property string `name=x1 value=1`
- **WHEN** key `value` is set to `.lib a b`
- **THEN** the string SHALL become `name=x1 value=".lib a b"`

### Requirement: Boolean values
Wherever this specification says an attribute is "true" or "false", the comparison SHALL work as follows:
- A value made only of decimal digits is true if non-zero and false if zero.
- `true`, `on` and `yes` are true; `false`, `off` and `no` are false (ASCII case-insensitive).
- Any other value equals the word only if it is identical to it, so `TRUEISH` is neither true nor false.

Compatibility: exact

(xschem ref: editprop.c:112-135)

#### Scenario: Numeric boolean
- **WHEN** an instance has `spice_ignore=1`
- **THEN** it SHALL be treated as `spice_ignore=true`

### Requirement: Coordinate system
Coordinates SHALL be unitless binary64 values in a frame whose x axis points right and whose y axis points down, as displayed. One coordinate unit has no physical size. The editor snap default SHALL be 10 units and the displayed grid default 20 units (capability `schematic-editing`). Symbol pins are conventionally 5×5 boxes centred on multiples of 10.

Compatibility: exact

(xschem ref: xschem.tcl:11926-11947)

#### Scenario: Pin centre
- **GIVEN** the pin box `B 5 -22.5 -2.5 -17.5 2.5 {name=G dir=in}`
- **WHEN** its connection point is computed
- **THEN** it SHALL be (-20, 0), the centre of the box

### Requirement: Orientation transform
An instance with origin (x0,y0), rotation r and flip f SHALL map a symbol-local point (x,y) to the schematic point (x0+rx, y0+ry), where x' = −x if f = 1, else x' = x, and:

| r | rx | ry |
|---|---|---|
| 0 | x' | y |
| 1 | −y | x' |
| 2 | −x' | −y |
| 3 | y | −x' |

The flip is applied before the rotation. On screen (y down) each rotation step turns the symbol 90° clockwise. The same transform SHALL apply to texts: a text inside a symbol placed with (r, f) SHALL get rotation `(t_rot + (f = 1 and t_rot odd ? r + 2 : r)) mod 4` and flip `t_flip XOR f`. An arc (centre transformed as a point) SHALL get start angle `fmod(a + 270·r, 360)` when f = 0, or `fmod(270·r + 180 − b − a, 360)` when f = 1, plus 360 if negative; the sweep b is unchanged.

Compatibility: exact

(xschem ref: xschem.h:351-358, save.c:5195-5215, save.c:5400-5410)

#### Scenario: Rotated pin position
- **GIVEN** an instance of a symbol with a pin centred at (-20,0), placed at (100,200) with r=1, f=0
- **WHEN** the pin position is computed
- **THEN** it SHALL be (100, 180)

#### Scenario: Flipped pin position
- **GIVEN** the same symbol placed at (100,200) with r=0, f=1
- **WHEN** the pin position is computed
- **THEN** it SHALL be (120, 200)

### Requirement: Canonical write order
Hefesto SHALL write a file in this order:
1. the `v` record
2. `G`, `K`, `V`, `S`, `F`, `E` (Requirement "Global property records")
3. all `L` records, grouped by layer 0, 1, ... 21, in read/creation order within a layer
4. all `B` records, grouped the same way
5. all `A` records, grouped the same way
6. all `P` records, grouped the same way
7. all `T` records in order
8. all `N` records in order
9. all `C` records in order, each followed by its embedded block when required

Each record SHALL be written on one line (brace strings MAY contain LFs) in this form, where `%g` means the numeric format of Requirement "Numeric fields" and `{s}` a brace string:

```
L %d %g %g %g %g {s}
B %d %g %g %g %g {s}
A %d %g %g %g %g %g {s}
P %d %d %g %g ... %g %g {s}      (each coordinate followed by one space)
T {s} %g %g %d %d %g %g {s}
N %g %g %g %g {s}
C {s} %g %g %d %d {s}
```

Fields SHALL be separated by exactly one space. The order of objects inside a list SHALL be the order of the in-memory model (capability `schematic-editing` defines how edits and "change element order" alter it).

Compatibility: exact

(xschem ref: save.c:3223-3281, save.c:3080-3221)

#### Scenario: Layers regrouped
- **GIVEN** a file with records in the order `L 4 ...`, `B 5 ...`, `L 3 ...`, `T ...`, `N ...`, `L 4 ...`
- **WHEN** it is read and saved
- **THEN** the written order SHALL be: the `L 3` record, the two `L 4` records in their original order, the `B 5` record, the `T` record, the `N` record

### Requirement: Round-trip guarantee
Reading a file and writing it with no edit SHALL produce exactly what xschem 3.4.8RC writes for the same file. In particular, for a file last written by xschem with file format 1.3 and the same version string, the output SHALL be byte-identical to the input. For other files, the only differences SHALL be the canonicalisations listed in this capability:
- version line and global-record order
- the added `F {}` record
- layer grouping
- endpoint and corner ordering
- numeric reformatting
- re-escaping of brace strings
- added `embed=true`
- dropped `#` and unknown records
- dropped content after the last field on a line

Hefesto SHALL NOT write a file that the user has not modified, unless the user explicitly asks to save it.

Compatibility: exact

(xschem ref: save.c:3223-3281, save.c:3585-3734)

#### Scenario: PDK example unchanged
- **GIVEN** `sky130_tests/not.sch` from `xschem_sky130` re-saved once by xschem 3.4.8RC
- **WHEN** Hefesto opens it and saves it with no edit
- **THEN** the output SHALL be byte-identical to the xschem-saved file

### Requirement: Malformed input
Hefesto SHALL read a malformed file as far as possible and SHALL report each problem without aborting:
- a record with missing numeric fields SHALL be skipped and the rest of its line discarded, with the warning `missing fields for <KIND> object` (KIND is `LINE`, `xRECT`, `ARC`, `POLYGON`, `TEXT`, `WIRE` or `INSTANCE`)
- an invalid layer SHALL be handled as in Requirement "Layers"
- a missing closing brace SHALL be handled as in Requirement "Brace strings"
- the exact message texts are informative

The skip and recovery behaviour is exact; the wording is informative.

Compatibility: exact

(xschem ref: save.c:3283-3584)

#### Scenario: Truncated wire
- **GIVEN** the line `N 0 0 10` followed by the line `N 0 10 20 10 {}`
- **WHEN** the file is read
- **THEN** the first record SHALL be skipped with a warning and only the wire (0,10)-(20,10) SHALL be loaded

### Requirement: Safe saving
Saving SHALL write the complete file to a temporary file in the destination folder and then atomically replace the destination, through the platform file services of capability `hefesto-files-and-pdks`. A failed write SHALL leave the previous file intact and report the error. Hefesto SHALL detect when the file on disk changed since it was loaded and SHALL ask before overwriting it.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Disk full
- **GIVEN** a schematic open on a volume that becomes full during the save
- **WHEN** the user saves
- **THEN** the original file SHALL be unchanged and an error SHALL be shown

### Requirement: Complete annotated example
The following symbol file SHALL be read into the model described in the comments, and written back byte-identically. Comments after `<--` are not part of the file.

```
v {xschem version=3.4.8RC file_version=1.3}      <-- version record, no header text
G {}                                              <-- VHDL text: empty
K {type=regulator                                 <-- symbol property, 4 attributes:
format="x@name @pinlist r@symname"                <--   format (decoded without quotes)
verilog_format="assign @#2 = @#0 ;"
template="name=U1 footprint=TO220"}               <--   template: default instance attrs
V {}
S {}
F {}
E {}
L 4 -60 0 -50 0 {}                                <-- symbol drawing, layer 4
L 4 -50 -20 50 -20 {}
B 5 -62.5 -2.5 -57.5 2.5 {name=IN dir=in pinnumber=1}    <-- pin 0, centre (-60,0)
B 5 -2.5 27.5 2.5 32.5 {name=GND dir=inout pinnumber=2}  <-- pin 1, centre (0,30)
B 5 57.5 -2.5 62.5 2.5 {name=OUT dir=out pinnumber=3}    <-- pin 2, centre (60,0)
T {@name} -17.5 -15 0 0 0.2 0.2 {}                <-- shows the instance name
T {@symname} -17.5 0 0 0 0.2 0.2 {}               <-- shows the symbol name
```

And this schematic places it:

```
v {xschem version=3.4.8RC file_version=1.3}
G {}
K {}
V {}
S {.option temp=27}                               <-- copied into the SPICE netlist
F {}
E {}
N 0 0 40 0 {lab=VIN}                              <-- wire, last computed net VIN
C {lab_pin.sym} 0 0 0 0 {name=p1 lab=VIN}         <-- label instance naming the net
C {7805.sym} 100 0 0 0 {name=U1 footprint=TO220}  <-- pin IN at (40,0) touches the wire
```

Compatibility: exact

(xschem ref: save.c:3223-3281)

#### Scenario: Example pin positions
- **WHEN** the example schematic is read
- **THEN** instance `U1` SHALL have pins IN at (40,0), GND at (100,30) and OUT at (160,0)
