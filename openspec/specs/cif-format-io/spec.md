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

Hefesto SHALL name the cell for symbol `n` with the decimal string of `n` (for example `"12"`), creating it on first reference by `DS` or `C`. On `DS n` when symbol `n` already maps to an available (already defined) cell whose name is still `"n"`, that cell is renamed `"n_k"` with the smallest `k >= 1` not in use, the warning "cell definition n reused." is issued, and a new cell `"n"` is created for the new definition. The definition always clears the target cell before reading. `DD n` forgets the mapping of `n`; a later `DS n` or `C n` maps again to a cell found by the name `"n"` if one exists (see Q-MASK-318). The user extension `9 NAME` renames the current cell (or, outside any definition, the edit cell, see Q-MASK-317) to `NAME`; if `NAME` already exists, the error "NAME already exists, so cell from CIF is named OLD." is reported and the name is unchanged.

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:199-290, cif/CIFrdcl.c:1218-1330)

#### Scenario: Redefined symbol keeps the old cell
- **GIVEN** a file with `DS 5; ... DF;` twice, with no `9` command
- **WHEN** the file is read
- **THEN** the first definition becomes cell `5_1` and the second cell `5`
- **AND** calls to symbol 5 read before the second `DS 5` refer to `5_1`

### Requirement: Calls and transformations

Hefesto SHALL process `C n transforms` by creating an instance of the cell for symbol `n` in the current cell (or the edit cell at top level), with transform `T` built as follows: start from identity; for each transformation in order, compose so that it is applied after the previous ones: `T p` translates by `p` (a point at scale 1, see "Point scaling"); `M X` mirrors x (x → −x); `M Y` mirrors y (y → −y); `R a b` rotates the x axis onto the direction `(a,b)`: `(+,0)` identity, `(−,0)` 180°, `(0,+)` 90° counter-clockwise, `(0,−)` 90° clockwise; any other direction is reported "non-manhattan direction vector (a, b); ignored." and treated as identity. The final translation components are converted from read units to internal units with exact conversion (capability `mask-input-styles`). The instance gets the pending `91` id if any (then cleared), otherwise a default id. Placing a cell inside itself or its descendants is reported ("attempt to place cell use inside its own definition!") and skipped. A malformed transformation is reported and the call is still made with the transform parsed so far, unscaled (see Q-MASK-335).

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

Hefesto SHALL process `B L W cx cy [dx dy]` as follows: `(L, W)` is scaled as a point with `iscale = 1`; `L <= 0` or `W <= 0` is reported and the box ignored. `(cx, cy)` is scaled with `iscale = 2` (giving `2c`); if this subdivides read units, `L` and `W` are multiplied by the same factor. The rectangle `[−L, L] × [−W, W]` is rotated by the direction `(dx, dy)` if present (same mapping as the `R` transformation), then each coordinate is computed as `(r + 2c) / 2` with C integer division (truncation toward zero), and the resulting rectangle is added to the current layer. See Q-MASK-315 for odd lengths.

Compatibility: exact

(Magic ref: cif/CIFrdpt.c:68-177)

#### Scenario: Even box
- **GIVEN** scale 1/1 and `B 40 20 20 10;`
- **WHEN** it is read
- **THEN** the rectangle (0,0)-(40,20) read units is added

#### Scenario: Odd length centered on an integer
- **GIVEN** scale 1/1 and `B 3 2 10 0;`
- **WHEN** it is read
- **THEN** the x extent is `(−3+20)/2 = 8` to `(3+20)/2 = 11`, giving (8,−1)-(11,1)

#### Scenario: Rotated box
- **GIVEN** scale 1/1 and `B 40 20 0 0 0 1;`
- **WHEN** it is read
- **THEN** the rectangle (−10,−20)-(10,20) is added

### Requirement: Round flash command

Hefesto SHALL process `R d cx cy` by computing `d' = d*N / Dn` (truncating; "Roundflash diameter snapped to nearest integer boundary." when inexact), scaling the center with `iscale = 2` (and multiplying `d'` by any subdivision factor), and adding the axis-aligned square `[(2cx − d')/2, (2cx + d')/2] × [(2cy − d')/2, (2cy + d')/2]` with truncating division. The flash is not approximated by a polygon (see Q-MASK-316).

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

Hefesto SHALL process `W w p1 p2 ...` by computing `w' = w*N / Dn` (truncating, warning "Wire width snapped to nearest integer boundary." when inexact), scaling the path with `iscale = 2` (doubled coordinates `P_i`; `w'` multiplied by any subdivision factor), and converting the path to polygons with square end extensions (see Q-MASK-333) as follows (all in doubled coordinates, double-precision arithmetic, `round` = round half away from zero, results divided by 2):

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

