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
rot         = integer ;                 (* normally 0..3; other values are kept, see Rules *)
flip        = integer ;                 (* normally 0..1; other values are kept, see Rules *)
num         = decimal-floating-point ;
ws          = { " " | TAB | CR | LF } ;
ws1         = ws ;                       (* at least the separator the writer emits *)
```

Rules:
- Records MAY appear in any order and any record kind MAY repeat. For a repeated `v`, `K`, `G`, `V`, `S`, `F` or `E` record the last one read SHALL win.
- A `bstring` MAY span lines (Requirement "Brace strings"). The "discard to end of line" step applies after the closing brace of the last field.
- An unknown tag SHALL be skipped up to the next LF that is not inside a brace string; braces in it SHALL be read as brace strings so that an LF inside them does not end the record.
- A `#` record SHALL be skipped to the end of its line.
- **Integer fields.** An integer field is an optional sign followed by decimal digits; reading stops at the first other character (so `4.5` as a layer reads `4`, and `.5` starts the next field). A field with no digits is a missing field (Requirement "Malformed input").
- **Rotation and flip values.** The rotation and flip of `T` and `C` records SHALL be stored as read and written back as read, also outside 0..3 and 0..1; Requirement "Orientation transform" says how such values are used. xschem keeps them as 16-bit integers.
- **Out-of-range integers (Q-SCHEM-34, `Resolution: fix`, approved by the user 2026-11-25).** C's reading of an integer that does not fit its field is undefined, so xschem has no result to match. A layer, polygon count, rotation or flip whose value does not fit a signed 32-bit integer, and a rotation or flip outside -32768..32767, SHALL make the record skipped as a missing field, in both modes; a layer that fits but is outside 0..21 is an invalid layer (Requirement "Layers").
- **Polygon count.** A count of 0 SHALL give a polygon with no points, kept and written back as `P <layer> 0 {...}`. A negative count SHALL make the record skipped as a missing field (xschem allocates from the negative count, which is undefined; Q-SCHEM-34, `Resolution: fix`, both modes). No storage SHALL be reserved from the count before the points are read: the points are read one pair at a time, and a record with fewer pairs than its count is skipped (Requirement "Line, box, arc and polygon records").

Compatibility: exact

(xschem ref: save.c:3585, save.c:2826)

#### Scenario: Unknown record skipped
- **GIVEN** a schematic containing the line `Z 1 2 {a` LF `b} 3` followed by `N 0 0 10 0 {}`
- **WHEN** the file is read
- **THEN** the `Z` record, including its two-line brace string, SHALL be skipped, and exactly one wire from (0,0) to (10,0) SHALL be loaded

#### Scenario: Rotation and flip outside their usual range
- **GIVEN** the record `C {res.sym} 0 0 5 2 {name=R1}`
- **WHEN** the file is read and written
- **THEN** the instance SHALL have rotation 5 and flip 2, and the record SHALL be written back byte-identically

#### Scenario: Polygon counts
- **GIVEN** the records `P 4 0 {}`, `P 4 -1 {}` and `P 4 2 0 0 1 1 {}`
- **WHEN** the file is read and written
- **THEN** `P 4 0 {}` and `P 4 2 0 0 1 1 {}` SHALL be written back, and `P 4 -1 {}` SHALL be skipped with the warning `missing fields for POLYGON object`, with `xschem-compat` ON or OFF (Q-SCHEM-34)

#### Scenario: Integer that does not fit
- **GIVEN** the records `L 99999999999 0 0 1 1 {}`, `C {res.sym} 0 0 40000 0 {name=R1}` and `N 0 0 1 0 {}`, with `xschem-compat` ON or OFF
- **WHEN** the file is read
- **THEN** the line and the instance SHALL be skipped with a warning, and the wire SHALL be loaded (Q-SCHEM-34)

#### Scenario: Rotation and flip at the 16-bit bounds
- **GIVEN** the records `C {res.sym} 0 0 32767 -32768 {name=R1}`, `C {res.sym} 10 0 0 -32769 {name=R2}` and `T {x} 0 0 -32769 0 0.4 0.4 {}`, with `xschem-compat` ON or OFF
- **WHEN** the file is read and written
- **THEN** `R1` SHALL be kept and written back with rotation `32767` and flip `-32768`, and the `R2` instance and the text SHALL be skipped with a warning (Q-SCHEM-34)

