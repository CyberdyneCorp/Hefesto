# CIF Format I/O

## Purpose

Defines the Caltech Intermediate Form (CIF) as read and written by Hefesto in a Magic-compatible way: the lexical rules and grammar accepted on input, how symbols, calls, transformations, boxes, polygons, wires, round flashes, layers and the user extensions 9/91/94/95 are interpreted and scaled into the database, the exact text produced on output (header comments, symbol numbering and order, `DS` scale, box/polygon formatting, labels, calls and arrays), and the `cif` command options other than those that control hierarchical/array mask generation.

Related capabilities: `mask-input-styles` (cifinput style, CIF-layer processing, unit conversion, polygon conversion), `mask-output-styles` (cifoutput style, `S`/reducer/expander), `mask-output-operators` (operator geometry), `mask-hierarchical-generation` (how output geometry of a cell is generated, and the `cif *hier`/`cif *array` options).

Notation: on input, `N/Dn` is the current symbol scale (numerator/denominator), `S` and `M` are the input style scale factor and multiplier (capability `mask-input-styles`). On output, `S` is the output style scale factor, `R` the reducer and `E` the expander (capability `mask-output-styles`); one output-plane unit is `1/E` centimicron.

## Requirements

### Requirement: CIF lexical rules

Hefesto SHALL tokenize CIF input with these rules. A *blank* is any character other than a decimal digit, an upper-case ASCII letter, `-`, `;`, `(`, `)` and end of file; in particular lower-case letters are blanks. Between commands blanks are skipped. When an integer is expected, blanks AND upper-case letters are skipped first (separator rule). An integer is an optional `-` followed by one or more digits (value via C `atoi`); a negative value where an unsigned integer is required is reported ("negative integer not permitted.") but still used. A comment starts with `(` and ends at the matching `)`, nesting allowed; a comment reaching end of file is an error. Each command is followed by `;`; a missing `;` is reported ("`;' expected.") and parsing continues. Newlines are counted for line numbers in messages.

Compatibility: exact

(Magic ref: cif/CIFrdutils.c:348-470, cif/CIFrdutils.c:500-560, cif/CIFrdutils.c:1394-1460)

#### Scenario: Lower-case text is ignored
- **GIVEN** the input `DS 1 25 2; L CMF; Box 4 2 2 1; DF; End`
- **WHEN** it is read
- **THEN** `Box` is parsed as the box command `B` (the letters `ox` are blanks) and `End` as `E`

#### Scenario: Nested comment
- **GIVEN** the input `(outer (inner) still comment);`
- **WHEN** it is read
- **THEN** the whole parenthesized text is skipped as one comment

### Requirement: CIF input grammar

Hefesto SHALL accept CIF input matching the grammar below; any other leading character of a command is reported ("unknown command `c'; ignored.") and skipped to the next `;`.

Compatibility: exact

```ebnf
cif-file     = { blank } , { command , [ ";" ] } , end-cmd ;
command      = def-start | def-finish | def-delete | call | layer | box
             | polygon | wire | flash | user-ext | comment | (* empty *) ;
def-start    = "D" , { blank } , "S" , uint , [ uint , uint ] ;  (* DS n [a b] *)
def-finish   = "D" , { blank } , "F" ;
def-delete   = "D" , { blank } , "D" , uint ;
call         = "C" , uint , { transform } ;
transform    = "T" , point | "M" , { blank } , ( "X" | "Y" ) | "R" , sint , sint ;
layer        = "L" , { blank } , layer-name ;   (* up to 5 chars of [0-9A-Z] *)
box          = "B" , uint , uint , point , [ sint , sint ] ;
polygon      = "P" , point , { point } ;
wire         = "W" , uint , point , { point } ;
flash        = "R" , uint , point ;
point        = sint , sint ;
user-ext     = "9" , space , name                   (* cell name *)
             | "91" , name                           (* next instance id *)
             | "94" , name , point , [ name ]        (* point label *)
             | "95" , name , uint , uint , point , [ name ] ;  (* area label *)
end-cmd      = "E" , { blank } ;                      (* must be followed by EOF *)
name         = non-space-non-semicolon-chars ;       (* leading spaces/tabs skipped *)
```

Annotated example:

```
(written by a generator);
DS 1 25 2;          (symbol 1; coordinates * 25/2 = centimicrons);
9 inv;              (rename symbol 1 to "inv");
L CMF;              (current layer CMF);
    B 40 20 20 10;  (box length 40, width 20, center (20,10));
94 out 40 10 CMF;   (label "out" at (40,10) on CMF);
DF;
91 u1;              (next call gets instance id "u1");
C 1 R 0 1 T 100 0;  (place symbol 1 rotated 90 CCW, translated);
End
```

(Magic ref: cif/CIFrdutils.c:1609-1704, cif/CIFrdcl.c:1682-1730)

#### Scenario: Unknown command is skipped
- **GIVEN** a command `Q 1 2;` between valid commands
- **WHEN** the file is read
- **THEN** "unknown command `Q'; ignored." is reported and reading continues after the `;`

#### Scenario: Unsupported user extension
- **GIVEN** a command `92 foo;`
- **WHEN** the file is read
- **THEN** "unimplemented user extension; ignored." is reported

### Requirement: Symbol definitions and scale

