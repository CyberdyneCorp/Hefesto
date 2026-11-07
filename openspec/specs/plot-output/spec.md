# Plot Output

## Purpose

Defines hard-copy/plot output of layout: the `plot` command and its subcommands (`postscript`, `pnm`, `svg`, `versatec`, `parameters`, `help`, and the optional `gremlin`/`pixels`), the plot parameters, the technology-file `plot` section with its per-device styles, and the exact content of the generated PostScript (encapsulated PostScript using the Magic prologue) and PNM (binary P6) files. Printer spooling and obsolete raster devices are documented informatively.

Related capabilities (referenced, not duplicated): `tech-file-format` (section syntax, type-list syntax), `display-style-files` (display style table, colour map and `.dstyle` files), `cmd-paint-erase` (layer-list arguments including `labels`/`subcell`, unique-prefix keyword matching), `ui-windows-and-views` (layout window, box tool, expansion state), `layout-geometry-model` (tile planes, canonical tile decomposition and hierarchical enumeration order), `router-channel` (wire widths used for label offsets).

## Requirements

### Requirement: Plot command and plotted region

The system SHALL provide `plot <option> ...` where `<option>` is matched by unique prefix against `postscript`, `pnm`, `svg`, `versatec`, `parameters` and `help` (informative: Magic offers `svg` only when an SVG-capable display is built in, and `gremlin` and `pixels` only in builds that include those devices); no option prints the help list ("The "plot" commands are:" followed by one line per option); an unknown option reports ""<x>" isn't a valid plot option." and prints the help. For every output option the plotted window is the layout window under the pointer or the only layout window ("The cursor must be over a layout window to plot." otherwise); the plotted area is the box if the box lies in that window's root cell, else the root cell's bounding box; the default layer set is the window's visible layers, plus labels when the window displays labels, plus subcell outlines; an explicit layer-list argument replaces that set (special names `labels` and `subcell` select labels and unexpanded-subcell outlines). Hierarchy is plotted according to the window's expansion state: paint and labels of expanded cells, bounding boxes of unexpanded subcells.

Compatibility: behavioral

```ebnf
plot_cmd   = "plot" , [ ws , ( ps_cmd | pnm_cmd | svg_cmd | vers_cmd | param_cmd | "help" ) ] ;
ps_cmd     = "postscript" , ws , file , [ ws , layers ] ;
pnm_cmd    = "pnm" , ws , file , [ ws , width_px , [ ws , layers ] ] ;
svg_cmd    = "svg" , ws , file ;
vers_cmd   = "versatec" , [ ws , scale , [ ws , layers ] ] ;
param_cmd  = "parameters" , [ ws , name , ws , value ] ;
```

(Magic ref: plot/plotCmd.c:50-362, commands/CmdSubrs.c:486-590)

#### Scenario: Box outside the root cell
- **GIVEN** a layout window showing cell TOP whose bounding box is (0,0)-(200,100), and the box tool in a different window's cell
- **WHEN** the user runs `plot postscript out.ps`
- **THEN** the area (0,0)-(200,100) of TOP is plotted

#### Scenario: Invalid option
- **WHEN** the user runs `plot foo`
- **THEN** ""foo" isn't a valid plot option." is reported followed by the option list

### Requirement: Plot parameters

`plot parameters` SHALL print all parameters, and `plot parameters <name> <value>` SHALL set one, with `<name>` matched by unique prefix (case-sensitive) against the table below; an unknown name reports ""<name>" isn't a valid plot parameter." and prints all parameters; a value violating the constraint reports the message shown and leaves the parameter unchanged.

Compatibility: behavioral

