## MODIFIED Requirements

### Requirement: Plot command and plotted region

The system SHALL provide `plot <option> ...` where `<option>` is matched by unique prefix against `postscript`, `pnm`, `svg`, `parameters` and `help` (informative: Magic offers `svg` only when an SVG-capable display is built in, and `versatec`, `gremlin` and `pixels` only in builds that include those devices, which the default build does not; Hefesto offers `svg` with every display and none of the raster devices, see Q-MISC-51); no option prints the help list ("The "plot" commands are:" followed by one line per option); an unknown option reports ""<x>" isn't a valid plot option." and prints the help. For every output option the plotted window is the layout window under the pointer or the only layout window ("The cursor must be over a layout window to plot." otherwise); the plotted area is the box if the box lies in that window's root cell, else the root cell's bounding box; the default layer set is the window's visible layers, plus labels when the window displays labels, plus subcell outlines; an explicit layer-list argument replaces that set (special names `labels` and `subcell` select labels and unexpanded-subcell outlines). Hierarchy is plotted according to the window's expansion state: paint and labels of expanded cells, bounding boxes of unexpanded subcells.

Compatibility: behavioral

```ebnf
plot_cmd   = "plot" , [ ws , ( ps_cmd | pnm_cmd | svg_cmd | param_cmd | "help" ) ] ;
ps_cmd     = "postscript" , ws , file , [ ws , layers ] ;
pnm_cmd    = "pnm" , ws , file , [ ws , width_px , [ ws , layers ] ] ;
svg_cmd    = "svg" , ws , file ;
param_cmd  = "parameters" , [ ws , name , ws , value ] ;
```

(Magic ref: plot/plotCmd.c:50-362, commands/CmdSubrs.c:486-590) (See Q-MISC-51.)

#### Scenario: Box outside the root cell
- **GIVEN** a layout window showing cell TOP whose bounding box is (0,0)-(200,100), and the box tool in a different window's cell
- **WHEN** the user runs `plot postscript out.ps`
- **THEN** the area (0,0)-(200,100) of TOP is plotted

#### Scenario: Invalid option
- **WHEN** the user runs `plot foo`
- **THEN** ""foo" isn't a valid plot option." is reported followed by the option list

#### Scenario: No raster printer device
- **WHEN** the user runs `plot versatec`
- **THEN** ""versatec" isn't a valid plot option." is reported followed by the option list `postscript`, `pnm`, `svg`, `parameters`, `help`

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
| `pnmmaxmem` | 65536 | integer 1..4194304, KB of raster memory ("pnmmaxmem must be an integer 1-4194304."; see Q-MISC-49) |
| `pnmdownsample` | 0 | integer ≥ 0, base-2 log of the downsampling factor |
| `pnmbackground` | 255 | integer 0..255, background grey level |

The printed listing (informative) groups parameters under "General plotting parameters are:", "Postscript plotting parameters are:" and "PNM plotting parameters are:", e.g. `    PS_width:       612 (8.500 in)`.

Informative: builds of Magic with raster printing also offer `cellidfont`, `cellnamefont`, `labelfont`, `directory`, `dotsperinch`, `printer`, `spoolcommand`, `swathheight`, `width`, `plottype` and `pnmplotRTL`, listed under "HP/Versatec plotting parameters are:". The default Magic build and Hefesto do not (see Q-MISC-51); in Hefesto these names are unknown parameters.

(Magic ref: plot/plotMain.c:300-650, plot/plotPS.c:120-140, plot/plotPNM.c:60-75, plot/plotVers.c:95-140) (See Q-MISC-49, Q-MISC-51.)

#### Scenario: Reject zero label size
- **WHEN** the user runs `plot parameters ps_labelsize 0`
- **THEN** "PS_labelSize must be an integer greater than zero." is reported and the size stays 12

#### Scenario: Plot type sets resolution
- **WHEN** the user runs `plot parameters plottype hpgl2`
- **THEN** ""plottype" isn't a valid plot parameter." is reported and all parameters are printed (informative: a Magic build with raster printing sets dotsperinch to 300 and width to 10650)

### Requirement: PostScript labels

