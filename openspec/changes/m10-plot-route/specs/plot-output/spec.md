## MODIFIED Requirements

### Requirement: Plot command and plotted region

The system SHALL provide `plot <option> ...` where `<option>` is matched by unique prefix against `postscript`, `pnm`, `svg`, `parameters` and `help` (informative: Magic 8.3.694's default build also offers `versatec`, and adds `svg` only when built with Cairo and the Tcl wrapper; `gremlin` and `pixels` are compiled out. Hefesto deviates as a platform decision, in both `magic-compat` settings: it offers `svg` with every display and does not offer `versatec`, see Q-MISC-51); no option prints the help list ("The "plot" commands are:" followed by one line per option); an unknown option reports ""<x>" isn't a valid plot option." and prints the help. For every output option the plotted window is the layout window under the pointer or the only layout window ("The cursor must be over a layout window to plot." otherwise); the plotted area is the box if the box lies in that window's root cell, else the root cell's bounding box; the default layer set is the window's visible layers, plus labels when the window displays labels, plus subcell outlines; an explicit layer-list argument replaces that set (special names `labels` and `subcell` select labels and unexpanded-subcell outlines). Hierarchy is plotted according to the window's expansion state: paint and labels of expanded cells, bounding boxes of unexpanded subcells.

Compatibility: behavioral

```ebnf
plot_cmd   = "plot" , [ ws , ( ps_cmd | pnm_cmd | svg_cmd | param_cmd | "help" ) ] ;
ps_cmd     = "postscript" , ws , file , [ ws , layers ] ;
pnm_cmd    = "pnm" , ws , file , [ ws , width_px , [ ws , layers ] ] ;
svg_cmd    = "svg" , ws , file ;
param_cmd  = "parameters" , [ ws , name , ws , value ] ;
```

(Magic ref: plot/plotCmd.c:89-361, commands/CmdSubrs.c:485-640) (See Q-MISC-51.)

#### Scenario: Box outside the root cell
- **GIVEN** a layout window showing cell TOP whose bounding box is (0,0)-(200,100), and the box tool in a different window's cell
- **WHEN** the user runs `plot postscript out.ps`
- **THEN** the area (0,0)-(200,100) of TOP is plotted

#### Scenario: Invalid option
- **WHEN** the user runs `plot foo`
- **THEN** ""foo" isn't a valid plot option." is reported followed by the option list

#### Scenario: No raster printer device (Hefesto deviates)
- **WHEN** the user runs `plot versatec`, with `magic-compat` ON or OFF
- **THEN** ""versatec" isn't a valid plot option." is reported followed by the option list `postscript`, `pnm`, `svg`, `parameters`, `help` (Magic rasterises the area and spools it to a printer; Q-MISC-51)

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

Hefesto deviates (Q-MISC-51, platform, both `magic-compat` settings): Magic's default build also has `cellidfont`, `cellnamefont`, `labelfont`, `directory`, `dotsperinch`, `printer`, `spoolcommand`, `swathheight`, `width` and `plottype`, listed under "HP/Versatec plotting parameters are:", and `pnmplotRTL`, listed as the last line of the PNM group. Hefesto neither lists nor accepts them; in Hefesto these names are unknown parameters. The golden cases that print the listing (MISC-PLOT-05, MISC-PLOT-17) carry these lines as a documented deviation.

(Magic ref: plot/plotMain.c:280-650, plot/plotPS.c:123-135, plot/plotPNM.c:77-82, plot/plotVers.c:108-142) (See Q-MISC-49, Q-MISC-51.)

#### Scenario: Reject zero label size
- **WHEN** the user runs `plot parameters ps_labelsize 0`
- **THEN** "PS_labelSize must be an integer greater than zero." is reported and the size stays 12

#### Scenario: Plot type sets resolution
- **WHEN** the user runs `plot parameters plottype hpgl2`, with `magic-compat` ON or OFF
- **THEN** ""plottype" isn't a valid plot parameter." is reported and all parameters are printed (Magic sets dotsperinch to 300 and width to 10650; Q-MISC-51)

### Requirement: PostScript labels

When labels are selected, the system SHALL (a) for each label in hierarchical order write `l2` if needed and a marker: point label `d x y pl` (cross of half-size d), line label via the line buffer, area label `x y w h mr`; then flush; (b) write `grestore` and `f1 0 setgray`; (c) for each label write `(<text>) <p> <x> <y> lb` when −d ≤ x ≤ X + d and −d ≤ y ≤ Y + d. The offset d = max(metal width, poly width)/8 from the router technology (integer division, at least 1; default widths 2 give d = 1). The anchor (x, y) and code p depend on the label's justification J after transformation: y = top + d (J ∈ {N, NE, NW}), centre-y (J ∈ {CENTER, W, E}, integer mean), bottom − d (J ∈ {S, SE, SW}); x = left − d (J ∈ {W, NW, SW}), centre-x (J ∈ {CENTER, N, S}), right + d (J ∈ {E, NE, SE}); p = 5 CENTER, 1 N, 0 NE, 4 E, 12 SE, 13 S, 15 SW, 7 W, 3 NW. In label texts, cell names and instance ids, each `(`, `)` and `\` SHALL be written preceded by a `\`, in both `magic-compat` settings, so the file is valid PostScript (Magic writes them unescaped, which produces an invalid file; see Q-MISC-42). Rendered-font attributes of labels are ignored. Label margins (requirement "PostScript label margins") count the characters of the unescaped text.

Compatibility: exact

(Magic ref: plot/plotPS.c:761-1030, plot/plotPS.c:1170-1177, plot/plotPS.c:1304-1324, dbwind/DBWdisplay.c:121-122) (See Q-MISC-42.)

#### Scenario: North-justified point label
- **GIVEN** d = 1, plotted area origin (0,0), and a point label "vdd" at (5,5) with justification N
- **WHEN** labels are written
- **THEN** the marker line is `1 5 5 pl` and the text line is `(vdd) 1 5 6 lb`

#### Scenario: Parenthesis in a label
- **GIVEN** d = 1, plotted area origin (0,0), and a point label "a(1)" at (5,5) with justification N
- **WHEN** labels are written, with `magic-compat` ON or OFF
- **THEN** the text line is `(a\(1\)) 1 5 6 lb`

### Requirement: SVG plot

`plot svg <file>` SHALL write an SVG 1.2 document of the current layout window's visible contents (window frame decorations and the box cursor excluded) at the window's pixel size, appending `.svg` when the name has no extension (the extension test follows requirement "Plot behaviour with magic-compat off" for `magic-compat` OFF). Hefesto SHALL provide `plot svg` with every display and in headless sessions, where the window has its headless frame (512 × 484 pixels, Magic's null-display screen). The document is produced by the view export of capability `hefesto-rendering` (requirement "View export to SVG and PNG"): its root element has `width` and `height` equal to the window's screen area in pixels and a `viewBox` of the same size, and it contains the layers, labels and cell outlines the window draws. Hefesto deviates with `magic-compat` ON as well (platform decision, Q-MISC-52): Magic writes SVG only when it is built with Cairo and the Tcl wrapper and runs with the Cairo display; with another display it reports `Must be running in mode "-d XR" (CAIRO) to get SVG output.` and writes nothing, and without Cairo `svg` is not a plot option. Informative: the drawing inside Magic's file is Cairo's rendering of the display, with `width` and `height` written in points (`512pt` for a 512-pixel window); only the file name, the numeric width and height and the document type are compared with Magic (see Q-MISC-52).

Compatibility: behavioral

(Magic ref: graphics/grTCairo1.c:354-406, plot/plotCmd.c:270-287) (See Q-MISC-52, Q-MISC-55.)

#### Scenario: Extension appended
- **WHEN** the user runs `plot svg view`
- **THEN** the file `view.svg` is written with the window's width and height

#### Scenario: Headless SVG
- **GIVEN** a headless session with one layout window in its default frame
- **WHEN** the user runs `plot svg v`
- **THEN** `v.svg` is an SVG document whose root element has a positive `width` and `height` equal to the window's screen area in pixels

### Requirement: Raster printer and legacy plot devices

The `versatec`, `colorversatec`, `gremlin` and `pixels` styles of the technology `plot` section SHALL be accepted for technology-file compatibility and ignored: no error is reported for their lines, whatever they contain, and no output uses them. Hefesto SHALL NOT provide `plot versatec`, `plot gremlin`, `plot pixels`, PNM output to HP RTL/HPGL2 plotters or printer spooling (see Q-MISC-51, Q-MISC-44). For `gremlin` and `pixels` this is what Magic's default build does (both devices are compiled out). For Versatec and HP RTL Hefesto deviates, as a platform decision in both `magic-compat` settings: Magic's default build parses the `versatec` and `colorversatec` lines (reporting malformed ones), provides `plot versatec` and spools raster plots. Informative: Magic's style-line grammar is: `versatec`: `<types> X|B` or `<types>` followed by 16 hexadecimal stipple words ("\"versatec\" lines must have either 2 or 17 fields."); `colorversatec`: `<types> X|B` or `<types> <color> w1 … wn` with color one of `black cyan magenta yellow K C M Y` and n ∈ {1, 2, 4, 8, 16} words repeated to 16; `gremlin`: `<types> X|B|<integer>`; `pixels`: lines ignored; raster plots are written to a temporary file in the `directory` parameter and printed by running the `spoolcommand` with the printer and file names.

Compatibility: behavioral

(Magic ref: plot/plotInt.h:28, plot/plotVers.c:212-498, plot/plotVers.c:1063-1493, plot/plotGremln.c:131-217, plot/plotPixels.c:149-181, plot/plotPNM.c:745-818, plot/plotPNM.c:895-954) (See Q-MISC-44, Q-MISC-45, Q-MISC-51.)

#### Scenario: Versatec lines accepted
- **GIVEN** a `style versatec` block with `m1 X` and a line `m2 1 2 3`
- **WHEN** the technology is loaded
- **THEN** no error is reported and no plot uses the lines (Magic reports ""versatec" lines must have either 2 or 17 fields." for the second line; Q-MISC-51)

### Requirement: Platform mechanism of plot output

Magic locates `magicps.pro` and display-style and colour-map files on its system library path and spools raster plots through an external command in a temporary directory (see Q-MISC-44). Hefesto SHALL embed the PostScript prologue text of Magic 8.3.694 byte-for-byte; a PostScript plot SHALL always contain the embedded prologue, and a `magicps.pro` file on disk is not read. The PNM colours come from two sources, as in Magic:
- **`default` and `draw <type>`** blend the colours of the display styles that draw each type, from the display style table and colour map the session loaded for its display (Magic's `GrStyleTable` and colour map, loaded by `styletype`). A headless session uses the `7bit` display class for this, as Magic's null display does (`mos.7bit.dstyle` and `mos.7bit.std.cmap` for the `mos` style type). When the session has no display style set loaded (a headless session without display files, or iPadOS with no Magic installation), the plot loads the `7bit` files of the technology's style type from the display search path, then from Hefesto's bundled resources.
- **`color` and `dstyle` lines** load a plot-only colour map and style table (default names `<style type>.7bit.mraster.cmap` and `<style type>.7bit.mraster_dstyle`), used by `draw <type> <style>` and discarded at the end of the section. They SHALL search the current directory, then the technology's directory, then the bundled resources.

Hefesto SHALL bundle, unmodified and with Magic's notice, `mos.7bit.mraster.cmap` and `mos.7bit.mraster_dstyle` (for `color` and `dstyle` lines without a file name) and `mos.7bit.dstyle` and `mos.7bit.std.cmap` (for the `default` colours when no display style set is loaded), unless Hefesto's display assets already carry the last two. Hefesto SHALL NOT start a process to print a plot on any platform.

Compatibility: informative

(Magic ref: plot/plotPS.c:1221-1232, plot/plotPNM.c:1033-1060, plot/plotPNM.c:1150-1165, plot/plotPNM.c:1261-1279, plot/plotPNM.c:1305-1318, plot/plotPNM.c:1360-1400, plot/plotPNM.c:1456-1507, dbwind/DBWdisplay.c:1750-1765, graphics/grMain.c:215-216, plot/plotVers.c:1474-1483) (See Q-MISC-44.)

#### Scenario: Prologue embedded
- **GIVEN** no `magicps.pro` file on disk
- **WHEN** Hefesto writes a PostScript plot
- **THEN** the output contains the standard prologue rather than `postscript_prolog_is_missing`

#### Scenario: Default PNM colours on an iPad
- **GIVEN** the sky130A technology, whose `style pnm` has `default` and no `color` or `dstyle` line, on iPadOS with no Magic installation and no display style set loaded
- **WHEN** the user runs `plot pnm inv 200`
- **THEN** the image is written with the blended colours of the display styles in the bundled `mos.7bit.dstyle` and `mos.7bit.std.cmap`, the same as a headless Magic run (`-dnull`) writes; the `mos.7bit.mraster` files are not used

## ADDED Requirements

### Requirement: Plot behaviour with magic-compat off

With `magic-compat` OFF, plotting SHALL differ from Magic in exactly these points; with it ON, each is as the other requirements of this capability state:
- **Rectangles that start outside the area (Q-MISC-41).** An `X` or `B` style tile and an unexpanded-cell outline are written when any part of them overlaps the plotted area, not only when the lower-left corner lies inside it. PostScript's clipping (`rectclip`) trims them.
- **Label margins (Q-MISC-43).** The label overhang of requirement "PostScript label margins" uses urx = trunc((x − X)/s0), ury = trunc((y − Y)/s0), llx = trunc(−x/s0) and lly = trunc(−y/s0), where X and Y are the area's width and height: every bound is relative to the area origin, as the anchor (x, y) is, so the margins do not depend on where the area lies.
- **File extension (Q-MISC-55).** `plot pnm` and `plot svg` append their extension when the file name after the last `/` has no `.`, instead of testing the whole path.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2) (See Q-MISC-41, Q-MISC-43, Q-MISC-55.)

#### Scenario: Cell outline across the left edge with magic-compat off
- **GIVEN** `magic-compat` OFF, an unexpanded instance whose root bounding box is (−10,0)-(20,30), and a plotted area (0,0)-(100,100)
- **WHEN** a PostScript plot with subcell outlines is written
- **THEN** the output contains `-10 0 30 30 mr`

#### Scenario: Cell outline across the left edge with magic-compat on
- **GIVEN** `magic-compat` ON, an unexpanded instance whose root bounding box is (−10,0)-(20,30), and a plotted area (0,0)-(100,100)
- **WHEN** a PostScript plot with subcell outlines is written
- **THEN** no `mr` line is written for that outline, as Magic

#### Scenario: Margins away from the origin with magic-compat off
- **GIVEN** `magic-compat` OFF and the same labels plotted once with the area (0,0)-(100,100) and once with the area (1000,1000)-(1100,1100) shifted with them
- **WHEN** both plots are written
- **THEN** both files have the same `%%BoundingBox` and scale

#### Scenario: Margins away from the origin with magic-compat on
- **GIVEN** `magic-compat` ON and a point label at the centre of the area (1000,1000)-(1100,1100)
- **WHEN** a PostScript plot with labels is written
- **THEN** the left and bottom margins are computed as Magic does, llx = trunc((1000 − 50)/s0) and lly = trunc((1000 − 50)/s0) from the absolute area corner and the relative anchor (50, 50), so the `%%BoundingBox` and scale differ from those of the same plot with the area at (0,0)-(100,100)

#### Scenario: Dot in a directory name with magic-compat off
- **GIVEN** `magic-compat` OFF
- **WHEN** the user runs `plot pnm dir.v2/out 100`
- **THEN** the file `dir.v2/out.pnm` is written

#### Scenario: Dot in a directory name with magic-compat on
- **GIVEN** `magic-compat` ON
- **WHEN** the user runs `plot pnm dir.v2/out 100`
- **THEN** the file `dir.v2/out` is written, with no extension, as Magic

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

The desktop app SHALL offer File > "Export Image…", which plots the current layout window as PostScript, PNM, SVG or PNG. For PostScript and PNM the dialog SHALL offer the plotted area (box or whole cell, as requirement "Plot command and plotted region" chooses) and the layers (the window's visible layers by default), and for PNM the width. SVG and PNG export what the window shows, as `plot svg` does, so for them the dialog offers no area or layer choice; PNG offers the image size. The dialog SHALL run the same command as the console (`plot postscript`, `plot pnm`, `plot svg`; PNG through the view export of capability `hefesto-rendering`), so the console shows the command and its messages. The plot runs with the progress and cancel controls of long operations. On iPadOS the same action is in the document menu, and the file goes to the share sheet.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-13, docs/decisions/audit/DECISIONS.md#D-37)

#### Scenario: Export as PostScript
- **GIVEN** a layout window with a box in its root cell
- **WHEN** the user chooses File > "Export Image…", picks PostScript and the file `chip.ps`
- **THEN** the console shows `plot postscript chip.ps` and the file is the same as the command writes