Hefesto SHALL process `DS n [a b]` as starting the definition of symbol `n` (`n >= 0`; a negative or missing number is reported and the definition ignored; a `DS` inside another definition is reported and ignored). If `a` and `b` are present the symbol scale becomes `N = a*M`, `Dn = b`; if only `a` is present, or either is `<= 0`, the scale is 1/1 with an error message; if absent the scale is 1/1. Geometry inside the symbol is collected into a fresh set of read planes and no layer is current at symbol start. `DF` ends the definition: the cell's CIF layers are processed (capability `mask-input-styles`, "Per-cell layer processing order"), labels are adjusted, the bounding box recomputed, and the reader returns to top level with scale 1/1 and the top-level current layer restored. `DF` without `DS` is reported and ignored. A pending `91` id at `DS` or `DF` is discarded with an error.

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:421-500, cif/CIFrdcl.c:1163-1200)

#### Scenario: Symbol scale with style multiplier
- **GIVEN** an input style with `M = 10` and the command `DS 3 25 2;`
- **WHEN** the command is read
- **THEN** the symbol scale becomes `N = 250`, `Dn = 2`, so a CIF coordinate 4 equals 500 read units

### Requirement: Symbol-to-cell naming on read

Hefesto SHALL name the cell for symbol `n` with the decimal string of `n` (for example `"12"`), creating it on first reference by `DS` or `C`. Within one read, symbol `n`, whether it first appears in a `C n` or in a `DS n`, SHALL always map to a cell created by that read; an in-memory cell is never looked up by the name `"n"`, reused or cleared. Name collisions are resolved as `"n_k"` with the smallest `k >= 1` not in use: on a second `DS n` (after `DD n`, after a `9` renamed the symbol, or a plain redefinition) a cell defined earlier in the same read that still has the name `"n"` is renamed `"n_k"` with the warning "cell definition n reused.", as in Magic, and a fresh cell is created for the new definition; the earlier cell and its callers stay intact. A cell that existed before the read keeps its name, and the new cell becomes `"n_k"`. A forward `C n` and the later `DS n` name the same new cell. These rules apply in both `magic-compat` settings (Q-MASK-318, resolution: fix; Magic can clear a user cell named `"n"` or a cell defined earlier in the read, which loses data). `DD n` depends on the setting (Q-MASK-318, resolution: compat for `DD`): with `magic-compat` ON (the default) it forgets the mapping of symbol `n` only, as in Magic; with it OFF it forgets every symbol numbered `n` or higher, as CIF 2.0 specifies. Magic never writes `DD`, so Magic-written CIF reads the same in both settings.

The user extension `9 NAME` renames the current cell to `NAME`. Outside any definition it depends on the `magic-compat` setting (Q-MASK-317, resolution: compat): with it ON (the default) it renames the edit cell (the cell receiving top-level geometry), as in Magic; with it OFF it is ignored with a warning. If `NAME` already exists, the error "NAME already exists, so cell from CIF is named OLD." is reported and the name is unchanged.

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:199-290, cif/CIFrdcl.c:1218-1330)

#### Scenario: Redefined symbol keeps the old cell
- **GIVEN** a file with `DS 5; ... DF;` twice, with no `9` command
- **WHEN** the file is read
- **THEN** the first definition becomes cell `5_1` and the second cell `5`
- **AND** calls to symbol 5 read before the second `DS 5` refer to `5_1`

#### Scenario: Existing user cell is not cleared
- **GIVEN** a cell named `5` with paint is open before the read, and a file with `DS 5; ... DF; C 5; E`
- **WHEN** the file is read
- **THEN** the existing cell `5` keeps its paint, the definition creates cell `5_1`, and the top-level call places `5_1`

#### Scenario: DD forgets higher symbols with magic-compat OFF
- **GIVEN** a file that defines symbols 3 and 7, then `DD 3;`, then `C 7;`
- **WHEN** the file is read with `magic-compat` OFF
- **THEN** symbol 7 is forgotten, so `C 7` refers to a new, undefined cell, which is reported as used but not defined
- **AND** with `magic-compat` ON `C 7` places the cell defined earlier for symbol 7

### Requirement: Calls and transformations

Hefesto SHALL process `C n transforms` by creating an instance of the cell for symbol `n` in the current cell (or the edit cell at top level), with transform `T` built as follows: start from identity; for each transformation in order, compose so that it is applied after the previous ones: `T p` translates by `p` (a point at scale 1, see "Point scaling"); `M X` mirrors x (x → −x); `M Y` mirrors y (y → −y); `R a b` rotates the x axis onto the direction `(a,b)`: `(+,0)` identity, `(−,0)` 180°, `(0,+)` 90° counter-clockwise, `(0,−)` 90° clockwise; any other direction is reported "non-manhattan direction vector (a, b); ignored." and treated as identity. The final translation components are converted from read units to internal units with exact conversion (capability `mask-input-styles`). The instance gets the pending `91` id if any (then cleared), otherwise a default id. Placing a cell inside itself or its descendants is reported ("attempt to place cell use inside its own definition!") and skipped. A malformed transformation is reported; with `magic-compat` ON (the default) the call is still made with the transform parsed so far, unscaled, as in Magic; with it OFF "call ignored" is also reported, the call is skipped and the pending `91` id is cleared (Q-MASK-335, resolution: compat). With `magic-compat` OFF, a call to a symbol defined earlier in the read also copies that symbol's saved `copyup` planes into the calling cell's read planes under the call transform, and a call to a symbol defined later warns once per read and copies nothing; with it ON the CIF reader never copies them (Q-MASK-309, resolution: compat; capability `mask-input-styles` "Input operator copyup").

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:1334-1430, cif/CIFrdutils.c:1475-1606)

#### Scenario: Rotation then translation
- **GIVEN** scalefactor 100, symbol scale 1/1 and `C 1 R 0 1 T 200 300;`
- **WHEN** the call is read
- **THEN** the instance maps cell point (x,y) to (−y+2, x+3) in internal units