When labels are selected, the system SHALL (a) for each label in hierarchical order write `l2` if needed and a marker: point label `d x y pl` (cross of half-size d), line label via the line buffer, area label `x y w h mr`; then flush; (b) write `grestore` and `f1 0 setgray`; (c) for each label write `(<text>) <p> <x> <y> lb` when −d ≤ x ≤ X + d and −d ≤ y ≤ Y + d. The offset d = max(metal width, poly width)/8 from the router technology (integer division, at least 1; default widths 2 give d = 1). The anchor (x, y) and code p depend on the label's justification J after transformation: y = top + d (J ∈ {N, NE, NW}), centre-y (J ∈ {CENTER, W, E}, integer mean), bottom − d (J ∈ {S, SE, SW}); x = left − d (J ∈ {W, NW, SW}), centre-x (J ∈ {CENTER, N, S}), right + d (J ∈ {E, NE, SE}); p = 5 CENTER, 1 N, 0 NE, 4 E, 12 SE, 13 S, 15 SW, 7 W, 3 NW. In label texts, cell names and instance ids, each `(`, `)` and `\` SHALL be written preceded by a `\`, in both `magic-compat` settings, so the file is valid PostScript (Magic writes them unescaped, which produces an invalid file; see Q-MISC-42). Rendered-font attributes of labels are ignored. Label margins (requirement "PostScript label margins") count the characters of the unescaped text.

Compatibility: exact

(Magic ref: plot/plotPS.c:760-1000, plot/plotPS.c:1150-1160, plot/plotPS.c:1291-1312, dbwind/DBWdisplay.c:121-122) (See Q-MISC-42.)

#### Scenario: North-justified point label
- **GIVEN** d = 1, plotted area origin (0,0), and a point label "vdd" at (5,5) with justification N
- **WHEN** labels are written
- **THEN** the marker line is `1 5 5 pl` and the text line is `(vdd) 1 5 6 lb`

#### Scenario: Parenthesis in a label
- **GIVEN** d = 1, plotted area origin (0,0), and a point label "a(1)" at (5,5) with justification N
- **WHEN** labels are written, with `magic-compat` ON or OFF
- **THEN** the text line is `(a\(1\)) 1 5 6 lb`

### Requirement: SVG plot