Hefesto SHALL process `94 NAME x y [LAYER]` as a point label: the point is scaled with `iscale = 1` and converted to internal units with "any" (snapping) conversion; the label rectangle is that point. `95 NAME L W x y [LAYER]` is an area label: `(L, W)` is scaled with `iscale = 1`, the center with `iscale = 2`; `ll = 2c − (L,W)`, `ur = 2c + (L,W)`; if both components of `ll` are even, `ll` and `ur` are halved, otherwise the input style is rescaled by 2 (without changing the symbol scale, see Q-MASK-314); each corner is then converted with "any" conversion. In both cases, if `LAYER` is given and is a CIF layer of the style, the label type is that layer's label type; if `LAYER` is unknown "label attached to unknown layer NAME." is reported and the label goes on space; if absent, the type is the current default label type (94) or space (95). The label is sticky when the layer's label kind is not NONE (see Q-MASK-327). Labels whose type is "discard" are dropped. Label names are read up to the first whitespace or `;` (see Q-MASK-313).

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

Hefesto SHALL count errors and warnings separately per read and report them prefixed with "Error at line L of CIF file: " / "Warning at line L of CIF file: " (or "CIF file read error: "/"CIF file read warning: " when the line is 0). Under warning level `none` nothing is printed; under `limit` the first 99 of each kind are printed and the 100th prints "Error limit set:  Remaining errors will not be reported." (resp. "Warning limit set:  Remaining warnings will not be reported."); levels `default`, `align` and `redirect` print everything for CIF input (redirection applies only to GDS input, see Q-MASK-331).

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
( @@tool : Magic VERSION.REVISION );
( @@compiled : COMPILETIME );
( @@technology : TECHNAME );
( @@version : TECHVERSION );
( @@techdesc : TECHDESC );
( @@style : OUTPUTSTYLE );
( @@date : DATE );
```

`USER`/`HOST` come from the environment (`?` when unset), `SOURCE` is the root cell's file path (`?` when none), `TECHVERSION` is `unknown` when the technology declares none, the `@@techdesc` line is omitted when there is no description, and `DATE` is the local time in C `ctime` format without the trailing newline. The `@@user`, `@@machine`, `@@source`, `@@tool`, `@@compiled` and `@@date` values are host/time dependent (see Q-MASK-324).

Compatibility: informative

(Magic ref: cif/CIFwrite.c:260-300)

#### Scenario: Header for a technology without description
- **GIVEN** technology `scmos`, version `2.0`, no description, output style `lambda=1.0(gen)`
- **WHEN** CIF is written
- **THEN** the lines `( @@technology : scmos );`, `( @@version : 2.0 );`, `( @@style : lambda=1.0(gen) );` appear and no `@@techdesc` line

### Requirement: CIF output symbol numbering and order

Hefesto SHALL number and order symbols as follows. The root cell is symbol 1. Starting with a stack containing only the root, repeatedly pop a cell; if it was already written skip it; otherwise, enumerate its child instances in database enumeration order and give each child cell not yet numbered the next number (2, 3, …) and push it; then write the popped cell's definition. Consequently parents are written before their children and the last-numbered pending cell is written first. A cell that cannot be read from disk is skipped. Before writing, the whole hierarchy below the root is loaded and timestamp mismatches are resolved; if a cell cannot be loaded the write fails with "Failure to read in entire subtree of the cell." The order of child enumeration must be made deterministic (see Q-MASK-323).

Compatibility: exact

(Magic ref: cif/CIFwrite.c:121-232, cif/CIFwrite.c:234-258, cif/CIFwrite.c:309-356)

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

Hefesto SHALL write each cell label (in the cell's label list order) as `94 TEXT CX CY;` or `94 TEXT CX CY LAYER;` where `CX = ((xb+xt)*S)/R`, `CY = ((yb+yt)*S)/R` from the label rectangle in internal units, and `LAYER` is the output layer assigned to the label's type by the output style (omitted when none). When area labels are enabled (`cif arealabels yes`), `95 TEXT SX SY CX CY [LAYER];` is written instead with `SX = ((xt−xb)*2*S)/R`, `SY = ((yt−yb)*2*S)/R`. A label-layer tile `[x1,x2]×[y1,y2]` (output units) is written as `94 LAYERNAME (x1+x2)/R (y1+y2)/R;` or, with area labels, `95 LAYERNAME (2*(x2−x1))/R (2*(y2−y1))/R (x1+x2)/R (y1+y2)/R;`. Label text is written verbatim (see Q-MASK-322). Area labels are disabled by default (see Q-MASK-337).

Compatibility: exact

(Magic ref: cif/CIFwrite.c:420-480, cif/CIFwrite.c:587-645, cif/CIFwrite.c:52-55)

#### Scenario: Point label
- **GIVEN** `S = 100`, `R = 1`, and a label `out` at point (4,1) of type m1 whose output layer is CMF
- **WHEN** CIF is written
- **THEN** the line `94 out 800 200 CMF;` is produced

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

Hefesto SHALL refuse hierarchical CIF output when the output style's reducer is 0 ("The current CIF output style can only be used for writing Calma output.") and SHALL report "N problems occurred.  See feedback entries." when generation created feedback. Flat output SHALL be generated as specified in capability `mask-hierarchical-generation` requirement "Flat CIF output" (one temporary cell holding the flattened paint, no labels, written as symbol 1 named `__CIF2__`), using the symbol format of this capability (no reducer check, see Q-MASK-332), and SHALL end with `C 1;` and `End` (see Q-MASK-321).

Compatibility: exact

(Magic ref: cif/CIFwrite.c:121-160, cif/CIFwrite.c:729-792)

#### Scenario: Flat output structure
- **GIVEN** a two-level hierarchy
- **WHEN** flat CIF is written
- **THEN** the file contains exactly one `DS 1 ...` definition with `9 __CIF2__;` and no `C` line inside it

### Requirement: cif command dispatch

Hefesto SHALL provide the capabilities of Magic's `cif` command. Option words are matched case-insensitively by unique prefix against: `help`, `arealabels`, `coverage`, `drccheck`, `idcell`, `istyle`, `lambda`, `limit`, `ostyle`, `paint`, `prefix`, `read`, `rescale`, `scale`, `see`, `statistics`, `warning`, `write`, `flat`, `polygon`, `unfracture`, plus the wizard options `*hier` and `*array` (capability `mask-hierarchical-generation`); an exact match wins over prefixes; an unknown option prints help. A first argument beginning with `list` (or `listall`) sets list (and all) mode for the following option. With no argument, CIF for the window's root cell is written to `ROOT.cif` where `ROOT` is the root cell name after its last `/`. Options other than `help`, `istyle`, `ostyle`, `prefix`, `arealabels`, `warning`, `limit`, `polygon` require a layout window.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:1752-1880)

#### Scenario: Bare cif command
- **GIVEN** a window whose root cell is `lib/inv`
- **WHEN** `cif` is invoked without arguments
- **THEN** hierarchical CIF is written to `inv.cif`

### Requirement: cif read, write and flat options

Hefesto SHALL implement `cif read FILE` (read CIF into the edit cell from `FILE.cif` searched along the cell search path), `cif write FILE` (hierarchical output of the window root to `FILE.cif` in the current directory) and `cif flat FILE` (flat output). Magic always appends `.cif` to the given name (see Q-MASK-320). An unopenable file is reported ("Cannot open FILE.cif to read CIF." / "... to write CIF") and nothing else happens; an I/O error during writing reports "I/O error in writing file FILE." and "File may be incompletely written.".

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

Hefesto SHALL implement `cif polygon subcells [yes|no]` (stores a flag, default no; it has no effect on CIF reading, see Q-MASK-319) and `cif unfracture [yes|no]` (default no; when yes, each generated output layer has adjacent non-Manhattan tiles merged after generation, as specified in capability `mask-hierarchical-generation` requirement "Non-manhattan merge option"). For `polygon`, with only the sub-word `subcells` given, the current value is reported. For `unfracture`, Magic reads the value from the fourth word (`cif unfracture <any> yes|no`), `cif unfracture <any>` reports the current value, and `cif unfracture` alone is an argument error (see Q-MASK-413).

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:2014-2080, cif/CIFgen.c:5785-5786)

#### Scenario: Query polygon setting
- **WHEN** `cif polygon subcells` is executed with no value
- **THEN** the current setting (default no) is reported

### Requirement: cif see, paint, coverage and statistics options

Hefesto SHALL implement, for the window root cell and the cursor box: `cif see LAYERS` — flatten paint under the box grown by the style radius, generate the listed output layers (comma-separated, with their dependencies) and show each tile as feedback with text `CIF layer "LAYERS"`; `cif paint LAYERS TYPE [CELL]` — generate as for `see`, then paint every tile, converted to internal units by truncating division by `S` and skipping degenerate results, as Magic type `TYPE` into `CELL` (created if absent) or the root cell, followed by redisplay and DRC re-check of the box; `cif [list] coverage LAYER [box]` — generate over the cell bounding box (or the box), sum the areas of all generated tiles (triangles counted as full rectangles, see Q-MASK-325) and report `Cell|Cursor Box Area`, `Layer Bounding Area`, `Layer Total Area` (in CIF units², area scaled by `S²`) and `Coverage in cell|box = %1.1f%%`, or only the fraction with `list`; `cif statistics` — print and reset the counters of tile operations, hierarchy tile operations, rectangles output and hierarchical rectangles as `recent/total`.

Compatibility: behavioral

(Magic ref: cif/CIFsee.c:60-352, cif/CIFsee.c:425-530, cif/CIFmain.c:95-118, commands/CmdCD.c:2132-2194)

#### Scenario: Coverage of a half-filled box
- **GIVEN** `S = 1`, a cursor box (0,0)-(10,10) and generated layer CMF covering (0,0)-(10,5)
- **WHEN** `cif list coverage CMF box` is executed
- **THEN** the value 0.5 is returned