| Name | Default | Constraint / meaning |
|---|---|---|
| `showcellnames` | true | `true`/`false` (prefix); draw cell name and instance id inside unexpanded-cell outlines |
| `ps_cellidfont` | `/Helvetica` | PostScript font name for instance ids |
| `ps_namefont` | `/HelveticaBold` | font for cell names |
| `ps_labelfont` | `/Helvetica` | font for labels |
| `ps_cellidsize` | 8 | integer > 0, points |
| `ps_namesize` | 12 | integer > 0, points |
| `ps_labelsize` | 12 | integer > 0, points |
| `ps_boundary` | true | draw outlines around each style's material |
| `ps_width` | 612 | integer > 0, page width in points (8.5 in) |
| `ps_height` | 792 | integer > 0, page height in points (11 in) |
| `ps_margin` | 72 | integer ≥ 0, points |
| `pnmmaxmem` | 65536 | integer > 0, KB of raster memory |
| `pnmdownsample` | 0 | integer ≥ 0, base-2 log of the downsampling factor |
| `pnmbackground` | 255 | integer 0..255, background grey level |
| `cellidfont`, `cellnamefont`, `labelfont` | `vfont.I.12`, `vfont.B.12`, `vfont.R.8` | raster font files (versatec) |
| `directory` | `/tmp` | temporary directory (versatec, RTL) |
| `dotsperinch` | 300 | integer > 0 |
| `printer` | `versatec` | printer name passed to the spool command |
| `spoolcommand` | `lp -d %s %s` | printf-style command: printer, file |
| `swathheight` | 64 | integer > 0 |
| `width` | 2400 | integer > 0, device dots across |
| `plottype` | `hprtl` | one of `versatec_color`, `versatec_bw`, `hprtl`, `hpgl2`; also sets dotsperinch/width to 215/7904, 215/7904, 316/2400, 300/10650 |
| `pnmplotRTL` | false | send PNM output to an HP RTL/HPGL2 plotter instead of a file |

The printed listing (informative) groups parameters under "General plotting parameters are:", "Postscript plotting parameters are:", "PNM plotting parameters are:" and "HP/Versatec plotting parameters are:", e.g. `    PS_width:       612 (8.500 in)`.

(Magic ref: plot/plotMain.c:300-650, plot/plotPS.c:120-140, plot/plotPNM.c:60-75, plot/plotVers.c:95-140)

#### Scenario: Reject zero label size
- **WHEN** the user runs `plot parameters ps_labelsize 0`
- **THEN** "PS_labelSize must be an integer greater than zero." is reported and the size stays 12

#### Scenario: Plot type sets resolution
- **WHEN** the user runs `plot parameters plottype hpgl2`
- **THEN** dotsperinch becomes 300 and width becomes 10650

### Requirement: Technology plot section structure

The technology `plot` section SHALL consist of `style <name>` lines, where `<name>` is exactly one of `postscript`, `pnm`, `gremlin`, `versatec`, `colorversatec`, `pixels`, each followed by lines interpreted by that style's grammar. A `style` line with other than two fields reports ""style" lines must have exactly two arguments"; an unknown style name reports "Plot style "<name>" doesn't exist.  Ignoring." and the following lines up to the next `style` are skipped; a non-style line before any `style` reports "Must declare a plot style before anything else." and lines are skipped until the next `style`. The section is optional; when absent, PNM plotting uses the display-derived defaults (with the warning "Warning:  No plot section in techfile, using defaults." at plot time) and PostScript output contains no paint styles. Styles of all kinds are reset at every technology load.

Compatibility: exact

```ebnf
plot_section = "plot" , nl , { style_block } , "end" , nl ;
style_block  = "style" , ws , style_name , nl , { style_line , nl } ;
style_name   = "postscript" | "pnm" | "gremlin" | "versatec" | "colorversatec" | "pixels" ;
```

(Magic ref: plot/plotMain.c:40-200)

#### Scenario: Unknown style skipped
- **GIVEN** a plot section `style laser` followed by `m1 1 S` and then `style postscript` with `m1 1 S`
- **WHEN** the technology is loaded
- **THEN** one error is reported for `laser`, and only the postscript style receives the m1 line

### Requirement: PostScript style lines

Within `style postscript` each line SHALL have 9, 5 or 3 fields ("\"ps\" lines must have either 9, 5, or 3 arguments." otherwise):

- 9 fields — stipple pattern: `<index> <row0> … <row7>`, index a decimal integer, each row parsed as up to 8 hexadecimal digits (32 bits).
- 5 fields — colour: `<index> <c> <m> <y> <k>`, decimal integers each reduced modulo 256 (low 8 bits), CMYK.
- 3 fields — layer style: `<types> <color-index> <fill>`, where `<color-index>` must be an integer ("2nd field must be an integer") and `<fill>` is `S` (solid), `X` (outline with cross), `B` (outline only) or an integer pattern index ("3rd field must be an integer or "S", "X", or "B"."). The type list is expanded so that naming a type also covers the types in its layer-composition set, then restricted to user-visible types.