#### Scenario: Instance id from 91
- **GIVEN** `91 xbuf_0;` followed by `C 2;`
- **WHEN** the commands are read
- **THEN** the new instance of cell `2` has id `xbuf_0`

### Requirement: Point scaling on input

Hefesto SHALL convert each CIF integer coordinate `v` of a point with an element scale `iscale` (1 for ordinary points, 2 for box/flash centers and wire paths) to read units as `v*N*iscale / Dn`, applying the subdivision-or-rounding rule of capability `mask-input-styles` ("Mid-read subdivision of read units") when the product is not divisible by `Dn`. The x coordinate is processed before the y coordinate, and a subdivision triggered by y also multiplies the already computed x. Within a path, a subdivision multiplies all previously parsed points of the path.

Compatibility: exact

(Magic ref: cif/CIFrdutils.c:586-720)

#### Scenario: Exact division
- **GIVEN** `N = 25`, `Dn = 2` and the point `4 6`
- **WHEN** it is scaled with `iscale = 1`
- **THEN** the point is (50, 75) read units

### Requirement: Layer selection

Hefesto SHALL process `L name` by reading up to 5 characters from `[0-9A-Z]` after optional blanks as the layer name and skipping the rest of the command (see Q-MASK-312). If the name is a CIF layer of the current input style (including templayers), it becomes the current layer for subsequent geometry, and the current default label type becomes that layer's label type. Otherwise "layer NAME isn't known in the current style." is reported, and subsequent geometry is ignored until the next valid `L`. Geometry commands (`B`, `P`, `W`, `R`) issued when no layer is current are silently ignored.

Compatibility: exact

(Magic ref: cif/CIFrdpt.c:766-827, cif/CIFrdpt.c:68-80)

#### Scenario: Geometry before a layer command
- **GIVEN** a symbol `DS 1; B 10 10 5 5; L CMF; B 10 10 5 5; DF;`
- **WHEN** it is read
- **THEN** only the second box is placed on CMF

### Requirement: Box command

Hefesto SHALL process `B L W cx cy [dx dy]` as follows: `(L, W)` is scaled as a point with `iscale = 1`; `L <= 0` or `W <= 0` is reported and the box ignored. `(cx, cy)` is scaled with `iscale = 2` (giving `2c`); if this subdivides read units, `L` and `W` are multiplied by the same factor. The rectangle `[−L, L] × [−W, W]` is rotated by the direction `(dx, dy)` if present (same mapping as the `R` transformation), then each coordinate is computed as `(r + 2c) / 2`, and the resulting rectangle is added to the current layer. When `r + 2c` is odd the result depends on the `magic-compat` setting (Q-MASK-315, resolution: compat): with it ON (the default) C integer division truncates toward zero, as in Magic, so odd lengths lose width asymmetrically and a box straddling 0 can lose width on both sides (`B 3 2 0 0` gives x from −1 to 1); with it OFF the read units are first subdivided by 2 (the same mechanism as a center rescale, capability `mask-input-styles`), so the box is exact.

Compatibility: exact

(Magic ref: cif/CIFrdpt.c:68-177)

#### Scenario: Even box
- **GIVEN** scale 1/1 and `B 40 20 20 10;`
- **WHEN** it is read
- **THEN** the rectangle (0,0)-(40,20) read units is added

#### Scenario: Odd length centered on an integer
- **GIVEN** scale 1/1 and `B 3 2 10 0;`
- **WHEN** it is read
- **THEN** with `magic-compat` ON the x extent is `(−3+20)/2 = 8` to `(3+20)/2 = 11`, giving (8,−1)-(11,1)
- **AND** with `magic-compat` OFF the read units are subdivided by 2 and the box is (17,−2)-(23,2) in the new units, i.e. x from 8.5 to 11.5 in the original units

#### Scenario: Rotated box
- **GIVEN** scale 1/1 and `B 40 20 0 0 0 1;`
- **WHEN** it is read
- **THEN** the rectangle (−10,−20)-(10,20) is added

### Requirement: Round flash command

Hefesto SHALL process `R d cx cy` by computing `d' = d*N / Dn` (truncating; "Roundflash diameter snapped to nearest integer boundary." when inexact), scaling the center with `iscale = 2` (and multiplying `d'` by any subdivision factor), and adding the axis-aligned square `[(2cx − d')/2, (2cx + d')/2] × [(2cy − d')/2, (2cy + d')/2]` with truncating division. The flash is not approximated by a polygon, in both `magic-compat` settings (Q-MASK-316, resolution: replicate).

Compatibility: exact

(Magic ref: cif/CIFrdpt.c:179-237)

#### Scenario: Flash as a square
- **GIVEN** scale 1/1 and `R 10 50 50;`
- **WHEN** it is read
- **THEN** the square (45,45)-(55,55) is added

### Requirement: Polygon command

Hefesto SHALL process `P p1 p2 ...` by scaling every point with `iscale = 1` and converting the closed polygon to a region with the algorithm of capability `mask-input-styles` ("Polygon to region conversion"), adding it to the current layer. A malformed path is reported ("polygon, but improper path; ignored.").

Compatibility: exact

(Magic ref: cif/CIFrdpt.c:829-872)

#### Scenario: Rectangle polygon
- **GIVEN** scale 1/1 and `P 0 0 0 10 20 10 20 0;`
- **WHEN** it is read
- **THEN** the rectangle (0,0)-(20,10) is added

### Requirement: Wire command

