# Plowing, Plotting and Wiring

## Purpose
This spec covers three editing and output aids:
- **Plowing:** moves an edge of the layout and pushes the material in front of it, keeping design rules intact.
- **Plotting:** renders the layout under the box to PostScript, PNM, SVG and older formats.
- **The wiring tool:** draws wires and automatically places contacts when the wire changes layer.

## Requirements

### Requirement: Plowing
`plow <dir> [layers]` SHALL move the box's leading edge in the given direction, and SHALL push the edit-cell material in front of it while keeping the DRC spacing rules. The default layers SHALL be `*,l,subcell,space`.

`plow` SHALL also support these options:
- `boundary`, `noboundary`
- `horizon n`; the jog horizon defaults to 0
- `jogs`, `nojogs`
- `straighten`, `nostraighten`
- `selection [dir [amt]]`

The tech `plowing` section SHALL declare `fixed`, `covered` and `drag` types. Contacts SHALL always be treated as fixed. Plowing does not handle non-Manhattan geometry, stacked contacts, or newer DRC rule types. (src: plow/PlowCmd.c:68-310, plow/PlowMain.c:74, plow/PlowTech.c:822-857, README.md)

#### Scenario: Plow east
- **WHEN** the box edge is plowed east by 5
- **THEN** material in front of the edge SHALL move east, and spacing rules SHALL still be satisfied

#### Scenario: Boundary clipping
- **WHEN** plowing would cross the plow boundary
- **THEN** the system SHALL report "Reduced plow size to stay within the boundary."

### Requirement: Plotting
`plot` SHALL render the box area (or the whole cell when there is no box) in one of these formats:
- `postscript file [layers]`
- `pnm file [width [layers]]`, default width 1000 px
- `svg file`, only when built with Cairo and Tcl
- `versatec`
- `gremlin`, only when compiled with `-DGREMLIN`

`plot parameters [name value]` SHALL show or set the plot parameters. The tech `plot` section SHALL define styles for each plot format. (src: plot/plotCmd.c:63-361, plot/plotMain.c:57-421, plot/plotPNM.c)

#### Scenario: PNM plot
- **WHEN** `plot pnm out.pnm 2000` is run
- **THEN** a 2000-pixel-wide PNM image of the box area SHALL be written

### Requirement: Wiring tool
The `wire` command SHALL support:
- `type [layer [width]]`
- `leg`, `horizontal`, `vertical`, each with an optional `to x y`
- `switch [layer width]`
- `increment|decrement layer|width`
- `width`, `show`, `values`
- `segment <coords|file> [-noendcap]`

`wire type` with no arguments SHALL take the wire layer and width from the material under the cursor. Its width SHALL be the smaller dimension of the chunk.

The tech `wiring` section SHALL define contact geometry for layer switching: `scalefactor`, and `contact` lines with 7 or 9 fields. The wiring tool buttons SHALL be:
- left: type
- right: leg
- middle: switch

(src: commands/CmdTZ.c:1689-2200, wiring/wireOps.c:150-260,1076-1127, wiring/wireTech.c:92-180)

#### Scenario: Switch layers
- **GIVEN** a metal1 wire in progress
- **WHEN** `wire switch metal2` is run
- **THEN** a via SHALL be placed at the wire end, and the wire SHALL continue in metal2

#### Scenario: Bad wiring contact line
- **WHEN** a `contact` line in the wiring section does not have 7 or 9 arguments
- **THEN** the loader SHALL report "\"contact\" lines must have exactly 7 or 9 arguments."