Patterns, colours and layer styles are each kept in reverse order of appearance (the last line comes first) and are emitted in that order.

Compatibility: exact

```ebnf
ps_line     = ps_pattern | ps_color | ps_layer ;
ps_pattern  = integer , 8 * ( ws , hex8 ) ;
ps_color    = integer , 4 * ( ws , integer ) ;
ps_layer    = types , ws , integer , ws , ( "S" | "X" | "B" | integer ) ;
hex8        = hexdigit , { hexdigit } ;   (* at most 8 significant digits *)
```

Example:

```
style postscript
 5 C3C3C3C3 3C3C3C3C C3C3C3C3 3C3C3C3C C3C3C3C3 3C3C3C3C C3C3C3C3 3C3C3C3C
 10 0 0 0 255
 12 200 0 50 0
 poly 12 5
 m1 10 S
 pc,m1c 10 X
```

(Magic ref: plot/plotPS.c:160-290)

#### Scenario: Reverse order of styles
- **GIVEN** the example above
- **WHEN** a PostScript plot is produced
- **THEN** the colour definitions appear as `/col12 ...` then `/col10 ...`, and the layer styles are processed in the order `pc,m1c`, `m1`, `poly`

### Requirement: PostScript file layout

`plot postscript <file>` SHALL write `<file>` (opened relative to the current directory, error "Couldn't write PS file "<file>"." on failure) with the following content, where (x0,y0)-(x1,y1) is the plotted area, X = x1−x0, Y = y1−y0, W, H, M are `ps_width`, `ps_height`, `ps_margin`, and all floats are single precision:

1. Scale: s = min((W − 2M)/X, (H − 2M)/Y). If labels are plotted, the label margin values n_x, n_y, p_x, p_y (requirement "PostScript label margins") are computed and s = min((W − 2M − n_x − p_x)/X, (H − 2M − n_y − p_y)/Y); otherwise all margins are 0. Then tw = trunc(X·s) + n_x + p_x and th = trunc(Y·s) + n_y + p_y.
2. Header lines: `%!PS-Adobe-3.0 EPSF-3.0`; `%%BoundingBox: M M tw+M th+M`; `%%DocumentNeededResources: font <idfont>` followed by ` font <namefont>` unless it equals the id font and ` font <labelfont>` unless it equals either; `%%EndComments`.
3. The prologue file `magicps.pro` copied verbatim (searched in the current directory then the system library directory); if missing, the line `postscript_prolog_is_missing` surrounded by blank lines.
4. Font procedures `/f1 { %.3f <labelfont> sf } def`, `/f2 { %.3f <namefont> sf } def`, `/f3 { %.3f <idfont> sf } def` with sizes labelsize/s, namesize/s, idsize/s.
5. For each colour: `/col<i> {%.3f %.3f %.3f %.3f sc} bind def` with each component /255.0.
6. For each pattern: `{<` then each row printed twice as `%08lx%08lx` (lowercase hex), then `>} <i> dp`.
7. `%%EndResource`, `%%EndProlog`, blank line, `%%Page: 1 1`, `/pgsave save def bop`, `% 0 0 offsets`, `ninit`, `<M+n_x> <M+n_y> translate`, `%.3f %.3f scale` (s, s), `minit`, `0 0 X Y gsave rectclip`, `l2`, `sp`, blank line.
8. Paint (requirement "PostScript paint output"), then unexpanded-subcell outlines if selected (requirement "PostScript cell outlines"), then, if labels are selected, label outlines followed by `grestore` and `f1 0 setgray` and the label texts (requirement "PostScript labels"); without labels just `grestore`.
9. Trailer: `pgsave restore showpage`, blank line, `%%Trailer`, `MAGICsave restore`, `%%EOF`.

All coordinates written in the page body are integers relative to (x0, y0) in internal units; the line width state starts at "medium".

Compatibility: exact

(Magic ref: plot/plotPS.c:1110-1334, magic/magicps.pro:1-72)