Hefesto SHALL process `W w p1 p2 ...` by computing `w' = w*N / Dn` (truncating, warning "Wire width snapped to nearest integer boundary." when inexact), scaling the path with `iscale = 2` (doubled coordinates `P_i`; `w'` multiplied by any subdivision factor), and converting the path to polygons with square half-width end extensions in both `magic-compat` settings (Q-MASK-333, resolution: replicate; the CIF standard specifies round ends, which no reference tool produces) as follows (all in doubled coordinates, double-precision arithmetic, `round` = round half away from zero, results divided by 2):

1. Remove consecutive duplicate points. A single-point path uses the point as both ends.
2. For each segment `P_{i-1} → P_i` with angle `θ`: at the first point of a polygon, move the start back by `w'` along `−θ` (end extension of `w'/2` real units) and emit the two corner points `round((P ± w'·(cos(θ−90°), sin(θ−90°)))/2)`.
3. At `P_i` with next angle `φ` (or `φ = θ` at the last point, where `P_i` is also extended by `w'` along `θ`): `α = (φ−θ)/2`. If `|cos α| < 1e−10` (reversal) a warning is printed, the polygon is closed at `P_i` (with end extension) and a new polygon is started at `P_i`. Else if `|cos α| < 0.25` the error "mitre limit exceeded" is reported and the rest of the wire is dropped. Otherwise the two mitre points are `round((P_i ∓ (w'/cos α)·(cos δ, sin δ))/2)` with `δ = (φ+θ)/2 − 90°` (constant 90° = 1.5708 rad).
4. When a polygon is closed (end or reversal) its outline is converted with the polygon algorithm of `mask-input-styles` and added to the layer; at interior joints where a new offset segment would run backward relative to `θ` (absolute angle difference in (0.1, 6.1) rad), the previous offset point is moved to the intersection of the two offset lines when the lines are not parallel (`|sin(φ−θ_prev)| > 1e−4`).

Compatibility: exact

(Magic ref: cif/CIFrdpt.c:339-560, cif/CIFrdpt.c:705-764)

#### Scenario: Straight horizontal wire
- **GIVEN** scale 1/1 and `W 10 0 0 100 0;`
- **WHEN** it is read
- **THEN** the rectangle (−5,−5)-(105,5) is added

#### Scenario: L-shaped wire (verify against Magic)
- **GIVEN** scale 1/1 and `W 10 0 0 100 0 100 100;`
- **WHEN** it is read
- **THEN** the region is (−5,−5)-(105,5) united with (95,−5)-(105,105)

### Requirement: Label user extensions

Hefesto SHALL process `94 NAME x y [LAYER]` as a point label: the point is scaled with `iscale = 1` and converted to internal units with "any" (snapping) conversion; the label rectangle is that point. `95 NAME L W x y [LAYER]` is an area label: `(L, W)` is scaled with `iscale = 1`, the center with `iscale = 2`; `ll = 2c − (L,W)`, `ur = 2c + (L,W)`; if both components of `ll` are even, `ll` and `ur` are halved, otherwise the input style is rescaled by 2 and the symbol scale numerator is multiplied by 2, as `CIFParsePoint` does, so later geometry in the symbol keeps its size, in both `magic-compat` settings (Q-MASK-314, resolution: fix; Magic does not update the symbol scale and corrupts the following geometry of the symbol; golden cases with odd `95` corners get a deviation entry); each corner is then converted with "any" conversion. In both cases, if `LAYER` is given and is a CIF layer of the style, the label type is that layer's label type; if `LAYER` is unknown "label attached to unknown layer NAME." is reported and the label goes on space; if absent, the type is the current default label type (94) or space (95). With `LAYER` given, the label is sticky when that layer's label kind is not NONE. Without `LAYER` (a `94` or `95` taking the default type) the stickiness depends on the `magic-compat` setting (Q-MASK-327, resolution: compat): with it ON (the default), as in Magic, the label kind is taken from the first CIF layer whose index is less than the number of `layer`/`templayer` rules and whose label type equals the current default label type; with it OFF it is the label kind of the CIF layer selected by the current `L`, and the label is not sticky when no valid layer is current. Labels whose type is "discard" are dropped. Label names are read up to the first whitespace or `;` (see Q-MASK-313).

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:1432-1680)

#### Scenario: Point label on named layer
- **GIVEN** scalefactor 100, `labels CMF` under `layer m1 CMF`, scale 1/1, and `94 vdd 250 400 CMF;`
- **WHEN** it is read
- **THEN** a label `vdd` of type m1 is placed at point (2,4) (250 snaps to 2 because ties round down)

#### Scenario: Area label
- **GIVEN** scalefactor 1 and `95 bus 20 10 50 50 CMF;`
- **WHEN** it is read
- **THEN** a label `bus` with rectangle (40,45)-(60,55) is placed

### Requirement: End of file and top-level geometry

Hefesto SHALL stop reading at the `E` command; non-blank text after it is reported ("End command isn't at end of file."). Reaching end of file without `E` is reported ("no \"End\" statement."). At the end of reading: an open definition is finished with an error message; every cell referenced but never defined is reported ("cell NAME was used but not defined."); top-level geometry is processed into the edit cell with the per-cell layer processing; labels of the edit cell are adjusted and its bounding box recomputed; every read cell is marked modified and (unless `cif drccheck no`) marked for DRC re-check over its bounding box. CIF reading is not undoable.

Compatibility: exact

(Magic ref: cif/CIFrdutils.c:1394-1421, cif/CIFrdutils.c:1609-1704, cif/CIFrdcl.c:1734-1841)

