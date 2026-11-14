# Display Style Files

## Purpose
Defines the file formats that give layers and user-interface elements their look: display-style files (`.dstyle`, all format versions), color-map files (`.cmap`), glyph files (`.glyphs`, cursor shapes) and the legacy `displays` file. It also defines how these files are found and which variant is chosen. Hefesto MUST read the files that PDKs and Magic installations ship without modification. How a style is rendered on screen is specified in `ui-windows-and-views`. The tech-file `styles` section grammar is owned by capability `tech-styles-section`. This capability only defines what a `styletype` line causes to be loaded and how style references resolve.

## Requirements

### Requirement: Style table model
The system SHALL keep one display-style table. Each entry SHALL hold:
- a write mask or opacity (integer)
- a color index into the color map (integer)
- an outline pattern (integer, 8 significant bits)
- a fill kind: one of `solid`, `cross`, `outline`, `stipple`, `grid`
- a stipple number (integer)
- a one-character short name (7-bit)
- an optional long name (at most 50 characters)
- an ordinal `num` (integer)

Table indices SHALL be assigned in file order, not from the `num` column:
- the lines of the `display_styles` section get indices 0, 1, 2, …
- the N lines of `layout_styles` get indices 52 … 52+N−1
- the N lines of `pale_styles` get indices 52+N … 52+2N−1

The pale style of the k-th layout style SHALL be the k-th pale style, so pale index = layout index + N. The value N SHALL be called the number of layout styles. The manual page dstyle(5) describes an obsolete numbering and SHALL NOT be followed (see Q-UI-39).

Compatibility: exact

(Magic ref: graphics/grDStyle.c:199-282, graphics/grDStyle.c:405-645, utils/styles.h:73-151, dbwind/DBWdisplay.c:115-119)

#### Scenario: Index assignment ignores the num column
- **GIVEN** a valid dstyle file with 52 display-style lines, then a `layout_styles` section whose first line has `num` 10 and whose second line has `num` 52, and a `pale_styles` section with the same number of lines (N)
- **WHEN** the file is loaded
- **THEN** the first layout line SHALL be table index 52 and the second SHALL be index 53
- **AND** the pale version of the first layout line SHALL be index 52+N

### Requirement: Fixed internal style slots
The first 52 display styles (indices 0–51) SHALL have these fixed meanings:

| Index | Use | Index | Use |
|---|---|---|---|
| 0 | transparent (no color) | 26 | toolkit light gray |
| 1 | solid highlights (the box) | 27 | pale (non-edit) labels |
| 2 | medium-stipple highlights | 28 | pale (non-edit) ports |
| 3 | pale-stipple highlights | 29 | white |
| 4 | horizontal-stripe highlights | 30 | gray 1 (pale) |
| 5 | vertical-stripe highlights | 31 | gray 2 (dark) |
| 6 | outline highlights | 32 | black |
| 7 | dotted-outline highlights | 33 | red 1 / pink |
| 8 | erase box | 34 | red 2 |
| 9 | erase highlights | 35 | red 3 |
| 10 | erase everything (background) | 36 | green 1 |
| 11 | erase all but tools | 37 | green 2 |
| 12 | labels | 38 | green 3 |
| 13 | ports | 39 | blue 1 |
| 14 | port connection lines | 40 | blue 2 |
| 15 | cell bounding boxes and names | 41 | blue 3 |
| 16 | grid | 42 | purple 1 |
| 17 | alternate (solid) grid | 43 | purple 2 |
| 18 | edit-cell origin square | 44 | yellow 1 (crosshair) |
| 19 | tile-structure debugging | 45 | yellow 2 |
| 20 | window border | 46 | orange 1 |
| 21 | scroll-bar elevator | 47 | orange 2 |
| 22 | window caption and scroll-bar background | 48 | brown 1 |
| 23 | window background | 49 | brown 2 |
| 24 | color-window "color being edited" (color overwritten at run time) | 50 | magenta |
| 25 | toolkit medium gray | 51 | cyan |

Index 52 SHALL be the first technology style.