#### Scenario: Header of a 10 x 10 plot
- **GIVEN** default parameters, no labels selected, and a plotted area (0,0)-(10,10)
- **WHEN** a PostScript plot is produced
- **THEN** s = min(468/10, 648/10) = 46.8, the second line is `%%BoundingBox: 72 72 540 540`, the third is `%%DocumentNeededResources: font /Helvetica font /HelveticaBold`
- **AND** the font lines are `/f1 { 0.256 /Helvetica sf } def`, `/f2 { 0.256 /HelveticaBold sf } def`, `/f3 { 0.171 /Helvetica sf } def`, and the page setup contains `72 72 translate`, `46.800 46.800 scale` and `0 0 10 10 gsave rectclip`

### Requirement: PostScript paint output

For each PostScript layer style in order, the system SHALL write `col<color-index>`, then `<pattern> sl` if the fill is a pattern index, then process every tile of the style's types (intersected with the selected layers) in the hierarchical tile enumeration order (cell's own planes in plane order, each plane's tiles in canonical enumeration order, then subcells recursively), transformed to root coordinates, as follows; at the end of the style the pending rectangle and pending line are flushed and both buffers reset.

- Fill `X` or `B`: if the line width is not medium write `l2`; then if the tile's x-offset x = xbot − x0 ≥ 0, xbot ≤ x1, y-offset y ≥ 0 and ybot ≤ y1, write `x y w h mx` (X) or `x y w h mr` (B) with unclipped width and height; nothing else is done for the tile.
- Diagonal (split) tile: flush the pending rectangle and line and reset buffers; clip the triangle to the area and write its n vertices `x y ` each followed by `n tb`; if boundaries are on, write `l1` if the width is not thin, and draw the first non-axis-parallel polygon edge as `xa ya xb yb ml`.
- Other tile: clip to the area to get (x, y, w, h) relative to (x0, y0). Merge into the pending rectangle if it has the same width and x and is vertically adjacent (above: height grows; below: y moves down and height grows), else if it has the same height and y and is horizontally adjacent; otherwise flush the pending rectangle and make this one pending. Then if boundaries are on and the width is not thin, write `l1`.
- Flushing a pending rectangle of positive width writes `x y w h ms` for fill `S`, else `x y x+w y+h fb`.
- If boundaries are on, for each side (bottom, left, top, right — skipping sides a split tile does not have and sides at the plane's infinite boundary) every neighbouring segment whose type is not in the current style's selected types yields a boundary segment clipped to the tile side, transformed to root coordinates, and passed to the line buffer. Neighbours are visited left-to-right along the bottom, bottom-to-top along the left, right-to-left along the top and top-to-bottom along the right.
- Line buffer: a segment is normalised so x1 ≤ x2 (then so y1 ≤ y2 by swapping endpoints) relative to (x0, y0); it is dropped if it lies entirely left of 0 or right of X, or entirely below 0 or above Y (no partial clipping). A vertical segment with the same x as a pending vertical segment that touches it end-to-end extends it; likewise horizontal; otherwise the pending line is flushed and this one becomes pending. Flushing writes `dx x y hl` for horizontal (dx > 0), `dy x y vl` for vertical, `x1 y1 x2 y2 ml` otherwise; a zero-length pending line writes nothing.

Line width changes are written only when the width changes (`l1` thin, `l2` medium, `l3` thick).

Compatibility: exact

(Magic ref: plot/plotPS.c:290-760, plot/plotPS.c:1265-1280)

#### Scenario: Single solid square with boundary (verify against Magic)
- **GIVEN** a postscript style `m1 10 S`, boundaries on, and one metal1 tile (0,0)-(10,10) surrounded by space, plotted with area (0,0)-(10,10)
- **WHEN** the paint section is written
- **THEN** the lines after `sp` and a blank line are, in order: `col10`, `l1`, `10 0 0 hl`, `10 0 0 vl`, `10 0 10 hl`, `0 0 10 10 ms`, `10 10 0 vl`

#### Scenario: Box partially left of the area is dropped
- **GIVEN** an `X` style and a contact tile (−2,0)-(4,4) with plotted area (0,0)-(10,10)
- **WHEN** the paint section is written
- **THEN** no `mx` command is written for that tile (see Q-MISC-41)

### Requirement: PostScript cell outlines

When subcell outlines are selected, for every unexpanded subcell instance (each array element separately, not the root) in hierarchical order the system SHALL write `l3` if the width is not thick, its bounding box transformed to root coordinates as `x y w h mr` (subject to the same left/bottom drop rule), and, if `showcellnames` is true, `f2 (<cell name>) 5 cx ny lb` with cx = (xtop + xbot − 2·x0)/2 and ny = (2·ytop + ybot − 3·y0)/3, and `f3 (<instance id>) 5 cx iy lb` with iy = (ytop + 2·ybot − 3·y0)/3 (integer division truncating toward zero), each only when the point lies within [0, X] × [0, Y]. The instance id is the use id followed by `[x]`, `[y]` or `[y,x]` array indices, prefixed by `*` for a locked instance. Afterwards pending rectangle and line are flushed.

Compatibility: exact

(Magic ref: plot/plotPS.c:1000-1060, plot/plotPS.c:1283-1289, database/DBcellbox.c:62-104)

#### Scenario: Cell outline text
- **GIVEN** an unexpanded instance `inv_0` of cell `inv` whose root bounding box is (0,0)-(30,60) and plotted area (0,0)-(100,100)
- **WHEN** outlines are written
- **THEN** the output contains `0 0 30 60 mr`, `f2 (inv) 5 15 40 lb` and `f3 (inv_0) 5 15 20 lb`

### Requirement: PostScript labels

When labels are selected, the system SHALL (a) for each label in hierarchical order write `l2` if needed and a marker: point label `d x y pl` (cross of half-size d), line label via the line buffer, area label `x y w h mr`; then flush; (b) write `grestore` and `f1 0 setgray`; (c) for each label write `(<text>) <p> <x> <y> lb` when −d ≤ x ≤ X + d and −d ≤ y ≤ Y + d. The offset d = max(metal width, poly width)/8 from the router technology (integer division, at least 1; default widths 2 give d = 1). The anchor (x, y) and code p depend on the label's justification J after transformation: y = top + d (J ∈ {N, NE, NW}), centre-y (J ∈ {CENTER, W, E}, integer mean), bottom − d (J ∈ {S, SE, SW}); x = left − d (J ∈ {W, NW, SW}), centre-x (J ∈ {CENTER, N, S}), right + d (J ∈ {E, NE, SE}); p = 5 CENTER, 1 N, 0 NE, 4 E, 12 SE, 13 S, 15 SW, 7 W, 3 NW. Label text is written without escaping (see Q-MISC-42). Rendered-font attributes of labels are ignored.

Compatibility: exact

(Magic ref: plot/plotPS.c:760-1000, plot/plotPS.c:1150-1160, plot/plotPS.c:1291-1312, dbwind/DBWdisplay.c:121-122)

#### Scenario: North-justified point label
- **GIVEN** d = 1, plotted area origin (0,0), and a point label "vdd" at (5,5) with justification N
- **WHEN** labels are written
- **THEN** the marker line is `1 5 5 pl` and the text line is `(vdd) 1 5 6 lb`

### Requirement: PostScript label margins

Before computing the final scale the system SHALL estimate label overhang in points: with s0 the scale computed without labels, d' = trunc(d / s0), for each label with anchor (x, y) and code p (requirement "PostScript labels"): urx = trunc((x − x1)/s0), ury = trunc((y − y1)/s0), llx = trunc((x0 − x)/s0), lly = trunc((y0 − y)/s0) (x, y are relative to the area origin while x0, x1, y0, y1 are absolute; see Q-MISC-43); text width tw = len(text) × trunc(labelsize × 0.7), text height th = trunc(labelsize × 1.4); then adjust by p: 0: ury += th + d', urx += tw + d'; 4: ury += th/2, lly += th/2, urx += tw + d'; 12: lly += th + d', urx += tw + d'; 13: lly += th + d', urx += tw/2, llx += tw/2; 15: lly += th + d', llx += tw + d'; 7: ury += th/2, lly += th/2, llx += tw + d'; 3: ury += th + d', llx += tw + d'; 1: ury += th + d', urx += tw/2, llx += tw/2; 5: all four += half sizes. The margins are p_x = max(0, urx), p_y = max(0, ury), n_x = max(0, llx), n_y = max(0, lly) over all labels.

Compatibility: exact

(Magic ref: plot/plotPS.c:850-930, plot/plotPS.c:1140-1158)

#### Scenario: No labels gives zero margins
- **GIVEN** a plot with the labels layer not selected
- **WHEN** the scale is computed
- **THEN** all four margins are 0 and the translate line is `M M translate`

### Requirement: PNM style lines

Within `style pnm`, lines SHALL be recognised by the prefix of their first field (case-sensitive): `color [file]` loads a colour map (default `<display-style-type>.7bit.mraster.cmap`), `dstyle [file]` loads a display-style file (default `<display-style-type>.7bit.mraster_dstyle`), `default` assigns every user type the blend of the colours of all display styles that draw it, `draw <type>` does the same for one type, `draw <type> <style>` replaces the type's colour with the named style's colour (from the loaded dstyle file if any, else from the display style table; unknown style: "Unknown drawing style "<s>" for PNM plot." and the type keeps its previous colour), `map <type> <type>…` blends the colours of the listed types into the first type's colour; unknown first words are ignored. Colour-map files contain up to 128 lines `r g b` (lines starting with `#` and blank lines skipped); dstyle files contain a `display_styles` section of lines `ord mask color outline fill stipple shortname longname` whose colour index refers to the loaded colour map when in range, else to the display colour table; a format error reports "Format error in ..." and disables PNM plotting ("PNM module initialization had failed; cannot plot"). If no type received a colour by the end of the section, `default` is applied. A type is drawn only if one of its contributing styles has a nonzero write mask.

Colour blending of a new colour P over an existing colour C, per channel: v = P − 127 + floor(C / 2), clamped below at 0. The initial colour of every type is white (255,255,255), so the first blend yields P.

Compatibility: exact

```ebnf
pnm_line = ( "color" , [ ws , file ] ) | ( "dstyle" , [ ws , file ] ) | "default"
         | ( "draw" , ws , type , [ ws , style_name ] ) | ( "map" , ws , type , { ws , type } ) ;
```

(Magic ref: plot/plotPNM.c:1088-1300, plot/plotPNM.c:1300-1507, plot/plotPNM.c:1040-1085)

#### Scenario: Blending two display styles
- **GIVEN** a type drawn by two display styles with colours (0,0,255) then (255,0,0)
- **WHEN** `draw <type>` is processed
- **THEN** after the first style the colour is (0,0,255) and after the second it is (255−127+0, 0−127+0 → 0, 0−127+127) = (128, 0, 0)

### Requirement: PNM rasterisation and file format

`plot pnm <file> [width [layers]]` SHALL produce a binary PPM image as follows (width default 1000 pixels; a non-integer width reports "Width should be an integer number of pixels"; width ≤ 0 reports "PNM module given negative pixel width; cannot plot"):

1. B = bounding box of all plotted paint tiles clipped to the plotted area; if there is none, print "Empty region, no plot" and write nothing. Labels and cell outlines are not drawn in PNM output.
2. scale = 1 / (1 / (Bw / width)) (single precision); scale_over_2 = ceil(scale/2) if scale > 2 or 1/scale is not an integer, else 0. Image size im_x = trunc(Bw / scale), im_y = trunc(Bh / scale).
3. The raster covers B grown by scale_over_2 on each side at one sample per internal unit (divided by 2^k when downsampling k is in effect; k is increased automatically, with the message "<2^k>X downsampling forced by memory size requirements." and two explanatory lines, while pnmmaxmem×1024 < (3·scale + 2·scale_over_2)·3·raster_width / 4^k). The raster is processed in horizontal strips from top to bottom when it does not fit in memory; each strip is cleared to the background grey and every paint tile (contacts once) is blended into it with its type colour, pixel by pixel; diagonal tiles fill the pixels whose position satisfies the triangle inequality of the split.
4. Output pixel (i, j) (j = 0 at the top) samples raster position (x, y) = (scale_over_2 + scale·i, scale_over_2 + scale·(rows − 1 − j)) truncated to integers. Without antialiasing (scale_over_2 = 0) the raster value at (x, y) is copied. With antialiasing the value is Σ_dx Σ_dy w(dx)·w(dy)·R(x+dx, y+dy) / N over dx, dy ∈ [−h, h), h = scale_over_2 / 2^k, skipping rows beyond the raster top, where w(o) = L(min(1023, trunc(|o| / (scale / 2^(k+1)) × 1024))) with L(i) = sinc(i/1024)·sinc(i/2048) (L(0) = 1, π = 3.14159265), N = Σ over the full window of w(dx)·w(dy); each channel is truncated to 0..255.
5. File: name gets `.pnm` appended if it contains no `.`; content `P6\n<im_x> <im_y>\n255\n` followed by im_y rows of im_x RGB byte triples, top row first. The message "PNM image dimensions: <im_x> x <im_y>" is printed.

Compatibility: exact

(Magic ref: plot/plotPNM.c:120-230, plot/plotPNM.c:235-545, plot/plotPNM.c:545-1040, plot/plotCmd.c:300-340)

#### Scenario: One pixel per unit
- **GIVEN** metal1 tiles (0,0)-(10,10) and (90,90)-(100,100), metal1 PNM colour C, background 255, and `plot pnm out 100 m1`
- **WHEN** the plot is written
- **THEN** the file `out.pnm` starts with `P6\n100 100\n255\n`, the first (top-left) pixel is (255,255,255), the last pixel of the first row is C, and the first pixel of the last row is C

#### Scenario: Antialiasing triggered by fractional scale
- **GIVEN** a 100-unit-wide plotted region and width 50
- **WHEN** the plot is computed
- **THEN** scale = 2.0, 1/scale = 0.5 is not an integer, so scale_over_2 = 1 and the image is 50 pixels wide with antialiasing

### Requirement: SVG plot

`plot svg <file>` SHALL write an SVG 1.2 document of the current layout window's visible contents (window frame decorations and the box cursor excluded) at the window's pixel size, appending `.svg` when the name has no extension. Informative: Magic writes SVG only when it runs with the Cairo display ("Must be running in mode "-d XR" (CAIRO) to get SVG output."). Hefesto SHALL provide `plot svg` with every display.

Compatibility: behavioral

(Magic ref: graphics/grTCairo1.c:354-404, plot/plotCmd.c:265-282)

#### Scenario: Extension appended
- **WHEN** the user runs `plot svg view`
- **THEN** the file `view.svg` is written with the window's width and height

### Requirement: Raster printer and legacy plot devices

The `versatec`/`colorversatec` styles, the `plot versatec [scale [layers]]` command, PNM output to HP RTL/HPGL2 plotters (`pnmplotRTL`), and the optional `gremlin` and `pixels` devices SHALL be accepted for technology-file compatibility; their output is informative. Grammar of style lines: `versatec`: `<types> X|B` or `<types>` followed by 16 hexadecimal stipple words ("\"versatec\" lines must have either 2 or 17 fields."); `colorversatec`: `<types> X|B` or `<types> <color> w1 … wn` with color one of `black cyan magenta yellow K C M Y` and n ∈ {1, 2, 4, 8, 16} words repeated to 16; `gremlin`: `<types> X|B|<integer>`; `pixels`: lines ignored. In builds without the corresponding device, the lines are accepted and ignored. Raster plots are written to a temporary file in the `directory` parameter and printed by running the `spoolcommand` with the printer and file names.

Compatibility: informative

(Magic ref: plot/plotVers.c:323-500, plot/plotVers.c:1063-1490, plot/plotGremln.c:159-215, plot/plotPixels.c:175-182, plot/plotInt.h:28, plot/plotPNM.c:700-760)

#### Scenario: Versatec lines accepted
- **GIVEN** a `style versatec` block with `m1 X`
- **WHEN** the technology is loaded
- **THEN** no error is reported, whether or not raster printing is supported

### Requirement: Platform mechanism of plot output

Magic locates `magicps.pro` and display-style/colour-map files on its system library path and spools raster plots through an external command in a temporary directory (see Q-MISC-44); Hefesto SHALL embed the PostScript prologue text byte-for-byte and resolve style files from its bundled resources, and MAY omit spooling on platforms without process execution (iPadOS).

Compatibility: informative

(Magic ref: plot/plotPS.c:1180-1192, plot/plotPNM.c:1380-1400, plot/plotVers.c:1475-1485)

#### Scenario: Prologue embedded
- **GIVEN** no `magicps.pro` file on disk
- **WHEN** Hefesto writes a PostScript plot
- **THEN** the output contains the standard prologue rather than `postscript_prolog_is_missing`