#### Scenario: Missing End
- **GIVEN** a CIF file whose last command is `DF;`
- **WHEN** it is read
- **THEN** "no \"End\" statement." is reported and all definitions are still processed

### Requirement: Read diagnostics

Hefesto SHALL count errors and warnings separately per read and report them prefixed with "Error at line L of CIF file: " / "Warning at line L of CIF file: " (or "CIF file read error: "/"CIF file read warning: " when the line is 0). Under warning level `none` nothing is printed; under `limit` the first 99 of each kind are printed and the 100th prints "Error limit set:  Remaining errors will not be reported." (resp. "Warning limit set:  Remaining warnings will not be reported."); levels `default` and `align` print everything for CIF input. Level `redirect` depends on the `magic-compat` setting (Q-MASK-331, resolution: compat): with it ON (the default) CIF messages are printed to the console and the redirect file is used only by GDS input, as in Magic; with it OFF CIF read errors and warnings go to the redirect file, opened for writing at the start of each read as for GDS, and nothing is printed to the console when no file name was given. In both settings a message's prefix and body go to the same stream.

Compatibility: informative

(Magic ref: cif/CIFrdutils.c:132-182)

#### Scenario: Limit level
- **GIVEN** warning level `limit` and a file producing 150 errors
- **WHEN** the file is read
- **THEN** 99 error messages and one "Error limit set" message are printed

### Requirement: CIF output header

Hefesto SHALL begin every CIF output file with these comment lines, in order, each terminated by a newline:

```
( @@user : USER );
( @@machine : HOST );
( @@source : SOURCE );
( @@tool : Hefesto VERSION );
( @@compiled : COMPILETIME );
( @@technology : TECHNAME );
( @@version : TECHVERSION );
( @@techdesc : TECHDESC );
( @@style : OUTPUTSTYLE );
( @@date : DATE );
```

The lines, their order and format are the same in both `magic-compat` settings (Q-MASK-324, resolution: platform). `USER`/`HOST` come from the environment on desktop platforms (`?` when unset); on iPadOS they are `?` unless the app provides them. `SOURCE` is the root cell's file path (`?` when none). The `@@tool` value is `Hefesto <version>` and `COMPILETIME` is Hefesto's build time: Hefesto does not claim to be Magic. `TECHVERSION` is `unknown` when the technology declares none, the `@@techdesc` line is omitted when there is no description, and `DATE` is the local time in C `ctime` format without the trailing newline. Golden comparisons mask the six host-, build- or time-dependent values `@@user`, `@@machine`, `@@source`, `@@tool`, `@@compiled` and `@@date`, and run with `TZ=UTC`.

Compatibility: informative

(Magic ref: cif/CIFwrite.c:260-300)

#### Scenario: Header for a technology without description
- **GIVEN** technology `scmos`, version `2.0`, no description, output style `lambda=1.0(gen)`
- **WHEN** CIF is written
- **THEN** the lines `( @@technology : scmos );`, `( @@version : 2.0 );`, `( @@style : lambda=1.0(gen) );` appear and no `@@techdesc` line

### Requirement: CIF output symbol numbering and order

Hefesto SHALL number and order symbols as follows. The root cell is symbol 1. Starting with a stack containing only the root, repeatedly pop a cell; if it was already written skip it; otherwise, enumerate its child instances in *use enumeration order* and give each child cell not yet numbered the next number (2, 3, …) and push it; then write the popped cell's definition. Consequently parents are written before their children and the last-numbered pending cell is written first. A cell that cannot be read from disk is skipped. Before writing, the whole hierarchy below the root is loaded and timestamp mismatches are resolved; if a cell cannot be loaded the write fails with "Failure to read in entire subtree of the cell."

*Use enumeration order* is the order defined by capability `gds-write` requirement "Structure ordering" for the current `magic-compat` setting, used for both the numbering and stack order here and the order of `C` calls ("CIF output calls and arrays"); no separate CIF order is defined (Q-MASK-323, resolution: compat, following Q-MASK-204). With `magic-compat` ON the order is sampled where Magic samples it: children are numbered when their parent is processed, before the parent's own mask generation (CIFGenSubcells), and calls are written after it, so for a cell with 10 or more uses whose generation ran a non-surrounding search the two orders can differ, as in Magic. With `magic-compat` OFF both use the canonical reverse-insertion order.

Compatibility: exact

(Magic ref: cif/CIFwrite.c:121-232, cif/CIFwrite.c:234-258, cif/CIFwrite.c:309-356, cif/CIFwrite.c:333, cif/CIFwrite.c:467)

#### Scenario: Two-level hierarchy
- **GIVEN** root `top` with instances of `a` then `b` (in enumeration order), and `a` instancing `c`
- **WHEN** CIF is written
- **THEN** `top`=1, `a`=2, `b`=3 are numbered when `top` is written, then `b` (3) is written, then `a` (2) is written and numbers `c`=4, then `c` (4)
- **AND** the file ends with `C 1;` and `End`

### Requirement: CIF output symbol definition format

Hefesto SHALL write each symbol as:

```
DS <num> <R> <2*E>;
9 <name>;
L <layer>;
    B <w> <h> <cx> <cy>;
    P <x1> <y1> <x2> <y2> <x3> <y3>;
94 <text> <x> <y> [<layer>];
91 <id>[(<i>)|(<j>,<i>)];
C <num> [MX ]R <a> <b> T <tx> <ty>;
DF;
```