`plot svg <file>` SHALL write an SVG 1.2 document of the current layout window's visible contents (window frame decorations and the box cursor excluded) at the window's pixel size, appending `.svg` when the name has no extension (the extension test follows requirement "Plot behaviour with magic-compat off" for `magic-compat` OFF). Hefesto SHALL provide `plot svg` with every display and in headless sessions, where the window has its headless frame (512 × 484 pixels, Magic's null-display screen). The document is produced by the view export of capability `hefesto-rendering` (requirement "View export to SVG and PNG"): its root element has `width` and `height` equal to the window's screen area in pixels and a `viewBox` of the same size, and it contains the layers, labels and cell outlines the window draws. Informative: Magic writes SVG only when it runs with the Cairo display ("Must be running in mode "-d XR" (CAIRO) to get SVG output."), and the drawing inside the file is Cairo's rendering of the display; only the file name, the size and the document type are compared with Magic (see Q-MISC-52).

Compatibility: behavioral

(Magic ref: graphics/grTCairo1.c:354-404, plot/plotCmd.c:265-282) (See Q-MISC-52, Q-MISC-55.)

#### Scenario: Extension appended
- **WHEN** the user runs `plot svg view`
- **THEN** the file `view.svg` is written with the window's width and height

#### Scenario: Headless SVG
- **GIVEN** a headless session with one layout window in its default frame
- **WHEN** the user runs `plot svg v`
- **THEN** `v.svg` is an SVG document whose root element has a positive `width` and `height` equal to the window's screen area in pixels

### Requirement: Raster printer and legacy plot devices

The `versatec`, `colorversatec`, `gremlin` and `pixels` styles of the technology `plot` section SHALL be accepted for technology-file compatibility and ignored: no error is reported for their lines, whatever they contain, and no output uses them. Hefesto SHALL NOT provide `plot versatec`, `plot gremlin`, `plot pixels`, PNM output to HP RTL/HPGL2 plotters or printer spooling (see Q-MISC-51, Q-MISC-44), as the default Magic build does not. Informative: in Magic builds with these devices the style-line grammar is: `versatec`: `<types> X|B` or `<types>` followed by 16 hexadecimal stipple words ("\"versatec\" lines must have either 2 or 17 fields."); `colorversatec`: `<types> X|B` or `<types> <color> w1 … wn` with color one of `black cyan magenta yellow K C M Y` and n ∈ {1, 2, 4, 8, 16} words repeated to 16; `gremlin`: `<types> X|B|<integer>`; `pixels`: lines ignored; raster plots are written to a temporary file in the `directory` parameter and printed by running the `spoolcommand` with the printer and file names.

Compatibility: behavioral

(Magic ref: plot/plotVers.c:241-498, plot/plotVers.c:1063-1490, plot/plotGremln.c:131-217, plot/plotPixels.c:149-183, plot/plotInt.h:28, plot/plotPNM.c:700-760, plot/Makefile.in:19-22) (See Q-MISC-44, Q-MISC-45, Q-MISC-51.)

#### Scenario: Versatec lines accepted
- **GIVEN** a `style versatec` block with `m1 X` and a line `m2 1 2 3`
- **WHEN** the technology is loaded
- **THEN** no error is reported and no plot uses the lines

### Requirement: Platform mechanism of plot output

Magic locates `magicps.pro` and display-style and colour-map files on its system library path and spools raster plots through an external command in a temporary directory (see Q-MISC-44). Hefesto SHALL embed the PostScript prologue text of Magic 8.3.694 byte-for-byte and SHALL bundle the colour-map and display-style files the PNM defaults name for the `mos` display-style type (`mos.7bit.mraster.cmap`, `mos.7bit.mraster_dstyle`), with their notices. A PostScript plot SHALL always contain the embedded prologue; a `magicps.pro` file on disk is not read. The PNM `color` and `dstyle` lines SHALL search the current directory, then the technology's directory, then the bundled resources. Hefesto SHALL NOT start a process to print a plot on any platform.

Compatibility: informative

(Magic ref: plot/plotPS.c:1180-1192, plot/plotPS.c:1221-1232, plot/plotPNM.c:1380-1400, plot/plotPNM.c:1456-1507, plot/plotVers.c:1475-1485) (See Q-MISC-44.)

#### Scenario: Prologue embedded
- **GIVEN** no `magicps.pro` file on disk
- **WHEN** Hefesto writes a PostScript plot
- **THEN** the output contains the standard prologue rather than `postscript_prolog_is_missing`

#### Scenario: Default PNM colours on an iPad
- **GIVEN** the sky130A technology, whose `style pnm` has `default` and no `color` or `dstyle` line, on iPadOS with no Magic installation
- **WHEN** the user runs `plot pnm inv 200`
- **THEN** the image is written with the colours of the bundled `mos.7bit.mraster` files

## ADDED Requirements

### Requirement: Plot behaviour with magic-compat off

With `magic-compat` OFF, plotting SHALL differ from Magic in exactly these points; with it ON, each is as the other requirements of this capability state:
- **Rectangles that start outside the area (Q-MISC-41).** An `X` or `B` style tile and an unexpanded-cell outline are written when any part of them overlaps the plotted area, not only when the lower-left corner lies inside it. PostScript's clipping (`rectclip`) trims them.
- **Label margins (Q-MISC-43).** The label overhang of requirement "PostScript label margins" uses urx = trunc((x − X)/s0) and ury = trunc((y − Y)/s0), with x and y relative to the area origin as the other bounds are, so the margins do not depend on where the area lies.
- **File extension (Q-MISC-55).** `plot pnm` and `plot svg` append their extension when the file name after the last `/` has no `.`, instead of testing the whole path.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2) (See Q-MISC-41, Q-MISC-43, Q-MISC-55.)

#### Scenario: Cell outline across the left edge
- **GIVEN** `magic-compat` OFF, an unexpanded instance whose root bounding box is (−10,0)-(20,30), and a plotted area (0,0)-(100,100)
- **WHEN** a PostScript plot with subcell outlines is written
- **THEN** the output contains `-10 0 30 30 mr`, which Magic and `magic-compat` ON omit