Compatibility: exact
(Magic ref: utils/styles.h:73-151)

#### Scenario: Crosshair uses the yellow-1 slot
- **GIVEN** a dstyle file whose display-style line at index 44 has color 47
- **WHEN** the crosshair is drawn
- **THEN** it SHALL be drawn with style 44, so with color-map entry 47

### Requirement: Dstyle file name and lookup
For a technology `styletype` value T and display class D, the dstyle file name SHALL be `T.D.dstyle`. It SHALL be searched in the current directory first and then in the library path (see Requirement: Style file search path). D SHALL be:

| Display class | D |
|---|---|
| Alpha-blended GPU rendering (Magic's OpenGL and Cairo) | `OpenGL` |
| Indexed color, depth 7 or 8 bits | `7bit` |
| Indexed or true color, any other depth ≥ 2 | `24bit` |
| Monochrome (depth 0 or 1) | `bw` |

If `T.D.dstyle` cannot be opened, Magic tries `T.D.dstyle5`. In Magic that fallback never succeeds. Hefesto SHALL parse a `.dstyle5` file found this way as a version-5 file (see Q-UI-1). If neither name is found, the message SHALL be `Couldn't open display styles file "T.D.dstyle5"` and the load SHALL fail.

Hefesto's default class SHALL be `OpenGL`; the other classes SHALL be selectable in settings (decision X-5, see Q-UI-8 and capability `hefesto-rendering`).

Compatibility: exact (file names); behavioral (class choice)
(Magic ref: graphics/grDStyle.c:434-450, graphics/grTk1.c:633-654, graphics/grTOGL1.c:336-337, graphics/grTCairo1.c:295-296)
(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: OpenGL variant name
- **GIVEN** the technology line `styletype mos` and the OpenGL display class
- **WHEN** styles are loaded
- **THEN** the file `mos.OpenGL.dstyle` SHALL be opened

### Requirement: Dstyle file grammar
A dstyle file SHALL be read line by line, with each line read up to 199 characters. Lines SHALL be classified as follows:
- A line made only of whitespace is a blank line. It marks that the next non-comment line is a section header.
- A line whose first character is `#` is a comment and is ignored. A comment does NOT reset the "next line is a header" state.
- The first non-comment line after one or more blank lines is a section header. Its first word names the section.
- All other lines are data lines of the current section.

```ebnf
file          = { comment | blank } , section , { blank , { blank | comment } , section } ;
section       = header , { comment | data_line } ;
header        = ( "version" , ws , integer
                | "display_styles" , [ ws , word ]   (* trailing word, e.g. planes, ignored *)
                | "layout_styles"
                | "pale_styles"
                | "stipples" ) , rest_of_line ;
data_line     = style_line | stipple_line ;     (* chosen by current section *)
style_line    = num , ws , mask , ws , color , ws , outline , ws , fill , ws ,
                stipple_no , ws , short_name , [ ws , long_name ] , rest_of_line ;
num           = token ;                          (* <= 10 chars; integer or anything *)
mask          = vint ;
color         = vint | color_name ;              (* color_name <= 29 chars, from the cmap *)
outline       = vint ;
fill          = "solid" | "cross" | "outline" | "stipple" | "grid" ;  (* full word, case-insensitive *)
stipple_no    = decimal ;
short_name    = any_non_blank_char ;             (* conventionally "-" for none *)
long_name     = token ;                          (* <= 50 chars; only the first word is kept *)
stipple_line  = decimal , 8 * ( ws , row ) , rest_of_line ;
vint          = version <= 6 ? octal : c_integer ;   (* c_integer: decimal, 0x.. hex, 0.. octal *)
row           = version <= 6 ? octal : hex ;         (* hex digits without 0x *)
```

Section rules:
- `version` SHALL set the format version. If the file has no `version` section, the version SHALL be 5. If the number cannot be read, the version SHALL be 6 and the message `DStyle format version could not be read: assuming version 6` SHALL be printed.
- `display_styles` SHALL come before `layout_styles` and `pale_styles`. Otherwise the error `DStyle sections out of order: display_styles must come before layout_styles and pale_styles` SHALL be reported and the load SHALL fail.
- `layout_styles` after `pale_styles` is fatal in Magic: the process exits with status 1. Hefesto SHALL never exit because of a style file. It SHALL report a load error that rejects the file, whatever the `magic-compat` setting, and SHALL keep running (see Q-UI-2).
- `stipples` MAY appear anywhere and MAY be omitted.
- An integer-looking word in header position SHALL produce `Unexpected empty line in .dstyle file.` The line SHALL then be treated as a data line of the current section.
- Any other unknown header SHALL produce `Bad section name "X" in .dstyle file.` and the load SHALL fail (see Q-UI-3).
- Content before the first blank line, other than comments, has undefined meaning in Magic. Hefesto SHALL treat the first non-comment line of the file as a section header (see Q-UI-4).

Compatibility: exact
(Magic ref: graphics/grDStyle.c:405-586, graphics/grDStyle.c:199-282, utils/strdup.c:92-105, utils/lookupfull.c:52-75)

#### Scenario: Version 8 hexadecimal fields
- **GIVEN** a file with `version 8`, then a blank line, then `display_styles 24`
- **AND** the data line `6	127	64	0xff	outline	0	-	outline_highlights`
- **WHEN** it is parsed
- **THEN** that style SHALL have mask 127, color 64, outline 255, fill `outline`, stipple 0, short name `-` and long name `outline_highlights`

#### Scenario: Version 5 octal fields
- **GIVEN** a file without a `version` section
- **AND** the display-style line `1 100 100 377 solid 0 - x`
- **WHEN** it is parsed
- **THEN** mask SHALL be 64, color SHALL be 64 and outline SHALL be 255

#### Scenario: Leading zero in a version-7 color
- **GIVEN** a `version 7` file whose style line has color field `070`
- **WHEN** it is parsed
- **THEN** the color SHALL be 56, because the C-integer rules read a leading 0 as octal

#### Scenario: Long name keeps only first word
- **WHEN** the line `30 127 33 0x00 solid 0 - gray1 (pale)` is parsed
- **THEN** the long name SHALL be `gray1`

### Requirement: Dstyle style-line field semantics
Each style line SHALL be interpreted as follows:
- `mask` and `color` SHALL be ANDed with the display's valid-plane mask. On a GPU display the plane mask covers 24 bits, so values pass through unchanged.
- `color` SHALL first be parsed as an integer. If that fails, it SHALL be looked up by exact name among the color-map entry names. An unknown name gives color −1, which after masking becomes the all-ones plane mask (see Q-UI-5).
- If `num` is an integer string (optional sign, then digits), it SHALL become the style's ordinal. Otherwise the ordinal SHALL be 1 (see Q-UI-5). With `magic-compat` OFF, a non-integer `num` SHALL take the next ordinal in sequence, and an unknown color name SHALL make the line an error.
- `fill` SHALL be matched case-insensitively against the full words `solid`, `cross`, `outline`, `stipple`, `grid`. A non-matching fill SHALL make the line an error.
- `short_name` SHALL be stored as its 7-bit value.
- The long name SHALL be stored only if present. A style without a long name SHALL never match a lookup by name (Magic crashes on that lookup; see Q-UI-5). A line with fewer than 7 fields SHALL be an error.
- Any style-line error SHALL report `Style line contained format error: <line>` and the whole load SHALL fail.

The meaning of `mask` depends on the display class:
- Indexed displays (`7bit`, `bw`): `mask` is a bit-plane write mask. A drawn pixel becomes `(old & ~mask) | (color & mask)`, and the color map turns the pixel value into RGB.
- `OpenGL` class: `mask` is an opacity from 0 to 127. 127 means opaque. Any other value v means alpha = v/127 (see Q-UI-35).
- `24bit` class: see Q-UI-8.

Compatibility: exact (parsing); behavioral (meaning of mask on screen)
(Magic ref: graphics/grDStyle.c:199-282, graphics/grTOGL1.c:90-136, doc/man/dstyle.5)

#### Scenario: Color by name
- **GIVEN** a cmap entry `66 213 66 2 diff_green`
- **WHEN** a version-8 style line gives color `diff_green`
- **THEN** the style's color SHALL be 2

#### Scenario: Bad fill keyword
- **WHEN** a style line has fill `dots`
- **THEN** `Style line contained format error:` SHALL be reported and the dstyle load SHALL fail

### Requirement: Dstyle section count validation
At the `layout_styles` header, the number of display-style lines read so far SHALL be compared with 52:
- If it is smaller, report `Error: Display style file defines only K of 52 required internal styles.`
- If it is larger, report `Error: Display style file defines too many (K) internal styles; should be 52.`

In both cases Magic continues loading and the indices of all later styles shift by the difference. With `magic-compat` ON (the default), Hefesto SHALL do the same: report the error and continue loading, so that third-party PDK style files still load. With `magic-compat` OFF, Hefesto SHALL reject the file (see Q-UI-2).

At the end of the file:
- If any of `display_styles`, `layout_styles` or `pale_styles` is missing, the system SHALL report `Not all required style sections were read.  Missing sections are:` followed by the missing names, and the load SHALL fail.
- If the number of pale lines differs from the number of layout lines, the system SHALL report `Error:  Number of pale styles (P) is different from the number of layout styles (L)` and the load SHALL fail.

A failed load SHALL leave no style table.

Compatibility: exact
(Magic ref: graphics/grDStyle.c:497-520, graphics/grDStyle.c:588-645)

#### Scenario: Wrong display-style count with magic-compat ON
- **GIVEN** a dstyle file with 51 display-style lines and `magic-compat` ON
- **WHEN** it is loaded
- **THEN** `Error: Display style file defines only 51 of 52 required internal styles.` SHALL be reported and loading SHALL continue with the later indices shifted by one

#### Scenario: Pale count mismatch
- **GIVEN** a file with 120 layout lines and 119 pale lines
- **WHEN** it is loaded
- **THEN** the message `Error:  Number of pale styles (119) is different from the number of layout styles (120)` SHALL be produced and loading SHALL fail

### Requirement: Stipple patterns
Each stipples-section line SHALL define stipple pattern `n` as eight 8-bit rows. If there are not exactly nine numbers, or if `n < 0`, the line is a format error. Undefined stipple numbers below the highest defined number SHALL be all-zero patterns. A `stipples` header SHALL discard every previously defined stipple.

On screen, the pattern SHALL repeat every 8 pixels in x and y, anchored to the window pixel grid, and a pixel SHALL be painted only where its pattern bit is 1. Hefesto SHALL use the orientation of Magic's OpenGL/Cairo driver: row 0 is the bottom pixel row of each 8×8 cell, and the most significant bit is the leftmost pixel (decision X-5). Magic's X11/Tk drivers use the opposite orientation: row 0 is the top row and bit 0 (the least significant bit) is the leftmost pixel (informative; see Q-UI-6).

Compatibility: exact (parsing); behavioral (orientation)
(Magic ref: graphics/grDStyle.c:287-345, graphics/grTk1.c:261-298, graphics/grTOGL1.c:191-212)
(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: Checkerboard stipple
- **WHEN** the version-8 line `6  aa 55 aa 55 aa 55 aa 55	half 'n half` is read
- **THEN** stipple 6 SHALL have rows 0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55
- **AND** in Hefesto's (OpenGL) orientation, in the bottom pixel row of each 8×8 cell the leftmost pixel SHALL be painted and the pixel to its right SHALL be unpainted

### Requirement: Outline and fill rendering semantics
A style SHALL be rendered as follows:
- `solid`: fill every pixel.
- `stipple`: fill only the pixels selected by stipple `stipple_no`.
- `cross`: draw both diagonals of each rectangle (not of non-Manhattan triangles).
- `outline`: draw no fill.
- `grid`: draw the window grid using the `outline` value as the line pattern.

Outlines:
- An outline SHALL be drawn when `outline` ≠ 0 and fill ≠ `grid`.
- `outline` is an 8-bit line pattern applied along each edge, least significant bit first. 0xFF and 0x00 SHALL both draw a solid line.
- For layout paint, an outline SHALL be drawn only along the boundary between the style's material and different material. Internal boundaries between abutting tiles of the same type SHALL NOT be outlined.
- When a rectangle is smaller than 4 pixels in both dimensions on screen, its outline and its cross diagonals SHALL be skipped, unless the fill is `outline`.

Compatibility: behavioral
(Magic ref: graphics/grClip.c:574-776, graphics/grClip.c:777-986, graphics/grClip.c:987-1158, graphics/grTOGL1.c:153-175, doc/man/dstyle.5)

#### Scenario: Merged outline
- **GIVEN** a contact style with fill `cross` and outline 0xff, and two abutting contact rectangles (0,0)-(4,4) and (4,0)-(8,4) of the same type
- **WHEN** they are drawn at a scale above 4 pixels per unit
- **THEN** the shared edge x=4 SHALL NOT be outlined
- **AND** each rectangle SHALL show its own two diagonals

### Requirement: Color map file grammar
For tech style T, color-map class C and monitor type M (default `std`), the color map SHALL be read from `T.C.M.cmap`. If that name cannot be opened, `T.C.M.cmap1` SHALL be tried. If neither opens, the message `Couldn't open color map file "T.C.M.cmap"` SHALL be reported. C is `OpenGL`, `7bit` or `24bit`, matching the dstyle class. Monochrome displays SHALL load no color map.

```ebnf
cmap_file  = { cmap_line } ;
cmap_line  = comment | entry ;
comment    = "#" , rest_of_line ;           (* '#' must be in column 1 *)
entry      = int , ws , int , ws , int , ws , int , [ ws , name ] , newline ;
                                             (* red green blue maxIndex [name]; name = rest of line, <= 99 chars *)
```

Lines SHALL be read up to 127 characters. Magic reports blank lines as syntax errors. Hefesto SHALL skip them (see Q-UI-7).

Loading SHALL take two passes:
1. The color-map size is 1 + the maximum `maxIndex` over all entries. A line with no fourth integer that is not a comment SHALL report `Syntax error in color map file "F.cmap"` and `Last color read was index K`, and loading fails. The lines of this pass are the same 127-character pieces as in the second pass, so a longer line contributes the fourth integer of each piece. If the size would exceed 2^24 entries (a `maxIndex` of 16777216 or more), the system SHALL report `Color map file "F.cmap" has too many colors` and loading fails before any entry is allocated. Magic has no limit and allocates whatever the file declares.
2. The entries SHALL be applied in order. Each entry fills every index from the previous `maxIndex`+1 (0 for the first entry) up to its own `maxIndex`, with (red & 255, green & 255, blue & 255). If the line has a name, that name SHALL be stored on every index the entry fills.

If an entry's `maxIndex` is below the next index to fill, the system SHALL report `Colors in map are out of order!` and stop. A short file SHALL report `Premature end-of-file in color map file "F.cmap"`. The load SHALL fail if any index stays unfilled.

Compatibility: exact
(Magic ref: graphics/grCMap.c:105-232)

#### Scenario: Run filling
- **GIVEN** the cmap lines `255 0 0 2`, `0 0 255 3`, `255 255 255 255`
- **WHEN** it is loaded
- **THEN** the map SHALL have 256 entries
- **AND** indices 0–2 SHALL be red, index 3 SHALL be blue and indices 4–255 SHALL be white

#### Scenario: Named entry
- **WHEN** the line `220 95 95 1 poly_red` follows `200 200 200 0 background_gray`
- **THEN** index 1 SHALL be (220,95,95) with name `poly_red`

#### Scenario: Blank line skipped
- **GIVEN** a cmap file that contains an empty line
- **WHEN** it is loaded
- **THEN** the empty line SHALL be skipped and the color map SHALL be loaded, where Magic reports a syntax error

#### Scenario: Oversized color map refused
- **GIVEN** a cmap file whose only line is `0 0 0 16777216`
- **WHEN** it is loaded
- **THEN** `Color map file "F.cmap" has too many colors` SHALL be reported, no color map SHALL be allocated and loading SHALL fail

### Requirement: Color map file writing
Saving the color map SHALL write `T.C.M.cmap`, opened through the search path in write mode. For each maximal run of consecutive indices with identical RGB, the system SHALL write one line `"%d %d %d %d"` (red green blue last-index-of-run). If the first index of the run has a name, the line SHALL be followed by `" %s"` with that name, and then `"\n"`. Names on later indices of the run SHALL be dropped (see Q-UI-7). If the file cannot be opened, the message SHALL be `Couldn't write color map file "T.C.M.cmap"`.

Compatibility: exact
(Magic ref: graphics/grCMap.c:247-301)

#### Scenario: Save merges runs
- **GIVEN** a color map in which indices 4–255 are all (255,255,255) and index 4 has no name
- **WHEN** it is saved
- **THEN** a single line `255 255 255 255` SHALL be written for that run

### Requirement: Glyph file grammar
A glyph file SHALL be named `<G>.glyphs`, where G is `color` for color displays and `bw` for monochrome displays. It SHALL be searched in the current directory first and then in the library path. Lines SHALL be read up to 199 characters. Lines that are entirely whitespace, or whose first character is `#`, SHALL be ignored.

```ebnf
glyph_file = size_line , glyph , { glyph } ;
size_line  = "size" , ws , nGlyphs , ws , width , ws , height , rest ;
glyph      = height * pixel_row ;
pixel_row  = cell , { sep , cell } , [ rest ] ;      (* exactly width cells *)
cell       = style_char , ( " " | "*" ) ;              (* second char of the last cell may be absent *)
style_char = any char except whitespace ;
```

Row and pixel rules:
- The first row of each glyph SHALL be the top row.
- Pixel (x, y) SHALL use y = 0 for the bottom row.
- Each style character SHALL be mapped to the display style whose short name is that character. A character that no style uses as a short name SHALL map to style 0 (transparent). When several styles share a short name, the earliest style in the file SHALL win (see Q-UI-34).
- A `*` right after a style character SHALL mark that pixel as the glyph's hot spot (origin). The default origin SHALL be (0,0).

Error and end-of-file handling:
- A whitespace style character SHALL report `white space is not a valid glyph character`. A row with too few cells SHALL report `line is too short.` Both make the load fail.
- At end of file, the load SHALL succeed only if all glyphs are complete. Otherwise it SHALL report `Unexpected end of file in file 'F'`.
- Extra rows after the last glyph SHALL report `Extra lines at end of glyph file 'F'`, but the load SHALL still succeed.
- A first non-ignored line that is not a `size` line SHALL report `Format error in  file 'F'`.

Compatibility: exact
(Magic ref: graphics/grGlyphs.c:97-244, graphics/grDStyle.c:353-384, utils/strdup.c:92-105, doc/man/glyphs.5)

#### Scenario: Hot spot location
- **GIVEN** `size 1 3 3` followed by the rows `. K .`, `K K*K`, `. K .`
- **WHEN** it is loaded
- **THEN** glyph 0 SHALL be 3×3 with origin (1,1)
- **AND** its pixel (1,2) SHALL use the style whose short name is `K`

#### Scenario: Missing rows
- **GIVEN** `size 2 16 16` followed by only 16 rows
- **WHEN** it is loaded
- **THEN** `Unexpected end of file` SHALL be reported and the cursors SHALL NOT be replaced

### Requirement: Cursor glyph indices
The cursor glyph file SHALL provide glyphs at these fixed indices:

| Index | Cursor |
|---|---|
| 0 | normal |
| 1 | star |
| 2–5 | lower-left, lower-right, upper-left, upper-right corner |
| 6–9 | the same four corners in box form |
| 10–13 | whole-window move: LL, LR, UL, UR |
| 14–17 | window-corner resize: LL, LR, UL, UR |
| 18 | netlist tool |
| 19 | wiring arrow |
| 20 | interactive route |
| 21 | rsim tool |
| 22 | pick tool |

Magic's distributed `color.glyphs` and `bw.glyphs` contain 24 glyphs of 16×16. `DEFAULT.glyphs` contains 20.

Compatibility: exact (indices); behavioral (shapes, see ui-tools-and-input)
(Magic ref: utils/styles.h:160-200, graphics/color.glyphs:9, graphics/DEFAULT.glyphs:9)

#### Scenario: Wiring cursor
- **WHEN** the wiring tool becomes active
- **THEN** cursor glyph 19 SHALL be shown

### Requirement: Style loading triggered by styletype
When the tech file's `styles` section gives `styletype T [path1 … pathK]`, the system SHALL load, for family T, the color map `T.C.M.cmap` (see Requirement: Color map file grammar), the display-style file `T.D.dstyle` (see Requirement: Dstyle file name and lookup) and the cursor glyphs (see Requirement: Glyph file grammar), and SHALL then select cursor 0.

The order of these steps, the search locations, which failures reject the line or fail the technology load (including the headless case), and the binding of tile types to styles by number or long name with contact-style propagation are owned by capability `tech-styles-section` (requirements "styletype statement", "Type to style mapping" and "Contact style propagation"). This capability only defines the files and the style table that those references resolve into: an integer reference selects the first layout style whose `num` ordinal equals it.

Compatibility: behavioral
(Magic ref: dbwind/DBWdisplay.c:1688-1789)

#### Scenario: Numeric tech style reference
- **GIVEN** a layout style line whose `num` is 20, at file position 25
- **WHEN** the tech `styles` section binds `metal1 20`
- **THEN** metal1 SHALL be drawn with table index 52+24

### Requirement: Style file search path
A file name SHALL be resolved as follows:
1. A name starting with `~` (home or `~user`) or `$VAR`/`${VAR}` SHALL be expanded and opened directly. `$CAD_ROOT` falls back to the built-in installation root when the variable is unset. Any other unset variable is left unexpanded.
2. A name starting with `/` SHALL be opened directly.
3. A name starting with `./` or `../` SHALL be tried directly first. If that fails, it SHALL also be tried through the path.
4. Otherwise each path entry SHALL be tried in order, then each library entry. Entries are separated by whitespace or `:`. A leading `./` in an entry, and an entry that is just `.`, both mean the current directory.

The search SHALL stop at the first success, and also at the first error other than "file not found". The default system library path SHALL be `. $CAD_ROOT/magic/sys $CAD_ROOT/magic/sys/current`.

Compatibility: behavioral (host mechanics [platform], see Q-UI-26)
(Magic ref: utils/path.c:193-388, utils/path.c:389-445, utils/path.c:647-830, utils/paths.h:40)

#### Scenario: Local override
- **GIVEN** `mos.OpenGL.dstyle` exists both in the current directory and in `$CAD_ROOT/magic/sys`
- **WHEN** styles are loaded
- **THEN** the copy in the current directory SHALL be used

### Requirement: Legacy displays file
The legacy `displays` file SHALL be optional for Hefesto. At startup, Magic reads a `displays` file from `. $CAD_ROOT/`. Each line holds whitespace-separated fields `tty graphicsPort monitorType displayType [tabletPort]`. The first line whose tty equals the terminal on standard input sets the graphics port, monitor type, display type and tablet port. The tablet port defaults to the graphics port. Reading SHALL stop at the first line with fewer than 4 fields. Hefesto MAY ignore this file because it has no serial graphics terminals.

Compatibility: informative
(Magic ref: utils/finddisp.c:74-123, utils/main.c:534-535, doc/man/displays.5)

#### Scenario: Matching tty
- **GIVEN** the line `/dev/ttyp1 /dev/null std NULL`, with standard input on `/dev/ttyp1`
- **WHEN** Magic starts
- **THEN** the display type SHALL be `NULL` and the monitor type SHALL be `std`