Details: the `9` line is omitted for an empty name, is `9 UNNAMED;` for the name `(UNNAMED)`, and is `9 PREFIX/NAME;` when a non-empty path prefix is set (`cif prefix`). Geometry is the cell's output CIF generated as in capability `mask-hierarchical-generation` over the cell bounding box grown by the style radius. For each non-temporary output layer in style order: if it is a label layer, each manhattan tile produces a label line with the layer name as text (see "CIF output labels"); otherwise the `L` line is written before the first tile only (no `L` line for empty layers), then each tile is written: a rectangle `[x1,x2]×[y1,y2]` as `    B (2*(x2−x1))/R (2*(y2−y1))/R (x1+x2)/R (y1+y2)/R;` and a triangle as `    P` followed by its three vertices `(2*x)/R (2*y)/R` in the vertex order given below, then `;`. All divisions are C integer division. Triangle vertex order for bounding box `[xb,xt]×[yb,yt]`: upper-left half: (xt,yt) (xb,yt) (xb,yb); lower-left half: (xb,yt) (xb,yb) (xt,yb); lower-right half: (xb,yb) (xt,yb) (xt,yt); upper-right half: (xt,yb) (xt,yt) (xb,yt). Tiles are the maximal-horizontal-strip decomposition enumerated in the standard tile-plane search order (see Q-MASK-336). Then label lines, then call lines, then `DF;`.

Compatibility: exact

(Magic ref: cif/CIFwrite.c:358-487, cif/CIFwrite.c:647-727, graphics/grClip.c:1159-1193)

#### Scenario: Box formatting
- **GIVEN** an output style with `R = 1`, `E = 1` and a CMF tile (0,0)-(40,20) in output units in symbol 1 named `inv`
- **WHEN** CIF is written
- **THEN** the symbol text contains `DS 1 1 2;`, `9 inv;`, `L CMF;` and `    B 80 40 40 20;`

#### Scenario: Triangle formatting
- **GIVEN** `R = 1` and a lower-right triangle with bounding box (0,0)-(10,10) on layer CMF
- **WHEN** CIF is written
- **THEN** the line `    P 0 0 20 0 20 20;` is produced

### Requirement: CIF output labels

Hefesto SHALL write each cell label (in the cell's label list order) as `94 TEXT CX CY;` or `94 TEXT CX CY LAYER;` where `CX = ((xb+xt)*S)/R`, `CY = ((yb+yt)*S)/R` from the label rectangle in internal units, and `LAYER` is the output layer assigned to the label's type by the output style (omitted when none). When area labels are enabled (`cif arealabels yes`), `95 TEXT SX SY CX CY [LAYER];` is written instead with `SX = ((xt−xb)*2*S)/R`, `SY = ((yt−yb)*2*S)/R`. A label-layer tile `[x1,x2]×[y1,y2]` (output units) is written as `94 LAYERNAME (x1+x2)/R (y1+y2)/R;` or, with area labels, `95 LAYERNAME (2*(x2−x1))/R (2*(y2−y1))/R (x1+x2)/R (y1+y2)/R;`. Label text depends on the `magic-compat` setting (Q-MASK-322, resolution: compat): with it ON (the default) it is written verbatim, as in Magic, and a warning names each label whose text contains whitespace or `;` (the file will not read back); with it OFF each space, tab, newline and `;` in the written text is replaced by `_`, with a warning in the GDS writer's form (`characters changed in string ...; modified string is ...`). The layout's own labels are never changed. Area labels are disabled by default (see Q-MASK-337).

Compatibility: exact

(Magic ref: cif/CIFwrite.c:420-480, cif/CIFwrite.c:587-645, cif/CIFwrite.c:52-55)

#### Scenario: Point label
- **GIVEN** `S = 100`, `R = 1`, and a label `out` at point (4,1) of type m1 whose output layer is CMF
- **WHEN** CIF is written
- **THEN** the line `94 out 800 200 CMF;` is produced

#### Scenario: Label text with a space
- **GIVEN** the same label with text `out 1`
- **WHEN** CIF is written
- **THEN** with `magic-compat` ON the line `94 out 1 800 200 CMF;` is produced with a warning
- **AND** with `magic-compat` OFF the line `94 out_1 800 200 CMF;` is produced with a warning

### Requirement: CIF output calls and arrays

Hefesto SHALL write each instance (in instance enumeration order) as one call per array element. For an instance with transform `(a b c; d e f)` (x' = a·x + b·y + c, y' = d·x + e·y + f), x index range `[xlo, xhi]`, y range `[ylo, yhi]`, separations `xsep`, `ysep`: for `ix = 0..|xhi−xlo|` (outer) and `iy = 0..|yhi−ylo|` (inner):

- if cell-id output is enabled (default) and the instance id is non-empty, write `91 ID` followed by `(Y,X)` when both ranges have more than one element, `(X)` when only x does, `(Y)` when only y does, then `;` — where `X` starts at `xlo` and steps by +1 if `xhi > xlo` else −1, and likewise `Y`;
- write `C NUM`, then ` MX R −a −d` if `a ≠ e` or (`a = 0` and `b = d`), else ` R a d`, then ` T TX TY;` with `TX = ((c + a·xsep·ix + b·ysep·iy)·2·S)/R` and `TY = ((f + d·xsep·ix + e·ysep·iy)·2·S)/R` (integer division).

The file ends with `C 1;` (the root number), newline, `End`, newline.

Compatibility: exact

(Magic ref: cif/CIFwrite.c:489-585, cif/CIFwrite.c:215-221)

#### Scenario: Identity instance
- **GIVEN** `S = 100`, `R = 1`, an instance `u1` of symbol 2 with identity transform translated by (3,0)
- **WHEN** CIF is written
- **THEN** the lines `91 u1;` and `C 2 R 1 0 T 600 0;` are produced

#### Scenario: Mirrored instance
- **GIVEN** an instance with transform `(−1 0 0; 0 1 0)` (mirror in x)
- **WHEN** CIF is written
- **THEN** the call contains ` MX R 1 0 T 0 0`

#### Scenario: 1x3 array
- **GIVEN** an instance `a` with x range 0..0, y range 0..2, `ysep = 5`, identity transform, `S = 1`, `R = 1`
- **WHEN** CIF is written
- **THEN** three calls are written with ids `a(0)`, `a(1)`, `a(2)` and translations `T 0 0`, `T 0 10`, `T 0 20`

### Requirement: CIF output preconditions and flat output

Hefesto SHALL refuse hierarchical CIF output when the output style's reducer is 0 ("The current CIF output style can only be used for writing Calma output.") and SHALL report "N problems occurred.  See feedback entries." when generation created feedback. Flat output SHALL be generated as specified in capability `mask-hierarchical-generation` requirement "Flat CIF output", using the symbol format of this capability (no reducer check, see Q-MASK-332), and SHALL end with `C 1;` and `End`. Its form depends on the `magic-compat` setting (Q-MASK-321 and Q-MASK-411, resolution: compat): with it ON (the default), as in Magic, one temporary cell holds the flattened paint only and is written as symbol 1 named `__CIF2__` (with any `cif prefix`), with no cell labels, and mask-hints and boundary layers come out empty; with it OFF symbol 1 is named after the root cell with the prefix and `UNNAMED` rules of hierarchical output, and the flat cell also holds the labels that `flatten` copies by default and the transformed `MASKHINTS_*` planes, written in the same `94`/`95` format.

Compatibility: exact

(Magic ref: cif/CIFwrite.c:121-160, cif/CIFwrite.c:729-792)

#### Scenario: Flat output structure
- **GIVEN** a two-level hierarchy
- **WHEN** flat CIF is written
- **THEN** the file contains exactly one `DS 1 ...` definition with `9 __CIF2__;` and no `C` line inside it
- **AND** with `magic-compat` OFF that definition has `9 <root name>;` and the flattened labels

### Requirement: cif command dispatch

Hefesto SHALL provide the capabilities of Magic's `cif` command. Option words are matched case-insensitively by unique prefix against: `help`, `arealabels`, `coverage`, `drccheck`, `idcell`, `istyle`, `lambda`, `limit`, `ostyle`, `paint`, `prefix`, `read`, `rescale`, `scale`, `see`, `statistics`, `warning`, `write`, `flat`, `polygon`, `unfracture`, plus the wizard options `*hier` and `*array` (capability `mask-hierarchical-generation`); an exact match wins over prefixes; an unknown option prints help. A first argument beginning with `list` (or `listall`) sets list (and all) mode for the following option. With no argument, CIF for the window's root cell is written to `ROOT.cif` where `ROOT` is the root cell name after its last `/`. Options other than `help`, `istyle`, `ostyle`, `prefix`, `arealabels`, `warning`, `limit`, `polygon` require a layout window.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:1752-1880)