#### Scenario: Margins away from the origin
- **GIVEN** `magic-compat` OFF and the same labels plotted once with the area (0,0)-(100,100) and once with the area (1000,1000)-(1100,1100) shifted with them
- **WHEN** both plots are written
- **THEN** both files have the same `%%BoundingBox` and scale

#### Scenario: Dot in a directory name
- **GIVEN** `magic-compat` OFF
- **WHEN** the user runs `plot pnm dir.v2/out 100`
- **THEN** the file `dir.v2/out.pnm` is written; with `magic-compat` ON the file is `dir.v2/out`

### Requirement: Plot output safety and limits

In both `magic-compat` settings plotting SHALL NOT crash, hang or write a partial file:
- **Zero-area PostScript plot (Q-MISC-48).** When the plotted area has zero width or height, `plot postscript` reports "Nothing to plot: the plotted area has zero width or height." and writes no file.
- **PNM memory arithmetic (Q-MISC-49).** Memory sizes are computed in 64-bit integers; every strip advances by at least one output row; the forced downsampling stops at a factor of 2^15, and an image that still does not fit in `pnmmaxmem` is reported and not written. The image is at most 65 535 pixels on each side; a larger width is reported and nothing is written.
- **Malformed style lines (Q-MISC-50).** A `map` line with no type and a `style postscript` line with a non-integer index, colour component or colour field are reported at technology load and ignored. A layer style naming an undefined colour or pattern is reported once per plot and skipped.
- **Complete files (Q-MISC-53).** Each output is written to a temporary file in the target directory and renamed onto the target name only when complete. An interrupted plot prints Magic's interrupt message, and a failed write reports the error; in both cases the temporary file is removed and an existing file of the target name is unchanged.
- **Cross-platform identity (Q-MISC-54).** PostScript and PNM output for the same inputs are byte-identical on Linux x86_64, Linux aarch64 and macOS arm64; the PNM antialiasing kernel is a fixed table, not computed from the platform's math library.

Compatibility: native

(Hefesto decisions: docs/decisions/cross-cutting.md#X-1, docs/decisions/cross-cutting.md#X-2) (See Q-MISC-48, Q-MISC-49, Q-MISC-50, Q-MISC-53, Q-MISC-54.)

#### Scenario: Zero-width box
- **GIVEN** the box (10,0)-(10,50) in the root cell of the plotted window
- **WHEN** the user runs `plot postscript z.ps`
- **THEN** "Nothing to plot: the plotted area has zero width or height." is reported and `z.ps` is not created

#### Scenario: Strip smaller than the antialiasing halo
- **GIVEN** `plot parameters pnmmaxmem 1` and a region whose scale gives scale_over_2 = 40
- **WHEN** the user runs `plot pnm big 2000`
- **THEN** the plot finishes, or reports that it does not fit and writes nothing, within the plot benchmark's time limit; it never loops without progress

#### Scenario: Interrupted PNM plot keeps the old file
- **GIVEN** a file `out.pnm` from an earlier plot
- **WHEN** a new `plot pnm out 4000` is interrupted
- **THEN** " *** interrupted ***" is printed and `out.pnm` is byte-identical to the earlier file, with no temporary file left in the directory

#### Scenario: Bare map line
- **GIVEN** a `style pnm` block containing the line `map`
- **WHEN** the technology is loaded
- **THEN** the line is reported and ignored, and the technology loads

### Requirement: Plot export from the desktop

The desktop app SHALL offer File > "Export Image…", which plots the current layout window as PostScript, PNM, SVG or PNG. The dialog SHALL offer the plotted area (box or whole cell, as requirement "Plot command and plotted region" chooses), the layers (the window's visible layers by default), the PNM width and the PNG size, and SHALL run the same command as the console (`plot postscript`, `plot pnm`, `plot svg`; PNG through the view export of capability `hefesto-rendering`), so the console shows the command and its messages. The plot runs with the progress and cancel controls of long operations. On iPadOS the same action is in the document menu, and the file goes to the share sheet.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-13, docs/decisions/audit/DECISIONS.md#D-37)

#### Scenario: Export as PostScript
- **GIVEN** a layout window with a box in its root cell
- **WHEN** the user chooses File > "Export Image…", picks PostScript and the file `chip.ps`
- **THEN** the console shows `plot postscript chip.ps` and the file is the same as the command writes