### Requirement: Brace strings
Every text field and property field SHALL be a brace string. Reading SHALL work as follows:
1. Characters up to and including the first `{` are skipped.
2. After that, a backslash that is not itself escaped starts an escape: the backslash is dropped and the next character is taken literally.
3. An unescaped `}` ends the string.
4. CR characters are dropped. All other characters, including LF, are kept.
5. An empty result SHALL be stored as "no value" (absent property), which is equivalent to the empty string for every lookup.
6. End of file inside a brace string, or before its opening `{`, SHALL make that field absent and SHALL report `EOF reached, malformed {...} string input, missing close brace`. The record keeps the fields read before it: a record whose numeric fields were all read is kept with the field absent (Requirement "Malformed input"), and a record that still needs a numeric field after it is skipped as a missing field.

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

Writing SHALL format every floating-point field with the C format `%.16g` and every integer field with `%d`. The source text of a numeric field SHALL NOT be kept: a field is written from its binary64 value on every save, edited or not, which is what xschem does (golden case SCHEM-RT-02: `%.16g` reproduces xschem's first save of 1373 of the 1375 PDK files; the other two differ only in the layer-5 box order of Requirement "Canonical write order"). Consequences that SHALL hold:
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
- The header text SHALL be the `v` string with the keys `xschem`, `version` and `file_version` removed, in that order (the key `xschem` only if the substring `xschem` starts within the first 50 characters), then leading spaces and tabs removed, then one leading LF removed. Each key is removed by setting it to the empty value (Requirement "Property strings", setting rule 5): its first occurrence goes, with its `=` and value if it has them and one following space or LF; a key that is absent removes nothing. So in `xschem version=3.4.7RC file_version=1.2` LF LF `* text` the removals leave LF `* text`, and the header text is `* text` (golden case SCHEM-RT-02, gf180mcu headers that start with an empty line).

On writing, the `v` record SHALL be the first line of the file:
- with no header text: `v {xschem version=<V> file_version=1.3}`
- with header text: `v {xschem version=<V> file_version=1.3` LF `<header text>}` (brace-escaped)

where `<V>` is the xschem compatibility version (Requirement "Writer version string"). Hefesto SHALL always write file format `1.3` regardless of the version read.

File version `1.0` SHALL change the meaning of symbol references: a reference is a symbol name without extension, and `.sym` SHALL be appended (replacing any extension) when the instance is loaded, so the reference is written back with `.sym`. Independently of when each instance got the extension, a document whose file version (as read) is `1.0` SHALL resolve every instance reference with `.sym` appended (replacing any extension) and SHALL write every instance reference that way, so a saved file never holds a reference without `.sym` that came from a 1.0 file (golden case SCHEM-RT-08). From `1.1` on, the reference is used and written as written. When the file has no `v` record, the first record depends on the `xschem-compat` setting (capability `hefesto-magic-compat-mode`; Q-SCHEM-3, `Resolution: compat`):
- with `xschem-compat` ON, the file version is unset (not `1.0`) until the end of the first record, as in xschem, so an instance record that comes before any other record is not given the extension when it is read: its stored reference, which property queries such as `xschem getprop instance <name> cell::name` return, stays as written; later records see `1.0`. Resolution and writing still add `.sym` (paragraph above), so the instance shows the same symbol and is saved the same way in both modes
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
- **GIVEN** `xschem-compat` ON and a schematic with no `v` record whose first record is `C {res} 0 0 0 0 {name=R1}`, followed by `C {capa} 100 0 0 0 {name=C1}` (golden case SCHEM-RT-08, input `b.sch`)
- **WHEN** it is read
- **THEN** the stored reference of `R1` SHALL be `res` and that of `C1` `capa.sym`, and `R1` SHALL resolve to `res.sym`
- **WHEN** it is then saved, and the saved file read and saved again
- **THEN** both saved files SHALL contain `C {res.sym} 0 0 0 0 {name=R1}` and `C {capa.sym} 100 0 0 0 {name=C1}` (SCHEM-RT-08 `b.sch`, `2-b.sch`)

#### Scenario: Leading instance record, xschem-compat OFF
- **GIVEN** `xschem-compat` OFF and the same schematic
- **WHEN** it is read and saved
- **THEN** the stored references SHALL be `res.sym` and `capa.sym`, and the saved file SHALL be the same as with `xschem-compat` ON

#### Scenario: Header keys removed with their separator
- **GIVEN** a symbol whose first record is `v {xschem version=3.4.7RC file_version=1.2` LF LF `* Copyright 2022 X` LF `}`
- **WHEN** it is read and saved
- **THEN** the header text SHALL be `* Copyright 2022 X` LF, and the first record written SHALL be `v {xschem version=3.4.8RC file_version=1.3` LF `* Copyright 2022 X` LF `}`

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
A `[` record normally directly follows a `C` record and holds a complete symbol definition (records up to a line starting with `]`). On reading a `[` record at the top level of a file:
- the block SHALL belong to the last instance read before it in the file, whatever records lie between that instance and the `[` (xschem does not check that the previous record is a `C` record); Hefesto MAY report a warning when the previous record is not a `C` record
- that instance SHALL get `embed=true` set in its property string (property update rules of Requirement "Property strings"), also when the block is then skipped
- if no instance has been read yet, or the instance's symbol reference is empty, or a symbol with the same reference is already loaded (for example from an earlier block), the block SHALL be skipped; otherwise its records SHALL define that symbol (as a `.sym` would), and the symbol SHALL be marked embedded
- **skipping** a block SHALL discard every line up to and including the first later line that starts with `]`, without counting nested `[` lines, or up to the end of the file; a `[` with no instance before it SHALL also report a warning
- every later instance that references the same symbol SHALL use the embedded definition

A block that defines a symbol SHALL end at the first line that starts with `]` at its own level, or at the end of the file. At the end of the file the records read so far SHALL define the symbol; xschem reports nothing, and Hefesto MAY report a warning. Inside a block that defines a symbol:
- a `C` record SHALL be an instance drawn inside the embedded symbol, handled as an instance inside a schematic used as a symbol (capability `schematic-symbols-and-libraries`, Requirement "Schematic used as symbol"): its symbol's graphics are added to the embedded symbol, transformed by the instance's placement (Requirement "Orientation transform"), its layer-5 boxes on layer 7, one nesting level deeper
- when that instance's symbol is not found as a file and the next non-blank character after its record is `[`, that nested block SHALL define the instance's symbol in place (its records read at the deeper level, up to the `]` that closes it); any other `[` inside a block SHALL be skipped as above
- `N` records SHALL become lines on layer 1, kept with the `L 1` lines in the order the records are met (depth-first through nested instances and blocks)
- `v`, `G` (when a `K` was read), `V`, `S`, `F` and `E` records SHALL be ignored for the symbol; `K`, or `G` when no `K` was read, SHALL be the symbol property only at the block's own level
- floater texts and boxes whose `flags` contain `graph` SHALL be dropped, and objects with `symbol_ignore=true` SHALL be dropped

The nesting depth of symbol levels inside one block SHALL follow the 40-level limit of Requirement "Schematic used as symbol" of capability `schematic-symbols-and-libraries`. In addition, the reader SHALL keep at most **64** levels of nested `[` blocks open at once, in both modes of `xschem-compat` (input limit approved by the user on 2026-11-21, Q-SCHEM-32): a `[` that would open a 65th level SHALL be skipped as above (up to the next line that starts with `]`) and SHALL report the warning `embedded symbol blocks nested deeper than 64 levels; block skipped`. The reader SHALL read nested blocks without recursion.

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

The embedded block SHALL NOT contain `F`, `N` or `C` records or nested blocks: it is written from the symbol as built (graphics of nested instances and nested blocks already added, layer-5 boxes of nested levels on layer 7), and SHALL write polygons after texts. Wires of a schematic-used-as-symbol, and `N` records of a block, appear there as `L 1` records, in the order of the layer-1 lines of the symbol as built. The layer-5 boxes of the block SHALL be written in the symbol's pin order (capability `schematic-symbols-and-libraries`, Requirement "Pin order"). These are xschem quirks that Hefesto SHALL reproduce whatever the `xschem-compat` setting, because they change only the layout of the block, which reads back the same either way (Q-SCHEM-2, `Resolution: replicate`).

Compatibility: exact

(xschem ref: save.c:3010-3078, save.c:3640-3690)

#### Scenario: One embedded copy
- **GIVEN** three instances of `res.sym` of which the second and third have `embed=true`
- **WHEN** the schematic is saved
- **THEN** exactly one `[` ... `]` block SHALL follow the second instance, and none SHALL follow the first or third

#### Scenario: Block after a wire belongs to the last instance
- **GIVEN** the records `C {a.sym} 0 0 0 0 {name=x1}`, `N 0 0 10 0 {}`, then a block `[` LF `K {type=x}` LF `]`, and no other definition of `a.sym`
- **WHEN** the file is read
- **THEN** `x1` SHALL get `embed=true`, the block SHALL define `a.sym` with symbol property `type=x`, and the wire SHALL be loaded

#### Scenario: Block with no instance before it
- **GIVEN** a file whose first records are `N 0 0 1 1 {}`, then `[` LF `K {type=x}` LF `N 5 5 6 6 {}` LF `]`, then `N 2 2 3 3 {}`
- **WHEN** the file is read
- **THEN** the block SHALL be skipped with a warning, no symbol SHALL be defined, and exactly the wires (0,0)-(1,1) and (2,2)-(3,3) SHALL be loaded

#### Scenario: Skipped block ends at the first closing line
- **GIVEN** an instance `C {a.sym} 0 0 0 0 {}` followed by a block defining `a.sym`, then a second instance `C {a.sym} 10 0 0 0 {}` followed by `[` LF `C {b.sym} 0 0 0 0 {}` LF `[` LF `K {type=b}` LF `]` LF `N 7 7 8 8 {}` LF `]` LF `N 2 2 3 3 {}`
- **WHEN** the file is read
- **THEN** the second block SHALL be skipped up to the first `]` line, so the wire (7,7)-(8,8) and the wire (2,2)-(3,3) SHALL both be loaded as wires of the schematic, the line `]` after the first wire SHALL be skipped as an unknown record, and `b.sym` SHALL NOT be defined

#### Scenario: Block cut by the end of the file
- **GIVEN** a file that ends with `C {a.sym} 0 0 0 0 {}`, `[`, `K {type=x}`, `L 4 0 0 1 1 {}` and no `]`
- **WHEN** the file is read and saved
- **THEN** `a.sym` SHALL be defined with the one line, and the saved file SHALL contain a complete block for it ending with `]`

#### Scenario: Nested block merged into the outer symbol
- **GIVEN** `C {a.sym} 0 0 0 0 {}` followed by the block `[`, `K {type=a}`, `C {b.sym} 100 0 0 0 {}`, `[`, `K {type=b}`, `L 4 0 0 10 0 {}`, `B 5 -2.5 -2.5 2.5 2.5 {name=P}`, `]`, `L 4 0 0 0 10 {}`, `]`, where no file `b.sym` exists
- **WHEN** the file is read and saved
- **THEN** `a.sym` SHALL hold the lines (100,0)-(110,0) and (0,0)-(0,10) on layer 4 and the box (97.5,-2.5)-(102.5,2.5) on layer 7, no pin, and the saved block SHALL contain no `C` record and no nested block

#### Scenario: Nesting deeper than 64 blocks
- **GIVEN** a file of 70 levels of `C {s<i>.sym} 0 0 0 0 {}` records each followed by `[`, then 70 `]` lines, then `N 2 2 3 3 {}`, with `xschem-compat` ON or OFF
- **WHEN** the file is read
- **THEN** the reader SHALL report the warning `embedded symbol blocks nested deeper than 64 levels; block skipped` once, for the block that would open level 65, SHALL skip that block up to the next line that starts with `]`, SHALL NOT crash or recurse, and SHALL give the same document in both modes (the 40-level warning of capability `schematic-symbols-and-libraries` is reported as well, when the symbol is built)

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
2. If `k` exists, the value of its first occurrence SHALL be replaced. An attribute without `=` that is followed by a separator SHALL become the key, that separator, `=v` and one space, before the next attribute (golden case SCHEM-PR-01: setting `k` to `5` in `k name=x1` gives `k =5 name=x1`). An attribute without `=` at the very end of the string SHALL become the key, `=v` and one space: setting `k` to `5` in `k` gives `k=5 ` and in `a=1 k` gives `a=1 k=5 ` (both with a trailing space).
3. If `k` does not exist and the string is empty, the result SHALL be `k=v`.
4. If `k` does not exist and the string ends in a space or LF, `k=v` SHALL be appended. Otherwise LF followed by `k=v` SHALL be appended.
5. Setting an empty value SHALL remove the first occurrence of `k`, together with its `=`, its value and one following space or LF. Separators before `k` are kept: removing `k` from `a=1 k` gives `a=1 ` (with the trailing space), and from `k` gives the empty string.

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

#### Scenario: Setting a key written without equals
- **GIVEN** the property string `k name=x1`
- **WHEN** key `k` is set to `5`
- **THEN** the string SHALL become `k =5 name=x1` (golden case SCHEM-PR-01)

#### Scenario: Removing a key
- **GIVEN** the property string `name=x1 value=1 m=2`
- **WHEN** key `value` is set to the empty value
- **THEN** the string SHALL become `name=x1 m=2` (golden case SCHEM-PR-01)

#### Scenario: Setting a key written without equals at the end
- **GIVEN** the property strings `k` and `a=1 k`
- **WHEN** key `k` is set to `5` in each
- **THEN** they SHALL become `k=5 ` and `a=1 k=5 `, each ending in one space

#### Scenario: Removing a key written without equals at the end
- **GIVEN** the property strings `a=1 k` and `k`
- **WHEN** key `k` is set to the empty value in each
- **THEN** they SHALL become `a=1 ` and the empty string

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
An instance with origin (x0,y0), rotation r and flip f SHALL map a symbol-local point (x,y) to the schematic point (x0+rx, y0+ry), where x' = −x if f ≠ 0, else x' = x, and:

| r | rx | ry |
|---|---|---|
| 0 | x' | y |
| 1 | −y | x' |
| 2 | −x' | −y |
| any other value (3, and also 4, 5, −1, ...) | y | −x' |

The flip is applied before the rotation. On screen (y down) each rotation step turns the symbol 90° clockwise. A flip is a mirror whenever it is non-zero: flip 2 mirrors exactly as flip 1 does. The same transform SHALL apply to texts: a text inside a symbol placed with (r, f) SHALL get rotation `(t_rot + (f ≠ 0 and t_rot odd ? r + 2 : r)) mod 4` (the remainder taken as 0..3, also for negative sums) and flip `t_flip XOR f`. An arc (centre transformed as a point) SHALL get start angle `fmod(a + 270·r, 360)` when f = 0, or `fmod(270·r + 180 − b − a, 360)` when f ≠ 0, plus 360 if negative; the sweep b is unchanged. These rules use r and f as stored (Requirement "Record grammar", rule "Rotation and flip values"); values outside 0..3 and 0..1 are written back unchanged.

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

#### Scenario: Flip 2 mirrors and rotation 5 acts as 3
- **GIVEN** the same symbol placed at (100,200) with r=0, f=2, and again with r=5, f=0
- **WHEN** the pin positions are computed
- **THEN** they SHALL be (120, 200) and (100, 220), the positions for (r=0, f=1) and (r=3, f=0)

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

**Pin boxes on save.** Before each save, if the document has at least one box on layer 5 and every layer-5 box has a non-empty `sim_pinnumber`, the layer-5 boxes SHALL be put in order of the integer value of `sim_pinnumber` (read as Requirement "Pin order" of capability `schematic-symbols-and-libraries` says), ascending, with a stable sort that keeps the model order on ties (Q-SCHEM-5, `Resolution: fix`, both modes). The new order SHALL stay in the in-memory model and SHALL NOT mark the document modified. This applies to `.sym` and `.sch` files alike; if one layer-5 box has no `sim_pinnumber`, or an empty one, the order is left unchanged. Reading does not reorder: a file that is read and not saved keeps its box order in the model. Embedded blocks write their layer-5 boxes in the symbol's pin order (Requirement "Embedded symbols").

Compatibility: exact

(xschem ref: save.c:3223-3281, save.c:3080-3221)

#### Scenario: Layers regrouped
- **GIVEN** a file with records in the order `L 4 ...`, `B 5 ...`, `L 3 ...`, `T ...`, `N ...`, `L 4 ...`
- **WHEN** it is read and saved
- **THEN** the written order SHALL be: the `L 3` record, the two `L 4` records in their original order, the `B 5` record, the `T` record, the `N` record

#### Scenario: Pin boxes saved in sim_pinnumber order
- **GIVEN** a symbol whose layer-5 boxes are, in file order, `B 5 ... {name=D sim_pinnumber=2}`, `B 5 ... {name=G sim_pinnumber=1}`, `B 5 ... {name=S sim_pinnumber=3}` (the shape of IHP `nmoscl_2.sym` in golden case SCHEM-RT-02)
- **WHEN** it is read and saved
- **THEN** the boxes SHALL be written in the order G, D, S, and the document SHALL NOT be marked modified by the reordering

#### Scenario: Pin boxes not reordered when one lacks sim_pinnumber
- **GIVEN** a symbol whose layer-5 boxes are `{name=B sim_pinnumber=2}` and `{name=A}` in that order
- **WHEN** it is read and saved
- **THEN** the boxes SHALL be written in the order B, A

### Requirement: Round-trip guarantee
Reading a file and writing it with no edit SHALL produce exactly what xschem 3.4.8RC writes for the same file. In particular, for a file last written by xschem with file format 1.3 and the same version string, the output SHALL be byte-identical to the input. For other files, the only differences SHALL be the canonicalisations listed in this capability:
- version line and global-record order
- the added `F {}` record
- layer grouping
- endpoint and corner ordering
- the order of layer-5 boxes when every one has `sim_pinnumber` (Requirement "Canonical write order", paragraph "Pin boxes on save")
- `.sym` added to references of a file version `1.0` file (Requirement "Version record and header text")
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
- a record with a missing or non-numeric numeric field SHALL be skipped, with the warning `missing fields for <KIND> object` (KIND is `LINE`, `xRECT`, `ARC`, `POLYGON`, `TEXT`, `WIRE` or `INSTANCE`). Because the separators between fields include LF (Requirement "Record grammar"), the fields are looked for across line ends: reading stops at the first character that cannot start the expected number, and the characters from there up to and including the next LF SHALL be discarded. A record cut short at the end of a line therefore also loses the line that follows it (golden case SCHEM-RT-06)
- an invalid layer SHALL be handled as in Requirement "Layers"
- a missing closing brace, or the end of the file before an opening brace, SHALL be handled as in Requirement "Brace strings"
- integers that do not fit their field, negative polygon counts and rotation or flip values outside their usual range SHALL be handled as in Requirement "Record grammar"
- no input SHALL make the reader crash, recurse without bound, or allocate storage from a count read in the file before the data is there
- the exact message texts are informative

The skip and recovery behaviour is exact; the wording is informative.

Compatibility: exact

(xschem ref: save.c:3283-3584)

#### Scenario: Truncated wire
- **GIVEN** the line `N 0 0 10`, followed by the lines `N 0 10 20 10 {}` and `N 0 0 10 0 {}`
- **WHEN** the file is read
- **THEN** the first record SHALL be skipped with a warning, its fields are looked for on the second line, which stops the read at `N` and is discarded, and only the wire (0,0)-(10,0) SHALL be loaded (golden case SCHEM-RT-06)

#### Scenario: Unterminated property at the end of the file
- **GIVEN** a file whose last line is `N 0 0 10 0 {lab=A` with no closing brace
- **WHEN** the file is read and saved
- **THEN** the wire (0,0)-(10,0) SHALL be loaded with no property, the missing-brace warning SHALL be reported, and the saved file SHALL contain `N 0 0 10 0 {}` (golden case SCHEM-RT-06)

#### Scenario: End of file before the opening brace
- **GIVEN** a file whose last bytes are `N 0 0 10 0` with no brace and no LF, and another file whose last bytes are `T`
- **WHEN** each file is read
- **THEN** the first SHALL load the wire (0,0)-(10,0) with no property and report the missing-brace warning, and the second SHALL load no text and report a warning

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