#### Scenario: Bare cif command
- **GIVEN** a window whose root cell is `lib/inv`
- **WHEN** `cif` is invoked without arguments
- **THEN** hierarchical CIF is written to `inv.cif`

### Requirement: cif read, write and flat options

Hefesto SHALL implement `cif read FILE` (read CIF into the edit cell from `FILE.cif` searched along the cell search path), `cif write FILE` (hierarchical output of the window root to `FILE.cif` in the current directory) and `cif flat FILE` (flat output). Appending `.cif` depends on the `magic-compat` setting (Q-MASK-320, resolution: compat): with it ON (the default) `.cif` is always appended, as in Magic (`cif write chip.cif` writes `chip.cif.cif`); with it OFF it is appended only when the name does not already end in `.cif`. In both settings the GUI and the iPadOS document picker pass the picked file name without its extension. An unopenable file is reported ("Cannot open FILE.cif to read CIF." / "... to write CIF") and nothing else happens; an I/O error during writing reports "I/O error in writing file FILE." and "File may be incompletely written.".

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:2075-2087, commands/CmdCD.c:2226-2275, utils/path.c:686-710)

#### Scenario: Extension appended
- **WHEN** `cif write chip` is executed
- **THEN** the output file is `chip.cif`

### Requirement: cif style options

Hefesto SHALL implement `cif istyle [NAME]` and `cif ostyle [NAME]`: with a name, select the input (capability `mask-input-styles`) or output (capability `mask-output-styles`) style by unique prefix; without a name, report the current style; `listall` reports all style names; `list` returns the current name as a value.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:2005-2012, commands/CmdCD.c:2066-2072, cif/CIFrdtech.c:1260-1363)

#### Scenario: List all input styles
- **GIVEN** input styles `a` and `b`
- **WHEN** `cif listall istyle` is executed
- **THEN** the result lists `a` and `b`

### Requirement: cif scale and lambda options

Hefesto SHALL implement `cif scale in|out` (prefixes of `input`/`output`) returning microns per internal unit: input `10*S/(M*1000)`, output `10*S/(E*1000)` (1.0 when there is no output style). `cif lambda` returns the same value with a deprecation warning. In text mode the value is printed with format `One %s internal unit is %2.1f microns.`

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:1928-1952, cif/CIFmain.c:139-147)

#### Scenario: Output scale
- **GIVEN** an output style with `S = 10`, `E = 10`
- **WHEN** `cif scale out` is executed
- **THEN** the value 0.01 is returned

### Requirement: cif limit and rescale options

Hefesto SHALL implement `cif limit [N]`: without argument report the rescale limit (default 100); with an integer set it, clamped to 1..100 with a warning when clamped; a non-integer is a usage error. `cif rescale [yes|no]`: without argument report whether internal-grid subdivision during input is allowed (default yes); `no` disables subdivision (all off-grid input is snapped, capability `mask-input-styles`) and also sets the warning level to `limit`.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:1954-1980, commands/CmdCD.c:2089-2110, cif/CIFrdutils.c:80-88)

#### Scenario: Limit is clamped
- **WHEN** `cif limit 500` is executed
- **THEN** the limit becomes 100 and "Warning:  rescale limit set at maximum value of 100" is printed

### Requirement: cif output-format toggles

Hefesto SHALL implement `cif arealabels [yes|no]` (enable `95` area labels on output; default no), `cif idcell [yes|no]` (enable `91` instance ids on output; default yes) and `cif prefix [PATH]` (set, or clear when omitted, the path prefix written in `9` lines). The two toggles print their new state ("Output of CIF area labels is now enabled|disabled", "Output of CIF cell ID labels is now ...").

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:1982-2003, commands/CmdCD.c:2058-2064)

#### Scenario: Prefix in output
- **GIVEN** `cif prefix /lib` and a cell `inv`
- **WHEN** CIF is written
- **THEN** its definition contains `9 /lib/inv;`

### Requirement: cif drccheck option

Hefesto SHALL implement `cif drccheck [yes|no]`: without argument report the setting; `yes` (default) marks every cell read from CIF as needing a DRC check; `no` leaves read cells unmarked.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:2112-2130, cif/CIFrdcl.c:1325-1327)

#### Scenario: Disable DRC marking
- **GIVEN** `cif drccheck no`
- **WHEN** a CIF file is read
- **THEN** no DRC re-check is scheduled for the read cells

### Requirement: cif warning option

Hefesto SHALL implement `cif warning [default|none|align|limit|redirect [FILE]|help]`: without argument report the level; `help` or an unknown word lists valid options and leaves the level unchanged; `redirect FILE` sets the redirect file name (cleared when omitted). Level semantics are in "Read diagnostics"; `align` makes `cif see` warn about tiles whose lower-left corner is at an odd output coordinate.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:2196-2224, cif/CIFsee.c:225-245)

#### Scenario: Set none
- **WHEN** `cif warning none` is executed
- **THEN** subsequent CIF reads print no warnings or errors

### Requirement: cif polygon and unfracture options

Hefesto SHALL implement `cif polygon subcells [yes|no]` (stores a flag, default no; it has no effect on CIF reading, see Q-MASK-319) and `cif unfracture [yes|no]` (default no; when yes, each generated output layer has adjacent non-Manhattan tiles merged after generation, as specified in capability `mask-hierarchical-generation` requirement "Non-manhattan merge option"). For `polygon`, with only the sub-word `subcells` given, the current value is reported. For `unfracture`, the value is read according to the `magic-compat` setting (Q-MASK-413, resolution: compat; settles D-20): with it ON (the default), as in Magic, the value is read from the fourth word (`cif unfracture <any> yes|no`), `cif unfracture <any>` reports the current value, and Hefesto warns that the value was not applied when that third word is `yes` or `no`; with it OFF `cif unfracture yes|no` sets the switch, and the four-word form is still accepted. In both settings `cif unfracture` alone is an argument error.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:2014-2080, cif/CIFgen.c:5785-5786)

#### Scenario: Query polygon setting
- **WHEN** `cif polygon subcells` is executed with no value
- **THEN** the current setting (default no) is reported

#### Scenario: Third-word unfracture value
- **WHEN** `cif unfracture yes` is executed
- **THEN** with `magic-compat` ON the current value is reported unchanged, with a warning that `yes` was not applied
- **AND** with `magic-compat` OFF the switch is set to yes

### Requirement: cif see, paint, coverage and statistics options

Hefesto SHALL implement, for the window root cell and the cursor box: `cif see LAYERS` — flatten paint under the box grown by the style radius, generate the listed output layers (comma-separated, with their dependencies) and show each tile as feedback with text `CIF layer "LAYERS"`; `cif paint LAYERS TYPE [CELL]` — generate as for `see`, then paint every tile, converted to internal units by truncating division by `S` and skipping degenerate results, as Magic type `TYPE` into `CELL` (created if absent) or the root cell, followed by redisplay and DRC re-check of the box; `cif [list] coverage LAYER [box]` — generate over the cell bounding box (or the box), sum the areas of all generated tiles (a triangle counts as its full rectangle with `magic-compat` ON, as in Magic, and as half its rectangle with it OFF, summed as twice the area in 64-bit integers and halved at the end; Q-MASK-325, resolution: compat) and report `Cell|Cursor Box Area`, `Layer Bounding Area`, `Layer Total Area` (in CIF units², area scaled by `S²`) and `Coverage in cell|box = %1.1f%%`, or only the fraction with `list`; `cif statistics` — print and reset the counters of tile operations, hierarchy tile operations, rectangles output and hierarchical rectangles as `recent/total`.

Compatibility: behavioral

(Magic ref: cif/CIFsee.c:60-352, cif/CIFsee.c:425-530, cif/CIFmain.c:95-118, commands/CmdCD.c:2132-2194)

#### Scenario: Coverage of a half-filled box
- **GIVEN** `S = 1`, a cursor box (0,0)-(10,10) and generated layer CMF covering (0,0)-(10,5)
- **WHEN** `cif list coverage CMF box` is executed
- **THEN** the value 0.5 is returned
